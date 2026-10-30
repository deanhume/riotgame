# Launches the real game window and drives it with synthetic mouse and keyboard input.
#
#   .\tools\smoke_test.ps1             title > map > level 1 (full screen if configured), a wave with chatter, pause,
#                                      F11 round trip, quit to the map; then level 5 to the finale with the shop and
#                                      wardrobe, a restart that reloads the save, lose/retry and Esc stepping back to
#                                      quit, and the short start text in a small window
#   .\tools\smoke_test.ps1 -Windowed   the same, but the first scenario starts windowed
#   .\tools\smoke_test.ps1 -Tour       also capture every level's intro card, street and a wave in progress, and the
#                                      wardrobe and shop with everything unlocked
#   .\tools\smoke_test.ps1 -Natural    also lose level 1 for real (no debug keys): builds nothing and waits for the
#                                      crowd to break through, which takes a minute or two
#
# Leave the keyboard and mouse alone while it runs: the game window takes focus, so real input would reach it.
#
# Frames are saved to .tools\smoke (scaled down to at most 1600 px wide). The script checks that each screen change
# really happens and that the game stays responsive; the pictures are for you to look at.
param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\dist\RiotControl\RiotControl.exe'),
    [string]$Output = (Join-Path $PSScriptRoot '..\.tools\smoke'),
    [switch]$Windowed,
    [switch]$Tour,
    [switch]$Natural
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class RiotWindow {
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint key, uint mapType);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
}
'@

# Work in physical pixels so window bounds, captures, and posted clicks agree on high-DPI displays.
$null = [RiotWindow]::SetThreadDpiAwarenessContext([IntPtr](-4))

$Executable = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force $Output | Out-Null
$Output = (Resolve-Path $Output).Path
$config = Get-Content (Join-Path $PSScriptRoot '..\Source\GameConfig.h') -Raw
$expectFullscreen = ($config -match 'startFullscreen\s*=\s*true') -and !$Windowed

$script:process = $null
$script:window = [IntPtr]::Zero

function Start-Game([string[]]$Arguments) {
    $startArguments = @{ FilePath = $Executable; WorkingDirectory = (Split-Path $Executable); PassThru = $true }
    if ($Arguments.Count -gt 0) { $startArguments.ArgumentList = $Arguments }
    $script:process = Start-Process @startArguments
    if (!$script:process.WaitForInputIdle(15000)) { throw 'Game did not become input-ready.' }
    $width = 0
    $height = 0
    for ($attempt = 0; $attempt -lt 100 -and ($width -le 0 -or $height -le 0); ++$attempt) {
        Start-Sleep -Milliseconds 100
        $script:process.Refresh()
        if ($script:process.HasExited) { throw "Game exited during startup ($($script:process.ExitCode))." }
        $handle = $script:process.MainWindowHandle
        $rect = New-Object RiotWindow+Rect
        if ($handle -ne [IntPtr]::Zero -and [RiotWindow]::GetClientRect($handle, [ref]$rect)) {
            $script:window = $handle
            $width = $rect.Right
            $height = $rect.Bottom
        }
    }
    if ($script:window -eq [IntPtr]::Zero -or $width -le 0 -or !$script:process.Responding) {
        throw 'Game has no responsive native window.'
    }
    Start-Sleep -Seconds 2
}

# A full screen window minimises itself when it loses focus (as in any full screen game), so bring it back.
function Restore-Window {
    if ([RiotWindow]::IsIconic($script:window)) {
        $null = [RiotWindow]::ShowWindow($script:window, 9)
        Start-Sleep -Milliseconds 800
    }
}

