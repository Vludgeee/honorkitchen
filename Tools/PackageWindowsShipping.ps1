# Package MyProject (UE 5.3) Shipping build. Run from project root:
#   .\Tools\PackageWindowsShipping.ps1
#   .\Tools\PackageWindowsShipping.ps1 -EngineRoot "D:\path\to\UE_5.3"
# Use ASCII-only strings so Windows PowerShell 5.1 reads the file with any system code page.

param(
    [string]$EngineRoot = $env:UE_5_3_ROOT,
    [string]$ArchiveDir = "",
    [switch]$BuildInstaller
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $ProjectDir "MyProject.uproject"

if (-not (Test-Path $UProject)) {
    throw "Project not found: $UProject"
}

$candidates = @(
    $EngineRoot,
    $env:UNREAL_ENGINE_PATH,
    $env:UE_5_3_ROOT,
    "D:\vlads\Documents\UE\UE_5.3",
    "C:\Program Files\Epic Games\UE_5.3",
    "D:\Epic Games\UE_5.3",
    "C:\UE_5.3"
) | Where-Object { $_ -and (Test-Path $_) }

$RunUAT = $null
foreach ($root in $candidates) {
    $bat = Join-Path $root "Engine\Build\BatchFiles\RunUAT.bat"
    if (Test-Path $bat) { $RunUAT = $bat; $EngineRoot = $root; break }
}

if (-not $RunUAT) {
    throw @"
RunUAT.bat for UE 5.3 not found.
Set engine root, then run again:
  `$env:UE_5_3_ROOT = 'C:\Program Files\Epic Games\UE_5.3'
  .\Tools\PackageWindowsShipping.ps1
or:
  .\Tools\PackageWindowsShipping.ps1 -EngineRoot 'D:\full\path\to\UE_5.3'
"@
}

if (-not $ArchiveDir) {
    $ArchiveDir = Join-Path $ProjectDir "Build\Packaged"
}

New-Item -ItemType Directory -Force -Path $ArchiveDir | Out-Null

Write-Host "Engine: $EngineRoot"
Write-Host "Archive: $ArchiveDir"
Write-Host "Starting BuildCookRun (this can take a long time)..."

# Paths with spaces (e.g. "Unreal Projects") must be one quoted token for RunUAT/cmd.
$uatArgs = @(
    "BuildCookRun",
    "-project=`"$UProject`"",
    "-noP4",
    "-utf8output",
    "-platform=Win64",
    "-clientconfig=Shipping",
    "-serverconfig=Shipping",
    "-target=MyProject",
    "-build",
    "-cook",
    "-stage",
    "-pak",
    "-archive",
    "-archivedirectory=`"$ArchiveDir`""
)

$p = Start-Process -FilePath $RunUAT -ArgumentList $uatArgs -NoNewWindow -Wait -PassThru
if ($p.ExitCode -ne 0) {
    throw "BuildCookRun failed with exit code $($p.ExitCode)"
}

$windows = Join-Path $ArchiveDir "Windows"
$windowsNoEditor = Join-Path $ArchiveDir "WindowsNoEditor"
$exe = "MyProject.exe"
if (Test-Path (Join-Path $windows $exe)) {
    $gameFolder = $windows
} elseif (Test-Path (Join-Path $windowsNoEditor $exe)) {
    $gameFolder = $windowsNoEditor
} else {
    $gameFolder = $windowsNoEditor
    Write-Host "Warning: expected $exe under Windows or WindowsNoEditor; check $ArchiveDir"
}
Write-Host "Done. Game folder: $gameFolder"
Write-Host 'On another PC: install MSVC VC++ Redistributable x64 if needed, then run MyProject.exe from that folder.'

if ($BuildInstaller) {
    & (Join-Path $PSScriptRoot "BuildInstaller.ps1")
}
