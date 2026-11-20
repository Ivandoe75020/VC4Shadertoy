$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'Visual Studio 2022 with the C++ desktop workload is required.'
}

$vsPath = & $vswhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw 'The MSVC x64 build tools were not found in Visual Studio.'
}

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
    throw 'vcvars64.bat was not found.'
}

$msysRoot = $env:MSYS2_ROOT
if (-not $msysRoot) {
    $msysRoot = 'C:\msys64'
}

$bash = Join-Path $msysRoot 'usr\bin\bash.exe'
if (-not (Test-Path $bash)) {
    throw 'MSYS2 was not found. Install it, then run: pacman -S make diffutils nasm pkgconf'
}

$variables = cmd /c "`"$vcvars`" > nul && set"
foreach ($line in $variables) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}

$env:MSYS2_PATH_TYPE = 'inherit'
$env:CHERE_INVOKING = '1'
$env:MSYSTEM = 'MSYS'

$rootUnix = (& $bash -lc "cygpath -u '$root'").Trim()
& $bash -lc "bash '$rootUnix/tools/build_ffmpeg.sh'"
if ($LASTEXITCODE -ne 0) {
    throw 'The FFmpeg build failed.'
}

Write-Host 'FFmpeg was built and installed in third_party\ffmpeg\x64.'
