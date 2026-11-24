# scripts/bench_cross_standards_and_wsl.ps1
# Automated C++23 cross-platform (Windows MSVC vs WSL2 GCC/Clang) SBO benchmark orchestrator for CPP-BigInt

$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$rootDir = Split-Path -Parent $PSScriptRoot
$benchDir = Join-Path $rootDir "benchmarks"
$dataDir = Join-Path $benchDir "data"
$resultsDir = Join-Path $benchDir "results"
$binDir = Join-Path $rootDir "build\matrix_bench"

if (-not (Test-Path $resultsDir)) {
    New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
}
if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir -Force | Out-Null
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars64 = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -prerelease -latest -property installationPath 2>$null
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $candidate) { $vcvars64 = $candidate }
    }
}
if (-not $vcvars64) {
    $fallbackVc = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path $fallbackVc) { $vcvars64 = $fallbackVc }
}

$benchSource = Join-Path $benchDir "cpp\bench_layer3_overhead.cpp"
$includeDir1 = Join-Path $rootDir "include"
$includeDir2 = Join-Path $benchDir "include"

Write-Host "==========================================================================" -ForegroundColor Cyan
Write-Host "  CPP-BigInt: C++23 Cross-Platform SBO Benchmark Suite                    " -ForegroundColor Cyan
Write-Host "==========================================================================" -ForegroundColor Cyan

# -----------------------------------------------------------------------------
# Part 1: Windows MSVC (C++23 /std:c++latest)
# -----------------------------------------------------------------------------
if ($vcvars64 -and (Test-Path $vcvars64)) {
    Write-Host "`n>>> [Platform: Windows] Compiler: MSVC x64 (cl.exe /std:c++latest)" -ForegroundColor Green

    $exePath = Join-Path $binDir "bench_msvc_cpp23.exe"
    $outJson = Join-Path $resultsDir "results_sbo_msvc_C++latest.json"

    cmd.exe /c "`"$vcvars64`" >nul 2>&1 && cl.exe /nologo /O2 /Oi /Ot /Gy /EHsc /std:c++latest /utf-8 /Zc:__cplusplus /I`"$includeDir1`" /I`"$includeDir2`" `"$benchSource`" /Fe:`"$exePath`" >nul 2>&1"
    if (($LASTEXITCODE -eq 0) -and (Test-Path $exePath)) {
        Write-Host "  [MSVC Running] Executing benchmark (C++23 /std:c++latest) ..." -ForegroundColor Cyan
        & $exePath $dataDir $outJson | Out-Null
        Write-Host "  -> Generated: $outJson" -ForegroundColor Green
    } else {
        Write-Host "  -> [BUILD FAILED] for MSVC C++23" -ForegroundColor Red
    }
}

# -----------------------------------------------------------------------------
# Part 2: WSL2 Linux GCC (C++23)
# -----------------------------------------------------------------------------
$wslCheck = wsl.exe uname -a 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "`n>>> [Platform: WSL2 Linux] Compiler: GCC (-std=c++23)" -ForegroundColor Green

    $wslRoot = "/mnt/" + $rootDir.Substring(0, 1).ToLower() + $rootDir.Substring(2).Replace("\", "/")
    wsl.exe --cd $wslRoot mkdir -p build/matrix_bench
    wsl.exe --cd $wslRoot mkdir -p benchmarks/results

    $relExe = "build/matrix_bench/bench_gcc_cpp23"
    $outJson = "benchmarks/results/results_sbo_wsl_gcc_C++23.json"

    wsl.exe --cd $wslRoot g++ -std=c++23 -O2 -Iinclude -Ibenchmarks/include benchmarks/cpp/bench_layer3_overhead.cpp -o $relExe 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "  [WSL GCC Running] Executing benchmark (C++23) ..." -ForegroundColor Cyan
        wsl.exe --cd $wslRoot ./$relExe benchmarks/data $outJson | Out-Null
        Write-Host "  -> Generated: $outJson" -ForegroundColor Green
    } else {
        Write-Host "  -> [BUILD FAILED] for WSL GCC C++23" -ForegroundColor Red
    }
}

# -----------------------------------------------------------------------------
# Part 3: Python Analysis & Report Generation
# -----------------------------------------------------------------------------
Write-Host "`n>>> Aggregating Matrix & Generating Cross-Platform Report..." -ForegroundColor Cyan
$pythonScript = Join-Path $benchDir "scripts\analyze_sbo_and_cross_platform.py"
python.exe $pythonScript

Remove-Item $binDir -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $rootDir -Filter *.obj | Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "`n==========================================================================" -ForegroundColor Green
Write-Host "  Benchmark Matrix Execution & Report Generation Complete!                " -ForegroundColor Green
Write-Host "==========================================================================" -ForegroundColor Green
