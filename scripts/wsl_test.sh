#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

# Ensure ~/.dotnet is discoverable in non-interactive shells
if ! command -v dotnet >/dev/null 2>&1 && [ -x "$HOME/.dotnet/dotnet" ]; then
    export DOTNET_ROOT="$HOME/.dotnet"
    export PATH="$DOTNET_ROOT:$PATH"
fi
export DOTNET_CLI_TELEMETRY_OPTOUT=1

echo "=========================================================="
echo "    CPP-BigInt Comprehensive WSL Validation Suite        "
echo "=========================================================="
echo "OS:     $(lsb_release -ds 2>/dev/null || uname -s)"
echo "g++:    $(g++ --version | head -n 1)"
echo "clang+: $(clang++ --version 2>/dev/null | head -n 1 || echo 'not installed')"
echo "CMake:  $(cmake --version | head -n 1)"
echo ".NET:   $(dotnet --version 2>/dev/null || echo 'not installed')"
echo "=========================================================="

run_cmake_suite() {
    local label="$1"
    local cxx="$2"
    local cc="$3"
    local build_type="$4"
    local extra_flags="${5:-}"
    local build_dir="/tmp/bigint_wsl_${label//[^a-zA-Z0-9_]/_}"

    echo ""
    echo ">>> Testing $label (C++23, $build_type)..."
    rm -rf "$build_dir"

    local cmake_args=(
        -B "$build_dir"
        -S "$REPO_ROOT"
        -DCMAKE_CXX_COMPILER="$cxx"
        -DCMAKE_C_COMPILER="$cc"
        -DCMAKE_CXX_STANDARD=23
        -DCMAKE_BUILD_TYPE="$build_type"
        -DBIGINT_BUILD_BENCHMARKS=OFF
    )
    if [ -n "$extra_flags" ]; then
        cmake_args+=(
            -DCMAKE_CXX_FLAGS="$extra_flags"
            -DCMAKE_C_FLAGS="$extra_flags"
        )
    fi

    cmake "${cmake_args[@]}" >/dev/null
    cmake --build "$build_dir" -j"$(nproc)"
    ctest --test-dir "$build_dir" --output-on-failure
    rm -rf "$build_dir"
    echo ">>> $label: PASSED!"
}

# 1. GCC C++23 verification across available g++ versions
for entry in "g++:gcc" "g++-14:gcc-14" "g++-13:gcc-13"; do
    cxx="${entry%%:*}"
    cc="${entry##*:}"
    if command -v "$cxx" >/dev/null 2>&1 && command -v "$cc" >/dev/null 2>&1; then
        run_cmake_suite "GCC_$cxx" "$cxx" "$cc" "Release"
    fi
done

# 2. AddressSanitizer & UndefinedBehaviorSanitizer (ASan + UBSan)
SAN_CXX="g++"
SAN_CC="gcc"
if command -v g++-14 >/dev/null 2>&1 && command -v gcc-14 >/dev/null 2>&1; then
    SAN_CXX="g++-14"
    SAN_CC="gcc-14"
fi
run_cmake_suite "ASan_UBSan_${SAN_CXX}" "$SAN_CXX" "$SAN_CC" "Debug" \
    "-fsanitize=address,undefined -fno-omit-frame-pointer -g"

