$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$path = Join-Path $PSScriptRoot '..\tools\smoke_test.ps1'
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }

# Load the real helpers without executing the desktop scenarios or requiring a built game.
$names = @('Wait-Until', 'Wait-State', 'Test-SaveFrame', 'Invoke-Scenario')
foreach ($name in $names) {
    $definition = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if (!$definition) { throw "Missing helper: $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}

function Check([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}

function Expect-Timeout([scriptblock]$Action) {
    try { & $Action }
    catch {
        if ($_.Exception.Message -like 'Timed out:*') { return }
        throw
    }
    throw 'Expected a timeout, not a successful wait.'
}

function Assert-Alive([string]$Context) {
    $script:polls++
    if ($script:exitGame) { throw "Game stopped responding ($Context)." }
    if ($script:nextState -and $script:polls -ge 2) {
        [System.IO.File]::WriteAllText($script:statePath, $script:nextState)
    }
}

$script:statePath = [System.IO.Path]::GetTempFileName()
$script:polls = 0
$script:exitGame = $false
$script:nextState = ''
try {
    [System.IO.File]::WriteAllText($script:statePath, "phase=DefendIntro`npaused=0`n")
    $script:nextState = "phase=Defend`r`npaused=1`r`n"
    Wait-State 'Defend' 1 1000
    Check ($script:polls -ge 2) 'State waits must poll until the requested phase and pause state arrive.'
    $script:nextState = ''
    Expect-Timeout { Wait-State 'Defend' 0 70 }
    Expect-Timeout { Wait-State 'DefendWon' -1 70 }
    [System.IO.File]::WriteAllText($script:statePath, 'phase=')
    Expect-Timeout { Wait-State '' -1 70 }
    Remove-Item -LiteralPath $script:statePath
    Expect-Timeout { Wait-State '' -1 70 }

    $script:exitGame = $true
    $detectedExit = $false
    try { Wait-State 'Title' -1 70 }
    catch {
        if ($_.Exception.Message -notlike 'Game stopped responding*') { throw }
        $detectedExit = $true
    }
    Check $detectedExit 'A game exit must fail immediately rather than being hidden as a timeout.'
    $script:exitGame = $false

    $Screenshots = 'Failures'
    Check (!(Test-SaveFrame 'title' $false)) 'Failures-only mode must not save successful frames.'
    Check (Test-SaveFrame 'failure_Defend' $true) 'Failure captures must override the screenshot policy.'
    $Screenshots = 'Key'
    Check (Test-SaveFrame 'finished' $false) 'Key mode must save important screens.'
    Check (Test-SaveFrame 'tour_intro1' $false) 'Explicit tours must retain their captures.'
    Check (!(Test-SaveFrame 'transition' $false)) 'Polling must not write intermediate PNGs.'
    Check (!(Test-SaveFrame 'shop_bought' $false)) 'Key mode must skip intermediate successful frames.'
    $Screenshots = 'All'
    Check (Test-SaveFrame 'shop_bought' $false) 'All mode must retain intermediate captures.'

    $Scenario = @('Defend')
    Invoke-Scenario 'Campaign' 'skipped campaign' @() { throw 'An unselected scenario ran.' }
    Invoke-Scenario 'Riot' 'skipped riot' @() { throw 'An unselected scenario ran.' }

    $defendScenario = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.CommandAst] -and
        $node.GetCommandName() -eq 'Invoke-Scenario' -and $node.CommandElements[1].Value -eq 'Defend'
    }, $true)
    if (!$defendScenario) { throw 'Missing defend scenario.' }
    $steps = $defendScenario.CommandElements[-1].ScriptBlock.GetScriptBlock()
    function Press-Key([int]$code, [int]$HoldMilliseconds = 0) {
        $script:keys.Add($code)
        $script:holds.Add($HoldMilliseconds)
    }
    function Wait-State { }
    function Save-Frame { , @(0) }
    function Assert-Changed { }
    function Assert-Alive { }
    $script:keys = [System.Collections.Generic.List[int]]::new()
    $script:holds = [System.Collections.Generic.List[int]]::new()
    $Quick = $true
    & $steps
    Check ($script:keys.Contains(0x25) -and $script:keys.Contains(32)) 'Quick mode must retain baton and shield input.'
    Check (!$script:holds.Contains(3200)) 'Quick mode must skip the real-time shield/barrage delay.'
    Check (($script:keys | Where-Object { $_ -eq 0x78 }).Count -eq 2) 'Quick mode must use debug loss for both retries.'
    $script:keys.Clear()
    $script:holds.Clear()
    $Quick = $false
    & $steps
    Check ($script:holds.Contains(3200)) 'The full run must retain its real shield-hold check.'
    Check (($script:keys | Where-Object { $_ -eq 0x78 }).Count -eq 1) 'The full run must wait for the natural second loss.'
    Write-Host 'Smoke helper checks passed (no game window launched).'
} finally {
    if (Test-Path -LiteralPath $script:statePath) { Remove-Item -LiteralPath $script:statePath }
}
