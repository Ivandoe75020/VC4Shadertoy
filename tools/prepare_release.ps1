param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$releaseDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $releaseDirectory)) {
    throw "Release output directory not found: $releaseDirectory"
}

$keepDirectories = @(
    "assets",
    "en-us",
    "licenses",
    "Microsoft.UI.Xaml",
    "ui"
)

Get-ChildItem -LiteralPath $releaseDirectory -Directory -Force | ForEach-Object {
    if ($_.Name -notin $keepDirectories) {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
}

$removeFiles = @(
    "*.pdb",
    "*.lib",
    "*.exp",
    "*.winmd",
    "RestartAgent.exe",
    "WindowsAppRuntime.png",
    "Microsoft.Web.WebView2.Core.dll",
    "Microsoft.Web.WebView2.Core.winmd",
    "Microsoft.Windows.Widgets.dll",
    "Microsoft.Windows.Widgets.winmd"
)

foreach ($pattern in $removeFiles) {
    Get-ChildItem -LiteralPath $releaseDirectory -Filter $pattern -File -Force -ErrorAction SilentlyContinue |
        Remove-Item -Force
}
