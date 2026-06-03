#Requires -Version 5.1
<#
.SYNOPSIS
  HonorKitchen 0.3.0 — собрать установщик из уже упакованной игры.

.DESCRIPTION
  1) Ищет Build\Packaged\Windows или WindowsNoEditor (MyProject.exe).
  2) Запускает Tools\BuildInstaller.ps1 -> Dist\HonorKitchen_Demo_Setup_<version>.exe

  Полный цикл (долго):  .\Tools\BuildHonorKitchenInstaller.ps1 -PackageFirst

.EXAMPLE
  .\Tools\BuildHonorKitchenInstaller.ps1
  .\Tools\BuildHonorKitchenInstaller.ps1 -PackageFirst
#>
param(
    [switch] $PackageFirst,
    [string] $EngineRoot = $env:UE_5_3_ROOT
)

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root

if ($PackageFirst) {
    Write-Host "=== Package Shipping (UE) ===" -ForegroundColor Cyan
    $pkgArgs = @()
    if ($EngineRoot) { $pkgArgs += "-EngineRoot", $EngineRoot }
    & (Join-Path $PSScriptRoot "PackageWindowsShipping.ps1") @pkgArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "=== Inno Setup installer ===" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "BuildInstaller.ps1")
exit $LASTEXITCODE
