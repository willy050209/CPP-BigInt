# scripts/bench_cross_standards_and_wsl.ps1
# Automated cross-standard (C++14 ~ C++23) and cross-platform (Windows MSVC vs WSL2 GCC/Clang)
# SBO benchmark orchestrator for CPP-BigInt

param (
    [switch]$Quick
)

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
$vsPath = & $vswhere -prerelease -latest -property installationPath
if (-not $vsPath) {
    $vsPath = "C:\Program Files\Microsoft Visual Studio\18\Insiders"
}
$vcvars64 = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

$benchSource = Join-Path $benchDir "cpp\bench_layer3_overhead.cpp"
$includeDir1 = Join-Path $rootDir "include"
$includeDir2 = Join-Path $benchDir "include"

Write-Host "==========================================================================" -ForegroundColor Cyan
Write-Host "  CPP-BigInt: Cross-Standards & Cross-Platform SBO Benchmark Suite        " -ForegroundColor Cyan
Write-Host "==========================================================================" -ForegroundColor Cyan

# -----------------------------------------------------------------------------
# Part 1: Windows MSVC (C++14, C++17, C++20, C++latest)
# -----------------------------------------------------------------------------
if (Test-Path $vcvars64) {
    Write-Host "`n>>> [Platform: Windows 11] Compiler: MSVC x64 (cl.exe)" -ForegroundColor Green
    
    $msvcStandards = if ($Quick) { @("/std:c++20", "/std:c++latest") } else { @("/std:c++14", "/std:c++17", "/std:c++20", "/std:c++latest") }
    
    foreach ($std in $msvcStandards) {
        $stdLabel = $std.Replace("/std:", "").ToUpper()
        if ($stdLabel -eq "C++LATEST") { $stdLabel = "C++latest" }
        Write-Host "  [MSVC Building] Standard: $stdLabel ..." -ForegroundColor Yellow

        $exeName = "bench_msvc_$($stdLabel.Replace('+', 'p')).exe"
        $exePath = Join-Path $binDir $exeName
        $outJson = Join-Path $resultsDir "results_sbo_msvc_$($stdLabel).json"

        # Build with MSVC
        cmd.exe /c "`"$vcvars64`" >nul 2>&1 && cl.exe /nologo /O2 /Oi /Ot /Gy /EHsc $std /utf-8 /I`"$includeDir1`" /I`"$includeDir2`" `"$benchSource`" /Fe:`"$exePath`" >nul 2>&1"
        $buildOk = ($LASTEXITCODE -eq 0) -and (Test-Path $exePath)

        if ($buildOk) {
            Write-Host "  [MSVC Running] Executing benchmark ($stdLabel) ..." -ForegroundColor Cyan
            & $exePath $dataDir $outJson | Out-Null
            Write-Host "  -> Generated: $outJson" -ForegroundColor Green
        } else {
            Write-Host "  -> [BUILD FAILED] for MSVC $stdLabel" -ForegroundColor Red
        }
    }
}

# -----------------------------------------------------------------------------
# Part 2: WSL2 Linux GCC (Ubuntu 24.04: g++ 14.2)
# -----------------------------------------------------------------------------
$wslCheck = wsl.exe uname -a 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "`n>>> [Platform: WSL2 Linux] Compiler: GCC (g++)" -ForegroundColor Green
    
    # Map paths to WSL
    $wslRoot = "/mnt/" + $rootDir.Substring(0, 1).ToLower() + $rootDir.Substring(2).Replace("\", "/")
    wsl.exe --cd $wslRoot mkdir -p build/matrix_bench
    wsl.exe --cd $wslRoot mkdir -p benchmarks/results

    $gccStandards = if ($Quick) { @("-std=c++20", "-std=c++23") } else { @("-std=c++14", "-std=c++17", "-std=c++20", "-std=c++23") }

    foreach ($std in $gccStandards) {
        $stdLabel = $std.Replace("-std=", "").ToUpper()
        Write-Host "  [WSL GCC Building] Standard: $stdLabel ..." -ForegroundColor Yellow

        $relExe = "build/matrix_bench/bench_gcc_$($stdLabel.Replace('+', 'p'))"
        $outJson = "benchmarks/results/results_sbo_wsl_gcc_$($stdLabel).json"

        wsl.exe --cd $wslRoot g++ $std -O2 -Iinclude -Ibenchmarks/include benchmarks/cpp/bench_layer3_overhead.cpp -o $relExe 2>&1 | Out-Null
        $buildOk = ($LASTEXITCODE -eq 0)

        if ($buildOk) {
            Write-Host "  [WSL GCC Running] Executing benchmark ($stdLabel) ..." -ForegroundColor Cyan
            wsl.exe --cd $wslRoot ./$relExe benchmarks/data $outJson | Out-Null
            Write-Host "  -> Generated: $outJson" -ForegroundColor Green
        } else {
            Write-Host "  -> [BUILD FAILED] for WSL GCC $stdLabel" -ForegroundColor Red
        }
    }

    # Optional: WSL Clang++
    $clangCheck = wsl.exe clang++ --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n>>> [Platform: WSL2 Linux] Compiler: Clang++ (clang++ 18.1)" -ForegroundColor Green
        Write-Host "  [WSL Clang Building] Standard: C++23 ..." -ForegroundColor Yellow
        $relClangExe = "build/matrix_bench/bench_clang_c23"
        $clangJson = "benchmarks/results/results_sbo_wsl_clang_C++23.json"
        wsl.exe --cd $wslRoot clang++ -std=c++23 -O2 -Iinclude -Ibenchmarks/include benchmarks/cpp/bench_layer3_overhead.cpp -o $relClangExe 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  [WSL Clang Running] Executing benchmark (C++23) ..." -ForegroundColor Cyan
            wsl.exe --cd $wslRoot ./$relClangExe benchmarks/data $clangJson | Out-Null
            Write-Host "  -> Generated: $clangJson" -ForegroundColor Green
        }
    }
}

# -----------------------------------------------------------------------------
# Part 3: Python Analysis & Report Generation
# -----------------------------------------------------------------------------
Write-Host "`n>>> Aggregating Matrix & Generating Cross-Platform Report..." -ForegroundColor Cyan
$pythonScript = Join-Path $benchDir "scripts\analyze_sbo_and_cross_platform.py"
python.exe $pythonScript

# Cleanup binaries
Remove-Item $binDir -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $rootDir -Filter *.obj | Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "`n==========================================================================" -ForegroundColor Green
Write-Host "  Benchmark Matrix Execution & Report Generation Complete!                " -ForegroundColor Green
Write-Host "==========================================================================" -ForegroundColor Green
