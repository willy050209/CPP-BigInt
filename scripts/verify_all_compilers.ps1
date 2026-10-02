# scripts/verify_all_compilers.ps1
# Automated cross-compiler and cross-standard matrix verification for CPP-BigInt

param (
    [switch]$Quick
)

$ErrorActionPreference = "Continue"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$rootDir = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $rootDir "build\matrix_tests"
if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir -Force | Out-Null
}

$vcvars64 = "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
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

    # Run the test binary
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

# --- 1. MSVC (cl.exe) ---
if (Test-Path $vcvars64) {
    $msvcStandards = @("/std:c++14", "/std:c++17", "/std:c++20", "/std:c++latest")
    foreach ($std in $msvcStandards) {
        $stdLabel = $std.Replace("/std:", "").ToUpper()
        $exeName = "test_msvc_$($stdLabel.Replace('+', 'p')).exe"
        $exePath = Join-Path $binDir $exeName
        $res = Run-TestItem -Compiler "MSVC (cl 19.51)" -Standard $stdLabel -ExePath $exePath -BuildScript {
            cmd.exe /c "`"$vcvars64`" >nul 2>&1 && cl.exe /nologo /W4 /O2 /EHsc /MD $std /utf-8 /Zc:__cplusplus /I`"$rootDir\include`" `"$rootDir\tests\*.cpp`" /Fe:`"$exePath`" >nul 2>&1"
        }
        $results += $res
    }
}

# --- 2. Clang++ (LLVM) ---
if (Test-Path $clangpp) {
    # Note: Clang on Windows uses MSVC STL which requires C++14+
    $clangStandards = @("-std=c++14", "-std=c++17", "-std=c++20", "-std=c++23")
    foreach ($std in $clangStandards) {
        $stdLabel = $std.Replace("-std=", "").ToUpper()
        $exeName = "test_clang_$($stdLabel.Replace('+', 'p')).exe"
        $exePath = Join-Path $binDir $exeName
        $res = Run-TestItem -Compiler "Clang++ 23.1.2" -Standard $stdLabel -ExePath $exePath -BuildScript {
            & $clangpp (Get-Item "$rootDir\tests\*.cpp").FullName $std -O2 -I "$rootDir\include" -rtlib=compiler-rt -o $exePath 2>&1 | Out-Null
        }
        $results += $res
    }
}

# --- 3. GCC (MinGW-w64 UCRT64) ---
if (Test-Path $gpp) {
    $oldPath = $env:PATH
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
    $gccStandards = @("-std=c++11", "-std=c++14", "-std=c++17", "-std=c++20", "-std=c++23")
    foreach ($std in $gccStandards) {
        $stdLabel = $std.Replace("-std=", "").ToUpper()
        $exeName = "test_gcc_$($stdLabel.Replace('+', 'p')).exe"
        $exePath = Join-Path $binDir $exeName
        $res = Run-TestItem -Compiler "GCC (g++ 16.1.0)" -Standard $stdLabel -ExePath $exePath -BuildScript {
            & $gpp (Get-Item "$rootDir\tests\*.cpp").FullName $std -O2 -static -I "$rootDir\include" -o $exePath 2>&1 | Out-Null
        }
        $results += $res
    }
    $env:PATH = $oldPath
}

Write-Host ""
Write-Host "==========================================================================================" -ForegroundColor Cyan
Write-Host "                             MATRIX VERIFICATION SUMMARY RESULTS                          " -ForegroundColor Cyan
Write-Host "==========================================================================================" -ForegroundColor Cyan
$results | Format-Table -AutoSize

# Cleanup build artifacts
Remove-Item $binDir -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $rootDir -Filter *.obj | Remove-Item -Force -ErrorAction SilentlyContinue