# 3. Clang C++23 verification (only if std::expected is supported by the active stdlib)
if command -v clang++ >/dev/null 2>&1 && command -v clang >/dev/null 2>&1; then
    CLANG_FLAGS=""
    if ! printf '#include <expected>\nstd::expected<int, int> f() { return 1; }\n' | clang++ -x c++ -std=c++23 -fsyntax-only - >/dev/null 2>&1; then
        if [ -d "/usr/lib/gcc/x86_64-linux-gnu/14" ] && \
           printf '#include <expected>\nstd::expected<int, int> f() { return 1; }\n' | clang++ --gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/14 -x c++ -std=c++23 -fsyntax-only - >/dev/null 2>&1; then
            CLANG_FLAGS="--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/14"
        elif printf '#include <expected>\nstd::expected<int, int> f() { return 1; }\n' | clang++ -stdlib=libc++ -x c++ -std=c++23 -fsyntax-only - >/dev/null 2>&1; then
            CLANG_FLAGS="-stdlib=libc++"
        else
            CLANG_FLAGS="UNSUPPORTED"
        fi
    fi

    if [ "$CLANG_FLAGS" = "UNSUPPORTED" ]; then
        echo ""
        echo ">>> [SKIP] Clang++ ($(clang++ --version | head -n 1)) cannot compile C++23 <expected> with the installed stdlib (requires Clang 19+ or libc++-dev)."
    else
        run_cmake_suite "Clang_Cpp23" "clang++" "clang" "Release" "$CLANG_FLAGS"
    fi
fi

# 4. Packaging & C-API export validation
echo ""
echo ">>> Testing Packaging & C-API Export Scripts under Linux..."
python3 "$REPO_ROOT/scripts/bundle_header.py" -v
python3 "$REPO_ROOT/scripts/export_module.py" -v
python3 "$REPO_ROOT/scripts/test_packaging.py"
python3 "$REPO_ROOT/scripts/export_c_api.py" --compiler gcc --verify
echo ">>> Packaging & C-API Export Validation: PASSED!"

# 5. .NET 10 Cross-Language Validation (C# 14, F# 10, VB.NET 10)
if command -v dotnet >/dev/null 2>&1; then
    echo ""
    echo ">>> Running .NET 10 Cross-Language Tests (C# 14, F# 10, VB.NET 10)..."
    NATIVE_SO="$REPO_ROOT/dotnet/src/Numeric.BigInt/runtimes/linux-x64/native/libbigint_c.so"
    if [ ! -f "$NATIVE_SO" ]; then
        echo "ERROR: Missing $NATIVE_SO" >&2
        exit 1
    fi

    dotnet build "$REPO_ROOT/dotnet/NumericBigInt.slnx" -c Release
    dotnet run --no-build -c Release --project "$REPO_ROOT/dotnet/tests/Numeric.BigInt.Tests.CSharp/Numeric.BigInt.Tests.CSharp.csproj"
    dotnet run --no-build -c Release --project "$REPO_ROOT/dotnet/tests/Numeric.BigInt.Tests.FSharp/Numeric.BigInt.Tests.FSharp.fsproj"
    dotnet run --no-build -c Release --project "$REPO_ROOT/dotnet/tests/Numeric.BigInt.Tests.VB/Numeric.BigInt.Tests.VB.vbproj"

    PACK_OUT="/tmp/bigint_wsl_nuget"
    rm -rf "$PACK_OUT"
    dotnet pack "$REPO_ROOT/dotnet/src/Numeric.BigInt/Numeric.BigInt.csproj" -c Release --no-build -o "$PACK_OUT"
    python3 - <<'PY'
import zipfile, glob, sys
pkgs = glob.glob("/tmp/bigint_wsl_nuget/*.nupkg")
assert pkgs, "No .nupkg produced"
with zipfile.ZipFile(pkgs[0]) as zf:
    names = zf.namelist()
    assert "runtimes/win-x64/native/bigint_c.dll" in names, f"Missing win-x64 dll in {names}"
    assert "runtimes/linux-x64/native/libbigint_c.so" in names, f"Missing linux-x64 so in {names}"
print("[PASS] NuGet package contains both win-x64/native/bigint_c.dll and linux-x64/native/libbigint_c.so")
PY
    rm -rf "$PACK_OUT"
    echo ">>> .NET 10 Cross-Language Validation: PASSED!"
else
    echo ""
    echo ">>> [SKIP] dotnet SDK not found in WSL."
fi

echo ""
echo "=========================================================="
echo "    ALL COMPREHENSIVE WSL TESTS PASSED SUCCESSFULLY!     "
echo "=========================================================="
