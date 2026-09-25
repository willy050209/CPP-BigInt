#!/usr/bin/env bash
set -e

REPO_ROOT="/mnt/d/program/C++/CPP-BigInt"
cd "$REPO_ROOT"

echo "=========================================================="
echo "    CPP-BigInt Comprehensive WSL Validation Suite        "
echo "=========================================================="
echo "OS: $(lsb_release -d 2>/dev/null | cut -f2 || uname -s)"
echo "GCC: $(g++ --version | head -n 1)"
echo "Clang: $(clang++ --version | head -n 1)"
echo "CMake: $(cmake --version | head -n 1)"
echo "=========================================================="

FAILED=0

# 1. GCC across C++ standards: 11, 14, 17, 20, 23
for std in 11 14 17 20 23; do
    echo ""
    echo ">>> Testing GCC (g++) with -std=c++$std..."
    BUILD_DIR="/tmp/bigint_build_gcc_$std"
    rm -rf "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CXX_STANDARD=$std \
        -DCMAKE_BUILD_TYPE=Release \
        -DBIGINT_BUILD_BENCHMARKS=OFF > /dev/null
    cmake --build "$BUILD_DIR" -j$(nproc)
    "$BUILD_DIR/bigint_test"
    rm -rf "$BUILD_DIR"
    echo ">>> GCC C++$std: PASSED!"
done

# 2. Clang across C++ standards: 11, 14, 17, 20, 23
for std in 11 14 17 20 23; do
    echo ""
    echo ">>> Testing Clang (clang++) with -std=c++$std..."
    BUILD_DIR="/tmp/bigint_build_clang_$std"
    rm -rf "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_STANDARD=$std \
        -DCMAKE_BUILD_TYPE=Release \
        -DBIGINT_BUILD_BENCHMARKS=OFF > /dev/null
    cmake --build "$BUILD_DIR" -j$(nproc)
    "$BUILD_DIR/bigint_test"
    rm -rf "$BUILD_DIR"
    echo ">>> Clang C++$std: PASSED!"
done

# 3. AddressSanitizer & UndefinedBehaviorSanitizer (ASan + UBSan)
echo ""
echo ">>> Testing with AddressSanitizer & UndefinedBehaviorSanitizer (Clang C++20)..."
BUILD_DIR="/tmp/bigint_build_sanitizer"
rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
    -DBIGINT_BUILD_BENCHMARKS=OFF > /dev/null
cmake --build "$BUILD_DIR" -j$(nproc)
"$BUILD_DIR/bigint_test"
rm -rf "$BUILD_DIR"
echo ">>> ASan + UBSan: PASSED (Zero memory leaks or undefined behavior)!"

# 4. Packaging validation
echo ""
echo ">>> Testing Packaging Scripts under Linux..."
python3 "$REPO_ROOT/scripts/bundle_header.py" -v
python3 "$REPO_ROOT/scripts/export_module.py" -v
python3 "$REPO_ROOT/scripts/test_packaging.py"
echo ">>> Packaging Validation: PASSED!"

echo ""
echo "=========================================================="
echo "    ALL COMPREHENSIVE WSL TESTS PASSED SUCCESSFULLY!     "
echo "=========================================================="
