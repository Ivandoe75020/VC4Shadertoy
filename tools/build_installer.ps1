$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$release = Join-Path $root "bin\x64\Release"
$executable = Join-Path $release "VideoConverterForShadertoy.exe"
$dist = Join-Path $root "dist"
$makensis = "C:\Program Files (x86)\NSIS\makensis.exe"

if (-not (Test-Path -LiteralPath $executable)) {
    throw "Release executable not found: $executable"
}
if (-not (Test-Path -LiteralPath $makensis)) {
    throw "NSIS compiler not found: $makensis"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null

& $makensis "/NOCD" "/DROOT=$root" (Join-Path $PSScriptRoot "installer.nsi")
if ($LASTEXITCODE -ne 0) {
    throw "NSIS compilation failed with exit code $LASTEXITCODE"
}

$installer = Join-Path $dist "VideoConverterForShadertoy-1.0.0-Setup.exe"
if (-not (Test-Path -LiteralPath $installer)) {
    throw "Installer was not created: $installer"
}

Write-Host "Installer created: $installer"
