param(
    [ValidatePattern('^\d{1,4}\.\d{1,4}\.\d{1,4}$')]
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$payload = Join-Path $root 'dist\RiotControl'
$executable = Join-Path $payload 'RiotControl.exe'
$branding = Join-Path $payload 'branding'
if (!(Test-Path -LiteralPath $executable) -or
    !(Test-Path -LiteralPath (Join-Path $branding 'RiotControl.ico')) -or
    !(Test-Path -LiteralPath (Join-Path $branding 'installer-sidebar.bmp'))) {
    throw 'Build the game first with .\build.ps1 -Installer.'
}

$tools = Join-Path $root '.tools'
$nsis = Join-Path $tools 'nsis-3.13'
$compiler = Join-Path $nsis 'makensis.exe'
if (!(Test-Path -LiteralPath $compiler)) {
    $archive = Join-Path $tools 'nsis-3.13.zip'
    $expectedHash = 'BA63DFFC4410EE89193E1CB5A41989991BD77C61068DA17E3156D136B7B0B3D8'
    if (!(Test-Path -LiteralPath $archive)) {
        New-Item -ItemType Directory -Force $tools | Out-Null
        $download = "$archive.download"
        try {
            Write-Host 'Downloading the portable NSIS compiler (no system-wide installation)...'
            Invoke-WebRequest 'https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip' -OutFile $download
            if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $expectedHash) {
                # SourceForge sometimes serves a landing page with a timed download link.
                $page = Get-Content -LiteralPath $download -Raw
                $link = [regex]::Match($page,
                    'https://downloads\.sourceforge\.net/project/nsis/NSIS%203/3\.13/nsis-3\.13\.zip\?ts=[^"<> ]+')
                if (!$link.Success) { throw 'SourceForge did not return the NSIS archive or a download link.' }
                Invoke-WebRequest ([Net.WebUtility]::HtmlDecode($link.Value)) -OutFile $download
            }
            if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ne $expectedHash) {
                throw 'Downloaded NSIS archive SHA-256 mismatch.'
            }
            Move-Item -LiteralPath $download -Destination $archive
        } finally {
            if (Test-Path -LiteralPath $download) { Remove-Item -LiteralPath $download }
        }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        throw "NSIS archive SHA-256 mismatch. Remove $archive and retry."
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $tools -Force
}

Copy-Item -LiteralPath (Join-Path $nsis 'COPYING') -Destination (Join-Path $payload 'licenses\NSIS-COPYING.txt') -Force
$files = @(Get-ChildItem -LiteralPath $payload -File -Recurse | Sort-Object FullName)
$build = Join-Path $root 'build\installer'
New-Item -ItemType Directory -Force $build | Out-Null
$manifest = Join-Path $build 'payload.nsh'
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('!macro InstallPayload')
foreach ($file in $files) {
    $relative = $file.FullName.Substring($payload.Length + 1)
    $directory = Split-Path $relative
    $lines.Add(('  SetOutPath "$INSTDIR\{0}"' -f $directory))
    $lines.Add(('  File "{0}"' -f $file.FullName))
}
$lines.Add('!macroend')
$lines.Add('!macro UninstallPayload')
foreach ($file in $files) {
    $relative = $file.FullName.Substring($payload.Length + 1)
    $lines.Add(('  Delete "$INSTDIR\{0}"' -f $relative))
}
$lines.Add('!macroend')
$lines.Add('!macro RemovePayloadDirectories')
# Remove only empty directories and the exact shipped files, never a recursive delete.
foreach ($directory in (Get-ChildItem -LiteralPath $payload -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending)) {
    $relative = $directory.FullName.Substring($payload.Length + 1)
    $lines.Add(('  RMDir "$INSTDIR\{0}"' -f $relative))
}
$lines.Add('!macroend')
[IO.File]::WriteAllLines($manifest, $lines, [Text.UTF8Encoding]::new($false))

$name = "RiotControl-Setup-$Version.exe"
$output = Join-Path $build $name
$sizeKB = [int][Math]::Ceiling(($files | Measure-Object Length -Sum).Sum / 1KB)
& $compiler /V2 /WX /NOCONFIG /INPUTCHARSET UTF8 "/DRIOT_VERSION=$Version" "/DOUTPUT_FILE=$output" `
    "/DPAYLOAD_MANIFEST=$manifest" "/DINSTALL_SIZE=$sizeKB" "/DBRANDING_DIR=$branding" (Join-Path $PSScriptRoot 'installer.nsi')
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed (exit $LASTEXITCODE)." }
$destination = Join-Path $root "dist\$name"
Copy-Item -LiteralPath $output -Destination $destination -Force
Write-Host ('Installer ready: {0} ({1:N1} MB)' -f $destination, ((Get-Item -LiteralPath $destination).Length / 1MB))
