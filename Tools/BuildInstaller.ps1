# Builds HonorKitchen_Setup.exe (Inno Setup 6).
# Packaged game: Build\Packaged\Windows (Editor) or Build\Packaged\WindowsNoEditor (UAT).

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$ProjectDir = Split-Path -Parent $ToolsDir
$PackagedRoot = Join-Path $ProjectDir "Build\Packaged"
$ExeName = "MyProject.exe"

$windows = Join-Path $PackagedRoot "Windows"
$windowsNoEditor = Join-Path $PackagedRoot "WindowsNoEditor"

if (Test-Path (Join-Path $windows $ExeName)) {
    $GameDir = $windows
} elseif (Test-Path (Join-Path $windowsNoEditor $ExeName)) {
    $GameDir = $windowsNoEditor
} else {
    throw @"
Packaged game not found. Need $ExeName in one of:
  $windows
  $windowsNoEditor
Package in UE: File -> Package Project -> Windows (Shipping), output under Build\Packaged.
Or run: .\Tools\PackageWindowsShipping.ps1
"@
}

$Iss = Join-Path $ToolsDir "MyProject_Setup.iss"

$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    throw "Inno Setup 6 not found (ISCC.exe). Install from https://jrsoftware.org/isinfo.php"
}

Write-Host "Using packaged folder: $GameDir"
Write-Host "ISCC: $iscc"
& $iscc $Iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE" }

$out = Join-Path $ProjectDir "Build\Installer\HonorKitchen_Setup.exe"
Write-Host "Done: $out"
Write-Host "Ship HonorKitchen_Setup.exe to players. They may need VC++ Redistributable x64."
