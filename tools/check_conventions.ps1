$root = Split-Path -Parent $PSScriptRoot
$self = $MyInvocation.MyCommand.Path
$errors = New-Object System.Collections.Generic.List[string]
$host1 = 'git' + 'hub'

$files = Get-ChildItem -Path $root -Recurse -File | Where-Object {
    $_.FullName -ne $self -and $_.FullName -notmatch '\\(bin|obj|x64|Debug|Release|third_party|Generated Files)\\'
}

foreach ($f in $files) {
    $text = Get-Content -Path $f.FullName -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) { continue }
    $ext = $f.Extension.ToLower()
    if ($ext -in '.h', '.hpp', '.cpp', '.c', '.idl', '.rc', '.xaml', '.ps1', '.sh' -and $text -match "(?i)$host1") {
        $errors.Add("Forbidden reference in $($f.Name)")
    }
    if ($ext -in '.h', '.hpp', '.cpp', '.c', '.idl', '.rc') {
        if ($text -match '(?<!:)//' -or $text -match '/\*') { $errors.Add("Comment found in $($f.Name)") }
    }
    if ($ext -in '.xaml', '.xml', '.manifest', '.vcxproj', '.props', '.targets') {
        if ($text -match '<!--') { $errors.Add("Comment found in $($f.Name)") }
    }
    if ($ext -in '.ps1', '.sh') {
        if ($text -match '(?m)(^|\s)#') { $errors.Add("Comment found in $($f.Name)") }
    }
}

$required = @(
    @{ Path = 'src\AppInfo.h'; Texts = @('Video converter for Shadertoy', 'Asger Andersen') },
    @{ Path = 'src\VideoConverter.rc'; Texts = @('Video converter for Shadertoy', 'Asger Andersen') },
    @{ Path = 'app.manifest'; Texts = @('Video converter for Shadertoy', 'Asger Andersen') }
)
foreach ($r in $required) {
    $p = Join-Path $root $r.Path
    if (-not (Test-Path $p)) { $errors.Add("Missing file: $($r.Path)"); continue }
    $content = Get-Content -Path $p -Raw
    foreach ($t in $r.Texts) {
        if ($content -notmatch [regex]::Escape($t)) { $errors.Add("Missing text '$t' in $($r.Path)") }
    }
}

$app = Join-Path $root 'ui\App.xaml'
if ((Test-Path $app) -and ((Get-Content -Path $app -Raw) -match 'RequestedTheme')) {
    $errors.Add("RequestedTheme must not be set in App.xaml")
}

if ($errors.Count -eq 0) {
    Write-Host "All conventions are respected."
    exit 0
}
$errors | ForEach-Object { Write-Host $_ }
exit 1
