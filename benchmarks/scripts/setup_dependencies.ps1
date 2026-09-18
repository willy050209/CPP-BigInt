# setup_dependencies.ps1
# Automates or guides the setup of external benchmark dependencies (Boost & GMP)

param(
    [switch]$InstallVcpkgBoost = $false
)

$RootDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$DepsDir = Join-Path $RootDir "benchmarks\deps"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  CPP-BigInt Benchmark Dependencies Setup" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Check/Install GMP (MPIR)
$MpirInclude = Join-Path $DepsDir "mpir\include\gmp.h"
$MpirLib = Join-Path $DepsDir "mpir\lib\mpir.lib"

if ((Test-Path $MpirInclude) -and (Test-Path $MpirLib)) {
    Write-Host "[GMP/MPIR] Pre-installed and ready in $DepsDir\mpir" -ForegroundColor Green
} else {
    Write-Host "[GMP/MPIR] Downloading MPIR x64 package from NuGet..." -ForegroundColor Yellow
    $url = "https://api.nuget.org/v3-flatcontainer/mpir-vc140-x64/2.7.2.1/mpir-vc140-x64.2.7.2.1.nupkg"
    $tmpZip = Join-Path $env:TEMP "mpir.zip"
    Invoke-WebRequest -Uri $url -OutFile $tmpZip
    $mpirTarget = Join-Path $DepsDir "mpir"
    Expand-Archive -Path $tmpZip -DestinationPath $mpirTarget -Force
    Remove-Item $tmpZip -Force
    Write-Host "[GMP/MPIR] Successfully installed to $mpirTarget" -ForegroundColor Green
}

# 2. Check Boost
$VcpkgDir = "D:\vcpkg"
$BoostHeader = Join-Path $VcpkgDir "installed\x64-windows\include\boost\multiprecision\cpp_int.hpp"

if (Test-Path $BoostHeader) {
    Write-Host "[Boost] Boost.Multiprecision found in $BoostHeader" -ForegroundColor Green
} else {
    Write-Host "[Boost] Optional: To install Boost.Multiprecision via vcpkg:" -ForegroundColor Yellow
    Write-Host "  1. cd D:\vcpkg" -ForegroundColor Gray
    Write-Host "  2. .\bootstrap-vcpkg.bat" -ForegroundColor Gray
    Write-Host "  3. .\vcpkg.exe install boost-multiprecision:x64-windows" -ForegroundColor Gray
    Write-Host "  4. Re-run CMake to enable bench_boost target." -ForegroundColor Gray
}

Write-Host "`nSetup check complete." -ForegroundColor Cyan