function Get-ClientSize {
    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        Restore-Window
        $rect = New-Object RiotWindow+Rect
        if ([RiotWindow]::GetClientRect($script:window, [ref]$rect) -and $rect.Right -gt 0 -and $rect.Bottom -gt 0) {
            return [pscustomobject]@{ Width = $rect.Right; Height = $rect.Bottom }
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Cannot read game window bounds.'
}

function Set-ClientSize([int]$width, [int]$height) {
    Restore-Window
    $outer = New-Object RiotWindow+Rect
    $inner = New-Object RiotWindow+Rect
    $null = [RiotWindow]::GetWindowRect($script:window, [ref]$outer)
    $null = [RiotWindow]::GetClientRect($script:window, [ref]$inner)
    $extraWidth = ($outer.Right - $outer.Left) - $inner.Right
    $extraHeight = ($outer.Bottom - $outer.Top) - $inner.Bottom
    # SWP_NOZORDER | SWP_NOACTIVATE
    $null = [RiotWindow]::SetWindowPos($script:window, [IntPtr]::Zero, $outer.Left, $outer.Top,
        $width + $extraWidth, $height + $extraHeight, 0x14)
    Start-Sleep -Milliseconds 1000
}

function Test-Fullscreen {
    Restore-Window
    $rect = New-Object RiotWindow+Rect
    $null = [RiotWindow]::GetWindowRect($script:window, [ref]$rect)
    $rect.Left -eq 0 -and $rect.Top -eq 0 -and
        $rect.Right -eq [RiotWindow]::GetSystemMetrics(0) -and $rect.Bottom -eq [RiotWindow]::GetSystemMetrics(1)
}

function Wait-Fullscreen([bool]$expected) {
    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        if ((Test-Fullscreen) -eq $expected) { Start-Sleep -Milliseconds 500; return }
        Start-Sleep -Milliseconds 100
    }
    throw "Game window is not $(if ($expected) { 'full screen' } else { 'windowed' }) as expected."
}

# A coarse fingerprint of a frame, so a screen change can be checked without comparing whole images.
function Get-Signature([System.Drawing.Bitmap]$image) {
    $columns = 64
    $rows = 40
    $samples = New-Object 'int[]' ($columns * $rows)
    for ($row = 0; $row -lt $rows; ++$row) {
        for ($column = 0; $column -lt $columns; ++$column) {
            $x = [int](($column + 0.5) * $image.Width / $columns)
            $y = [int](($row + 0.5) * $image.Height / $rows)
            $samples[$row * $columns + $column] = $image.GetPixel($x, $y).ToArgb()
        }
    }
    , $samples
}

