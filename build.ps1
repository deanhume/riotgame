param(
    [switch]$Run,
    [switch]$ModelOnly,
    [switch]$Installer
)

$ErrorActionPreference = 'Stop'
if ($ModelOnly -and $Installer) { throw '-Installer cannot be combined with -ModelOnly.' }
$root = $PSScriptRoot
$tools = Join-Path $root '.tools'
$engine = Join-Path $tools 'axmol-2.11.5'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) {
    throw 'Install Visual Studio with Desktop development with C++ and C++ CMake tools.'
}
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'The Visual Studio C++ workload is missing.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
if (!(Test-Path $cmake) -or !(Test-Path $ninja)) {
    throw 'Install the C++ CMake tools component through Visual Studio Installer.'
}

if (!$ModelOnly) {
    if (!(Test-Path (Join-Path $engine 'core\axmol.h'))) {
        New-Item -ItemType Directory -Force $tools | Out-Null
        $archive = Join-Path $tools 'axmol-2.11.5.zip'
        if (!(Test-Path $archive)) {
            Invoke-WebRequest 'https://github.com/axmolengine/axmol/releases/download/v2.11.5/axmol-2.11.5.zip' -OutFile $archive
        }
        if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '425bf1b2661b4488f1d62f24d2e7b8f0f5ce2154891820ce84c05219a5c0c0ee') {
            throw 'Axmol archive SHA256 mismatch. Remove .tools\axmol-2.11.5.zip and retry.'
        }
        Expand-Archive -LiteralPath $archive -DestinationPath $tools -Force
    }
    $shaderTools = Join-Path $engine 'tools\external\axslcc'
    if (!(Test-Path (Join-Path $shaderTools 'axslcc.exe'))) {
        $archive = Join-Path $tools 'axslcc-1.14.0-win64.zip'
        Invoke-WebRequest 'https://github.com/axmolengine/axslcc/releases/download/v1.14.0/axslcc-1.14.0-win64.zip' -OutFile $archive
        New-Item -ItemType Directory -Force $shaderTools | Out-Null
        Expand-Archive -LiteralPath $archive -DestinationPath $shaderTools -Force
    }
}

$build = Join-Path $root $(if ($ModelOnly) { 'build\model' } else { 'build\windows' })
$modelFlag = if ($ModelOnly) { 'ON' } else { 'OFF' }
$command = 'set "PATH={7};%PATH%" && call "{0}" >nul && "{1}" -S "{2}" -B "{3}" -G Ninja -DCMAKE_MAKE_PROGRAM="{4}" -DCMAKE_BUILD_TYPE=Release -DRIOT_MODEL_ONLY={5} && "{1}" --build "{3}" --parallel 6 && "{6}" --test-dir "{3}" --output-on-failure' -f `
    $vcvars, $cmake, $root, $build, $ninja, $modelFlag, (Join-Path (Split-Path $cmake) 'ctest.exe'), (Split-Path $vswhere)
& $env:ComSpec /d /c $command
if ($LASTEXITCODE -ne 0) { throw "Build or gameplay checks failed (exit $LASTEXITCODE)." }

if (!$ModelOnly) {
    $output = Join-Path $build 'bin\RiotControl'
    $destination = Join-Path $root 'dist\RiotControl'
    New-Item -ItemType Directory -Force $destination | Out-Null
    Copy-Item (Join-Path $output '*') $destination -Recurse -Force

    $redistVersion = Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt') |
        Select-Object -First 1
    $redistRoot = Join-Path $vs "VC\Redist\MSVC\$($redistVersion.Trim())\x64"
    $crt = Get-ChildItem $redistRoot -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
    if (!$crt) { throw 'The x64 Visual C++ redistributable files are missing.' }
    Copy-Item (Join-Path $crt.FullName '*.dll') $destination -Force

    $licenses = Join-Path $destination 'licenses'
    New-Item -ItemType Directory -Force $licenses | Out-Null
    Copy-Item (Join-Path $engine 'LICENSE') (Join-Path $licenses 'Axmol-LICENSE.txt') -Force
    Copy-Item (Join-Path $engine '3rdparty\README.md') (Join-Path $licenses 'Axmol-third-party-index.md') -Force
    $thirdParty = Join-Path $engine '3rdparty'
    Get-ChildItem $thirdParty -Recurse -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE|COPYRIGHT)([._-].*)?$' } |
        ForEach-Object {
            $relative = $_.FullName.Substring($thirdParty.Length + 1)
            $target = Join-Path $licenses $relative
            New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
            Copy-Item $_.FullName $target -Force
        }
    Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.txt') $destination -Force
    Copy-Item (Join-Path $root 'README.md') $destination -Force
    Write-Host "Game ready: $destination\RiotControl.exe"
    if ($Installer) {
        & (Join-Path $root 'tools\package_installer.ps1')
    }
    if ($Run) {
        Start-Process -FilePath (Join-Path $destination 'RiotControl.exe') -WorkingDirectory $destination
    }
}
