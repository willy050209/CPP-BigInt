# scripts/verify_all_compilers.ps1
# Automated C++23 cross-compiler verification for CPP-BigInt (Windows & WSL)

param (
    [switch]$IncludeWsl
)

$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$rootDir = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $rootDir "build\matrix_tests"
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

$clangpp = "C:\Program Files\LLVM\bin\clang++.exe"
$gpp = "C:\msys64\ucrt64\bin\g++.exe"

$results = @()

function Run-TestItem {
    param (
        [string]$Compiler,
        [string]$Standard,
        [scriptblock]$BuildScript,
        [string]$ExePath
    )

    Write-Host "==========================================================" -ForegroundColor Cyan
    Write-Host " [RUNNING] $Compiler - $Standard" -ForegroundColor Yellow
    Write-Host "==========================================================" -ForegroundColor Cyan

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $BuildScript
    $buildOk = ($LASTEXITCODE -eq 0) -and (Test-Path $ExePath)

    if (-not $buildOk) {
        $sw.Stop()
        Write-Host "  -> [BUILD FAILED]" -ForegroundColor Red
        return [PSCustomObject]@{
            Compiler   = $Compiler
            Standard   = $Standard
            Build      = "FAILED"
            Test       = "SKIPPED"
            Assertions = 0
            Passed     = 0
            Duration   = "$([math]::Round($sw.Elapsed.TotalSeconds, 2))s"
        }
    }

    $testOutput = & $ExePath 2>&1
    $testExit = $LASTEXITCODE
    $sw.Stop()

    $totalAsserts = 0
    $passedAsserts = 0
    foreach ($line in $testOutput) {
        if ($line -match "Total Assertions:\s+(\d+)") {
            $totalAsserts = [int]$matches[1]
        }
        if ($line -match "Passed:\s+(\d+)") {
            $passedAsserts = [int]$matches[1]
        }
    }

    $testOk = ($testExit -eq 0) -and ($passedAsserts -gt 0) -and ($totalAsserts -eq $passedAsserts)

    if ($testOk) {
        Write-Host "  -> [PASSED] $passedAsserts / $totalAsserts assertions ($([math]::Round($sw.Elapsed.TotalSeconds, 2))s)" -ForegroundColor Green
    } else {
        Write-Host "  -> [TEST FAILED] Exit code: $testExit" -ForegroundColor Red
    }

    Remove-Item $ExePath -Force -ErrorAction SilentlyContinue

    return [PSCustomObject]@{
        Compiler   = $Compiler
        Standard   = $Standard
        Build      = "PASSED"
        Test       = if ($testOk) { "PASSED" } else { "FAILED" }
        Assertions = $totalAsserts
        Passed     = $passedAsserts
        Duration   = "$([math]::Round($sw.Elapsed.TotalSeconds, 2))s"
    }
}

# --- 1. MSVC (cl.exe, /std:c++latest) ---
if ($vcvars64) {
    $exePath = Join-Path $binDir "test_msvc_cpp23.exe"
    $results += Run-TestItem -Compiler "MSVC (cl.exe)" -Standard "C++23 (/std:c++latest)" -ExePath $exePath -BuildScript {
        cmd.exe /c "`"$vcvars64`" >nul 2>&1 && cl.exe /nologo /W4 /WX /O2 /EHsc /MD /std:c++latest /permissive- /utf-8 /Zc:__cplusplus /I`"$rootDir\include`" `"$rootDir\tests\*.cpp`" /Fe:`"$exePath`" >nul 2>&1"
    }
}

# --- 2. Clang++ (LLVM Windows, -std=c++23) ---
if (Test-Path $clangpp) {
    $exePath = Join-Path $binDir "test_clang_cpp23.exe"
    $results += Run-TestItem -Compiler "Clang++ (Windows)" -Standard "C++23 (-std=c++23)" -ExePath $exePath -BuildScript {
        & $clangpp (Get-Item "$rootDir\tests\*.cpp").FullName -std=c++23 -O2 -Wall -Wextra -Werror -I "$rootDir\include" -rtlib=compiler-rt -o $exePath 2>&1 | Out-Null
    }
}

# --- 3. GCC (MinGW-w64 UCRT64, -std=c++23) ---
if (Test-Path $gpp) {
    $oldPath = $env:PATH
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
    $exePath = Join-Path $binDir "test_gcc_cpp23.exe"
    $results += Run-TestItem -Compiler "GCC (MinGW UCRT64)" -Standard "C++23 (-std=c++23)" -ExePath $exePath -BuildScript {
        & $gpp (Get-Item "$rootDir\tests\*.cpp").FullName -std=c++23 -O2 -Wall -Wextra -Werror -static -I "$rootDir\include" -o $exePath 2>&1 | Out-Null
    }
    $env:PATH = $oldPath
}

Write-Host ""
Write-Host "==========================================================================================" -ForegroundColor Cyan
Write-Host "                          C++23 COMPILER MATRIX SUMMARY RESULTS                           " -ForegroundColor Cyan
Write-Host "==========================================================================================" -ForegroundColor Cyan
$results | Format-Table -AutoSize

Remove-Item $binDir -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $rootDir -Filter *.obj | Remove-Item -Force -ErrorAction SilentlyContinue

if ($IncludeWsl) {
    $wslRoot = "/mnt/" + $rootDir.Substring(0, 1).ToLower() + $rootDir.Substring(2).Replace("\", "/")
    wsl.exe bash "$wslRoot/scripts/wsl_test.sh"
}

if ($results | Where-Object { $_.Build -ne "PASSED" -or $_.Test -ne "PASSED" }) {
    exit 1
}