# Saves the window contents as <name>.png and returns the frame's signature.
function Save-Frame([string]$name) {
    Start-Sleep -Milliseconds 300
    $size = Get-ClientSize
    $image = New-Object System.Drawing.Bitmap($size.Width, $size.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($image)
    $dc = $graphics.GetHdc()
    try {
        if (![RiotWindow]::PrintWindow($script:window, $dc, 3)) { throw 'Window capture failed.' }
    } finally {
        $graphics.ReleaseHdc($dc)
        $graphics.Dispose()
    }
    try {
        $signature = Get-Signature $image
        $saved = $image
        if ($image.Width -gt 1600) {
            $saved = New-Object System.Drawing.Bitmap($image, 1600, [int]($image.Height * 1600 / $image.Width))
        }
        try { $saved.Save((Join-Path $Output "$name.png")) } finally { if ($saved -ne $image) { $saved.Dispose() } }
    } finally {
        $image.Dispose()
    }
    , $signature
}

function Get-Difference($before, $after) {
    $changed = 0
    for ($i = 0; $i -lt $before.Length; ++$i) {
        $p = $before[$i]
        $q = $after[$i]
        $delta = [Math]::Abs((($p -shr 16) -band 255) - (($q -shr 16) -band 255)) +
            [Math]::Abs((($p -shr 8) -band 255) - (($q -shr 8) -band 255)) +
            [Math]::Abs(($p -band 255) - ($q -band 255))
        if ($delta -gt 60) { ++$changed }
    }
    $changed / $before.Length
}

function Assert-Changed($before, $after, [string]$what) {
    $difference = Get-Difference $before $after
    if ($difference -lt 0.05) {
        throw "$what (only $([Math]::Round($difference * 100, 1))% of the frame changed)."
    }
}

function Assert-Alive([string]$context) {
    $script:process.Refresh()
    if ($script:process.HasExited -or !$script:process.Responding) { throw "Game stopped responding ($context)." }
}

# The 1280x800 design is letterboxed in the client area, so map through the viewport.
function Click-Game([int]$x, [int]$y) {
    $size = Get-ClientSize
    $scale = [Math]::Min($size.Width / 1280.0, $size.Height / 800.0)
    $clientX = [int](($size.Width - 1280 * $scale) / 2 + $x * $scale)
    $clientY = [int](($size.Height - 800 * $scale) / 2 + (800 - $y) * $scale)
    $position = [IntPtr](($clientY -shl 16) -bor $clientX)
    $null = [RiotWindow]::PostMessage($script:window, 0x200, [IntPtr]::Zero, $position)
    Start-Sleep -Milliseconds 60
    $null = [RiotWindow]::PostMessage($script:window, 0x201, [IntPtr]1, $position)
    $null = [RiotWindow]::PostMessage($script:window, 0x202, [IntPtr]::Zero, $position)
    Start-Sleep -Milliseconds 100
}

function Press-Key([int]$code) {
    $flags = ([RiotWindow]::MapVirtualKey($code, 0) -shl 16) -bor 1
    $null = [RiotWindow]::PostMessage($script:window, 0x100, [IntPtr]$code, [IntPtr]$flags)
    $null = [RiotWindow]::PostMessage($script:window, 0x101, [IntPtr]$code, [IntPtr]($flags -bor 0xC0000000))
    Start-Sleep -Milliseconds 100
}

# Button centres on the 1280x800 design.
$titleButton = @(640, 139)      # START (title) / BACK TO MAP (end screen)
$mapLevel1 = @(300, 505)        # the level 1 tile on the map
$mapShop = @(300, 126)
$mapWardrobe = @(640, 126)
$introButton = @(540, 231)      # START LEVEL
$introMap = @(210, 231)         # < MAP
$reportButton = @(840, 171)     # NEXT LEVEL / UPGRADES / TRY AGAIN / FINISH
$reportMap = @(361, 171)        # < MAP
$pauseQuit = @(775, 346)        # QUIT TO MAP on the pause box
$shopBuy1 = @(1005, 558)        # BUY on the first upgrade row
$shopPrimary = @(905, 116)      # NEXT LEVEL / BACK TO MAP
$wardrobeBack = @(640, 126)
function Get-HatTile([int]$index) { @((560 + 128 * $index), 505) }
function Get-ConeTile([int]$index) { @((560 + 128 * $index), 325) }

function Invoke-Scenario([string]$Name, [string[]]$Arguments, [scriptblock]$Steps) {
    Write-Host "-- $Name"
    $script:process = $null
    try {
        Start-Game $Arguments
        & $Steps
    } finally {
        if ($null -ne $script:process -and !$script:process.HasExited) {
            $null = $script:process.CloseMainWindow()
            if (!$script:process.WaitForExit(5000)) { Stop-Process -Id $script:process.Id }
        }
    }
    if ($script:process.ExitCode -ne 0) { throw "${Name}: game exited with code $($script:process.ExitCode)." }
}

# Each scenario saves progress to its own file under the output folder, never to the player's real save.
function Get-SavePath([string]$name) {
    $path = Join-Path $Output "$name.save.txt"
    Remove-Item $path -ErrorAction SilentlyContinue
    $path
}

$playSave = Get-SavePath 'play'
$playArguments = @('--save', $playSave) + $(if ($Windowed) { @('--windowed') } else { @() })
Invoke-Scenario 'play level 1 from the map' $playArguments {
    $mode = if ($expectFullscreen) { 'full screen' } else { 'windowed' }
    if ((Test-Fullscreen) -ne $expectFullscreen) { throw "Game did not start $mode as configured." }
    $title = Save-Frame 'title'
    Click-Game @titleButton
    $map = Save-Frame 'map'
    Assert-Changed $title $map 'START did not open the map'
    Click-Game @mapLevel1
    $intro = Save-Frame 'intro1'
    Assert-Changed $map $intro 'Clicking level 1 on the map did not open its intro card'
    Click-Game @introButton
    $prep = Save-Frame 'street1'
    Assert-Changed $intro $prep 'START LEVEL did not begin the level'
    Click-Game 596 444
    Click-Game 596 340
    Click-Game 596 236
    Click-Game 130 110
    Click-Game 362 444
    $null = Save-Frame 'prep'
    Press-Key 32
    foreach ($second in 6, 9, 12, 15) {
        Start-Sleep -Seconds 3
        $null = Save-Frame "wave_$second"
    }
    Press-Key 80
    $paused = Save-Frame 'paused'
    Press-Key 0x7A
    Wait-Fullscreen (!$expectFullscreen)
    $null = Save-Frame 'toggled'
    Press-Key 0x7A
    Wait-Fullscreen $expectFullscreen
    $null = Save-Frame 'restored'
    Click-Game @pauseQuit
    $back = Save-Frame 'map_after_quit'
    Assert-Changed $paused $back 'QUIT TO MAP did not leave the level'
    Assert-Alive 'gameplay'
    Write-Host "   started $mode, F11 toggles both ways, pause quits to the map"
}

$campaignSave = Get-SavePath 'campaign'
Invoke-Scenario 'campaign: level 5 to the finale, shop and wardrobe (--level 5 --debug)' @('--windowed', '--level', '5', '--debug', '--save', $campaignSave) {
    $intro = Save-Frame 'intro5'
    Press-Key 13     # Enter: START LEVEL
    $street = Save-Frame 'street5'
    Assert-Changed $intro $street 'Enter did not begin level 5'
    Press-Key 0x77   # F8: clear the level
    Start-Sleep -Milliseconds 1800
    $report = Save-Frame 'won5'
    Assert-Changed $street $report 'F8 did not show the level report'
    Press-Key 32     # Space: UPGRADES (stars were earned, so points are there to spend)
    $shop = Save-Frame 'shop_after_win'
    Assert-Changed $report $shop 'Space did not open the upgrade shop after a win'
    Click-Game @shopBuy1
    $bought = Save-Frame 'shop_bought'
    if ((Get-Difference $shop $bought) -le 0) { throw 'BUY changed nothing in the shop.' }
    Click-Game @shopPrimary
    $next = Save-Frame 'intro6'
    Assert-Changed $bought $next 'NEXT LEVEL in the shop did not open the level 6 intro card'
    Press-Key 13
    $street6 = Save-Frame 'street6'
    Assert-Changed $next $street6 'Enter did not begin level 6'
    Press-Key 0x77
    Start-Sleep -Milliseconds 1800
    $report6 = Save-Frame 'won6'
    Assert-Changed $street6 $report6 'F8 did not show the level 6 report'
    Press-Key 32     # FINISH: the last level goes straight to the end screen
    Start-Sleep -Milliseconds 1500
    $finished = Save-Frame 'finished'
    Assert-Changed $report6 $finished 'Space did not show the end screen'
    Press-Key 13     # BACK TO MAP
    $map = Save-Frame 'map_after_finale'
    Assert-Changed $finished $map 'Enter did not return to the map'
    Press-Key 0x57   # W: wardrobe
    $wardrobe = Save-Frame 'wardrobe'
    Assert-Changed $map $wardrobe 'W did not open the wardrobe'
    $partyHat = Get-HatTile 3   # unlocked by clearing level 6
    Click-Game @partyHat
    $party = Save-Frame 'wardrobe_party'
    if ((Get-Difference $wardrobe $party) -le 0) { throw 'Clicking the party hat changed nothing.' }
    Click-Game @wardrobeBack
    $null = Save-Frame 'map_party'
    Press-Key 0x1B   # Esc: map to title
    $null = Save-Frame 'title_party'
    Press-Key 0x1B   # Esc on the title quits
    if (!$script:process.WaitForExit(4000)) { throw 'Esc on the title did not quit.' }
    $saved = Get-Content $campaignSave -Raw
    if ($saved -notmatch 'hat=party' -or $saved -notmatch 'upgrade\.barriers=1' -or $saved -notmatch 'stars=0,0,0,0,[1-3],[1-3]') {
        throw "Progress was not saved as expected:`n$saved"
    }
    Write-Host '   stars, upgrade and hat saved'
}

Invoke-Scenario 'progress survives a restart' @('--windowed', '--save', $campaignSave) {
    $title = Save-Frame 'reload_title'
    Click-Game @titleButton
    $map = Save-Frame 'reload_map'
    Assert-Changed $title $map 'START did not open the map'
    Click-Game @mapShop
    $shop = Save-Frame 'reload_shop'
    Assert-Changed $map $shop 'The shop button did not open the shop'
    Press-Key 0x1B
    $back = Save-Frame 'reload_map_again'
    Assert-Changed $shop $back 'Esc did not leave the shop'
    Assert-Alive 'reload'
}

$loseSave = Get-SavePath 'lose'
Invoke-Scenario 'lose, retry and back out, mouse and Esc (--level 2 --debug)' @('--windowed', '--level', '2', '--debug', '--save', $loseSave) {
    $intro = Save-Frame 'intro2'
    Click-Game @introButton
    $street = Save-Frame 'street2'
    Assert-Changed $intro $street 'START LEVEL did not begin level 2'
    Press-Key 0x78   # F9: fail the level
    $lost = Save-Frame 'lost2'
    Assert-Changed $street $lost 'F9 did not show the level report'
    Click-Game @reportButton
    $retry = Save-Frame 'retry2'
    Assert-Changed $lost $retry 'TRY AGAIN did not restart the level'
    Press-Key 0x78
    $again = Save-Frame 'lost2_again'
    Press-Key 0x1B
    $map = Save-Frame 'map_after_loss'
    Assert-Changed $again $map 'Esc on the report did not go back to the map'
    Press-Key 0x1B
    $title = Save-Frame 'title_after_loss'
    Assert-Changed $map $title 'Esc on the map did not go back to the title'
    Press-Key 0x1B
    if (!$script:process.WaitForExit(4000)) { throw 'Esc on the title did not quit.' }
    Write-Host '   Esc steps back: report > map > title > quit'
}

Invoke-Scenario 'short start text (small window)' @('--windowed', '--save', (Get-SavePath 'small')) {
    $long = Save-Frame 'title_long'
    Set-ClientSize 800 500
    $short = Save-Frame 'title_short'
    Assert-Changed $long $short 'Shrinking the window did not switch to the short start text'
    Click-Game @titleButton
    $map = Save-Frame 'map_small'
    Assert-Changed $short $map 'START did not work in the small window'
    Assert-Alive 'small window'
}

if ($Tour) {
    foreach ($level in 1..6) {
        Invoke-Scenario "tour: level $level" @('--windowed', '--level', "$level", '--save', (Get-SavePath 'tour')) {
            $null = Save-Frame "tour_intro$level"
            Click-Game @introButton
            $null = Save-Frame "tour_street$level"
            Click-Game 596 444
            Click-Game 596 340
            Click-Game 596 236
            Press-Key 32
            Start-Sleep -Seconds 14
            $null = Save-Frame "tour_wave$level"
        }
    }
    # A save with every star and goal, so everything in the wardrobe is unlocked.
    $fullSave = Get-SavePath 'full'
    Set-Content -Path $fullSave -Value "stars=3,3,3,3,3,3`ngoals=3,3,3,3,3,3"
    Invoke-Scenario 'tour: wardrobe and shop with everything unlocked' @('--windowed', '--save', $fullSave) {
        Click-Game @titleButton
        $null = Save-Frame 'tour_map_full'
        Click-Game @mapWardrobe
        $crown = Get-HatTile 4
        $goldCone = Get-ConeTile 4
        Click-Game @crown
        Click-Game @goldCone
        $null = Save-Frame 'tour_wardrobe_full'
        Click-Game @wardrobeBack
        Click-Game @mapShop
        $null = Save-Frame 'tour_shop_full'
    }
}

if ($Natural) {
    Invoke-Scenario 'natural loss: level 1 with no defences' @('--windowed', '--level', '1', '--save', (Get-SavePath 'natural')) {
        Click-Game @introButton
        Start-Sleep -Milliseconds 500
        Press-Key 32     # READY with nothing built
        $previous = Save-Frame 'natural_wave'
        $lost = $null
        for ($waited = 0; $waited -lt 180 -and $null -eq $lost; $waited += 5) {
            Start-Sleep -Seconds 5
            $frame = Save-Frame 'natural_progress'
            if ((Get-Difference $previous $frame) -gt 0.3) { $lost = $frame }
            $previous = $frame
        }
        if ($null -eq $lost) { throw 'The crowd never broke through an undefended level 1.' }
        $null = Save-Frame 'natural_lost'
        Click-Game @reportButton
        $retry = Save-Frame 'natural_retry'
        Assert-Changed $lost $retry 'TRY AGAIN did not restart the level'
        Write-Host "   the crowd broke through after about $waited seconds"
    }
}

Write-Host "Smoke test passed; frames saved in $Output"