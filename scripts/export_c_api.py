#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scripts/export_c_api.py
=======================
CPP-BigInt C-API Library Exporter

Exports a self-contained C99/C11 ABI distribution bundle (headers, shared library,
import library, usage examples, and README) so other languages (Python, Rust, Go,
.NET, C) and legacy C++ codebases (C++11/14/17/20) can call CPP-BigInt's C++23
engine without requiring a C++23 compiler on the caller side.

Also synchronizes `dist/bigint_c.h` from `include/numeric/c_api.h`.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


EXAMPLE_C_SOURCE = r"""#include "bigint_c.h"
#include <stdio.h>

int main(void) {
    bigint_sbo256_t sbo_a, sbo_b, sbo_sum;
    bigint_t* a = bigint_init_sbo256_str(&sbo_a, "123456789012345678901234567890", 10);
    bigint_t* b = bigint_init_sbo256_str(&sbo_b, "987654321098765432109876543210", 10);
    bigint_t* sum = bigint_init_sbo256(&sbo_sum, 0);

    if (!a || !b || !sum || bigint_mul(sum, a, b) != BIGINT_OK) {
        return 1;
    }

    char buf[128];
    if (bigint_to_str(sum, buf, sizeof(buf), 10) != BIGINT_OK) {
        return 2;
    }
    printf("[C99] product = %s\n", buf);

    bigint_destroy_sbo256(&sbo_sum);
    bigint_destroy_sbo256(&sbo_b);
    bigint_destroy_sbo256(&sbo_a);
    return 0;
}
"""

EXAMPLE_LEGACY_CPP_SOURCE = r"""#include "bigint_c.h"
#include <iostream>
#include <string>

// Minimal RAII wrapper usable in C++11/14/17/20 without C++23
class LegacyBigInt {
public:
    LegacyBigInt() : handle_(bigint_create()) {}
    explicit LegacyBigInt(const char* s, int base = 10) : handle_(bigint_create_str(s, base)) {}
    ~LegacyBigInt() { bigint_destroy(handle_); }

    LegacyBigInt(const LegacyBigInt& other) : handle_(bigint_clone(other.handle_)) {}
    LegacyBigInt& operator=(const LegacyBigInt& other) {
        if (this != &other) {
            bigint_assign(handle_, other.handle_);
        }
        return *this;
    }

    LegacyBigInt operator*(const LegacyBigInt& rhs) const {
        LegacyBigInt out;
        bigint_mul(out.handle_, handle_, rhs.handle_);
        return out;
    }

    std::string to_string(int base = 10) const {
        char* raw = bigint_to_str_alloc(handle_, base);
        if (!raw) return "";
        std::string res(raw);
        bigint_free_str(raw);
        return res;
    }

private:
    bigint_t* handle_;
};

int main() {
    LegacyBigInt a("123456789012345678901234567890");
    LegacyBigInt b("987654321098765432109876543210");
    LegacyBigInt prod = a * b;
    std::cout << "[Legacy C++ __cplusplus=" << __cplusplus << "] product = "
              << prod.to_string() << std::endl;
    return 0;
}
"""

EXAMPLE_PYTHON_SOURCE = r"""import ctypes
import os
import platform
import sys
from pathlib import Path

bundle_dir = Path(__file__).resolve().parent.parent
system = platform.system()
if system == "Windows":
    lib_path = bundle_dir / "bin" / "bigint_c.dll"
elif system == "Darwin":
    lib_path = bundle_dir / "lib" / "libbigint_c.dylib"
else:
    lib_path = bundle_dir / "lib" / "libbigint_c.so"

lib = ctypes.CDLL(str(lib_path))
lib.bigint_create_str.restype = ctypes.c_void_p
lib.bigint_create_str.argtypes = [ctypes.c_char_p, ctypes.c_int]
lib.bigint_create.restype = ctypes.c_void_p
lib.bigint_destroy.argtypes = [ctypes.c_void_p]
lib.bigint_mul.restype = ctypes.c_int
lib.bigint_mul.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
lib.bigint_to_str_alloc.restype = ctypes.c_void_p
lib.bigint_to_str_alloc.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.bigint_free_str.argtypes = [ctypes.c_void_p]

a = lib.bigint_create_str(b"123456789012345678901234567890", 10)
b = lib.bigint_create_str(b"987654321098765432109876543210", 10)
out = lib.bigint_create()
rc = lib.bigint_mul(out, a, b)
if rc != 0:
    sys.exit(rc)

ptr = lib.bigint_to_str_alloc(out, 10)
val = ctypes.cast(ptr, ctypes.c_char_p).value.decode("utf-8")
print(f"[Python ctypes] product = {val}")
assert val == "121932631137021795226185032733622923332237463801111263526900"

lib.bigint_free_str(ptr)
lib.bigint_destroy(out)
lib.bigint_destroy(b)
lib.bigint_destroy(a)
"""

EXPORT_README = """# CPP-BigInt C-API Export Bundle

Self-contained C99/C11 ABI distribution for calling CPP-BigInt from other languages
(Python, Rust, Go, .NET, C) and legacy C++ standards (C++11 / C++14 / C++17 / C++20).

## Contents

- `include/bigint_c.h` & `include/numeric/c_api.h`: C99/C11 ABI headers
- `lib/`: Shared library (`libbigint_c.so` / `libbigint_c.dylib` / `bigint_c.lib`)
- `bin/`: Windows runtime DLL (`bigint_c.dll`, when built on Windows)
- `examples/`: Ready-to-run C99, Legacy C++11/14, and Python `ctypes` samples

## Quick Usage

### Linux / macOS (C99 or Legacy C++11~20)
```bash
cc -std=c99 -Iinclude examples/example_c.c -Llib -lbigint_c -Wl,-rpath,'$ORIGIN/lib' -o example_c
c++ -std=c++11 -Iinclude examples/example_legacy_cpp.cpp -Llib -lbigint_c -Wl,-rpath,'$ORIGIN/lib' -o example_cpp11
```

### Windows MSVC (C or Legacy C++14~20)
```powershell
cl.exe /EHsc /std:c++14 /DBIGINT_C_SHARED /Iinclude examples\\example_legacy_cpp.cpp lib\\bigint_c.lib /Fe:example_cpp14.exe
```

### Python (`ctypes`)
```bash
python examples/example_python.py
```
"""


def find_repo_root(start_path: Path) -> Path:
    curr = start_path.resolve()
    for _ in range(10):
        if (curr / "CMakeLists.txt").exists() or (curr / ".git").exists():
            return curr
        if curr.parent == curr:
            break
        curr = curr.parent
    return start_path.resolve()


def sync_standalone_c_header(src_header: Path, dst_header: Path) -> None:
    text = src_header.read_text(encoding="utf-8-sig")
    lines = text.splitlines()
    # Keep pure ASCII C99/C11 header without #pragma once so legacy C/C++ and MSVC CP950 build warning-free
    cleaned = [
        line for line in lines
        if not line.strip().startswith("// filepath:")
        and line.strip() != "#pragma once"
        and all(ord(ch) < 128 for ch in line)
    ]
    while cleaned and not cleaned[0].strip():
        cleaned.pop(0)
    dst_header.parent.mkdir(parents=True, exist_ok=True)
    dst_header.write_text("\n".join(cleaned) + "\n", encoding="utf-8")


def find_vcvars64() -> Path | None:
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        res = subprocess.run(
            [str(vswhere), "-prerelease", "-latest", "-property", "installationPath"],
            capture_output=True, text=True, check=False
        )
        if res.returncode == 0 and res.stdout.strip():
            candidate = Path(res.stdout.strip()) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
            if candidate.exists():
                return candidate
    fallback = Path(r"C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat")
    return fallback if fallback.exists() else None


def run_cmd(cmd: list[str], cwd: Path | None = None, env: dict | None = None) -> None:
    print(f"[RUN] {' '.join(cmd)}")
    subprocess.run(cmd, cwd=str(cwd) if cwd else None, env=env, check=True)


def run_windows_vs_cmd(vcvars: Path, commands: list[str], cwd: Path | None = None) -> None:
    print(f"[RUN/VS] {' && '.join(commands)}")
    with tempfile.NamedTemporaryFile("w", suffix=".bat", delete=False, encoding="utf-8") as tf:
        tf.write("@echo off\r\n")
        tf.write(f'call "{vcvars}" >nul 2>&1 || exit /b 1\r\n')
        for c in commands:
            tf.write(f"{c} || exit /b %ERRORLEVEL%\r\n")
        bat_path = Path(tf.name)
    try:
        subprocess.run(["cmd.exe", "/c", str(bat_path)], cwd=str(cwd) if cwd else None, check=True)
    finally:
        bat_path.unlink(missing_ok=True)


def build_c_api_library(repo_root: Path, build_dir: Path, build_type: str, compiler: str) -> Path:
    cmake_bin = shutil.which("cmake")
    if platform.system() == "Windows" and not cmake_bin and compiler in ("auto", "msvc"):
        vcvars = find_vcvars64()
        if not vcvars:
            raise FileNotFoundError("cmake not found on PATH and Visual Studio vcvars64.bat not found.")
        cfg_cmd = (
            f'cmake -B "{build_dir}" -S "{repo_root}" '
            f'-DCMAKE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE={build_type} '
            f'-DBIGINT_BUILD_C_API=ON -DBIGINT_BUILD_BENCHMARKS=OFF'
        )
        bld_cmd = f'cmake --build "{build_dir}" --target bigint_c --config {build_type} -j'
        run_windows_vs_cmd(vcvars, [cfg_cmd, bld_cmd])
        return build_dir

    cmake_args = [
        cmake_bin or "cmake",
        "-B", str(build_dir),
        "-S", str(repo_root),
        "-DCMAKE_CXX_STANDARD=23",
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DBIGINT_BUILD_C_API=ON",
        "-DBIGINT_BUILD_BENCHMARKS=OFF",
    ]
    if compiler == "gcc":
        cxx = "g++-14" if shutil.which("g++-14") else "g++"
        cc = "gcc-14" if shutil.which("gcc-14") else "gcc"
        cmake_args += [f"-DCMAKE_CXX_COMPILER={cxx}", f"-DCMAKE_C_COMPILER={cc}"]
    elif compiler == "clang":
        cmake_args += ["-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_C_COMPILER=clang"]

    run_cmd(cmake_args)
    run_cmd([cmake_bin or "cmake", "--build", str(build_dir), "--target", "bigint_c", "--config", build_type, "-j"])
    return build_dir


def collect_built_artifacts(build_dir: Path, output_dir: Path, build_type: str) -> None:
    lib_dir = output_dir / "lib"
    bin_dir = output_dir / "bin"
    lib_dir.mkdir(parents=True, exist_ok=True)
    bin_dir.mkdir(parents=True, exist_ok=True)

    search_dirs = [build_dir / build_type, build_dir]
    copied = []
    for sdir in search_dirs:
        if not sdir.exists():
            continue
        for fname in ("bigint_c.dll",):
            p = sdir / fname
            if p.exists():
                shutil.copy2(p, bin_dir / fname)
                copied.append(bin_dir / fname)
        for fname in ("bigint_c.lib", "libbigint_c.so", "libbigint_c.dylib", "libbigint_c.dll.a"):
            p = sdir / fname
            if p.exists():
                shutil.copy2(p, lib_dir / fname)
                copied.append(lib_dir / fname)
        if copied:
            break

    if not copied:
        raise FileNotFoundError(f"Could not find compiled bigint_c artifacts in {build_dir}")

    for item in copied:
        print(f"[EXPORTED] {item}")


def verify_exported_bundle(output_dir: Path) -> None:
    print("\n=== Verifying Exported C-API Bundle (C99 + Legacy C++ + Python) ===")
    inc_dir = output_dir / "include"
    lib_dir = output_dir / "lib"
    bin_dir = output_dir / "bin"
    examples_dir = output_dir / "examples"

    # 1. Python ctypes verification
    run_cmd([sys.executable, str(examples_dir / "example_python.py")])

    with tempfile.TemporaryDirectory(prefix="bigint_capi_verify_") as tmp:
        tmp_path = Path(tmp)
        system = platform.system()
        if system == "Windows":
            vcvars = find_vcvars64()
            if not vcvars:
                raise RuntimeError("MSVC vcvars64.bat not found for C/Legacy C++ verification.")

            import_lib = lib_dir / "bigint_c.lib"
            shutil.copy2(bin_dir / "bigint_c.dll", tmp_path / "bigint_c.dll")

            c_exe = tmp_path / "example_c.exe"
            cpp_exe = tmp_path / "example_legacy_cpp14.exe"
            run_windows_vs_cmd(
                vcvars,
                [
                    f'cl.exe /nologo /W4 /DBIGINT_C_SHARED /I"{inc_dir}" "{examples_dir / "example_c.c"}" "{import_lib}" /Fo:"{tmp_path / "c.obj"}" /Fe:"{c_exe}"',
                    f'cl.exe /nologo /W4 /EHsc /std:c++14 /DBIGINT_C_SHARED /I"{inc_dir}" "{examples_dir / "example_legacy_cpp.cpp"}" "{import_lib}" /Fo:"{tmp_path / "cpp.obj"}" /Fe:"{cpp_exe}"',
                ],
            )
            run_cmd([str(c_exe)], cwd=tmp_path)
            run_cmd([str(cpp_exe)], cwd=tmp_path)
        else:
            cc = shutil.which("gcc-14") or shutil.which("gcc") or shutil.which("clang") or "cc"
            cxx = shutil.which("g++-14") or shutil.which("g++") or shutil.which("clang++") or "c++"
            c_exe = tmp_path / "example_c"
            cpp_exe = tmp_path / "example_legacy_cpp11"

            run_cmd([cc, "-std=c99", "-Wall", "-Wextra", f"-I{inc_dir}", str(examples_dir / "example_c.c"), f"-L{lib_dir}", "-lbigint_c", f"-Wl,-rpath,{lib_dir}", "-o", str(c_exe)])
            run_cmd([cxx, "-std=c++11", "-Wall", "-Wextra", f"-I{inc_dir}", str(examples_dir / "example_legacy_cpp.cpp"), f"-L{lib_dir}", "-lbigint_c", f"-Wl,-rpath,{lib_dir}", "-o", str(cpp_exe)])
            run_cmd([str(c_exe)])
            run_cmd([str(cpp_exe)])

    print("[PASS] C99, Legacy C++ (C++11/C++14), and Python ctypes verified against exported C-API bundle!")


def export_bundle(repo_root: Path, output_dir: Path, build_type: str, compiler: str, verify: bool) -> None:
    src_c_header = repo_root / "include" / "numeric" / "c_api.h"
    dist_c_header = repo_root / "dist" / "bigint_c.h"
    sync_standalone_c_header(src_c_header, dist_c_header)
    print(f"[SYNCED] {dist_c_header}")

    inc_dir = output_dir / "include"
    inc_numeric_dir = inc_dir / "numeric"
    examples_dir = output_dir / "examples"
    inc_numeric_dir.mkdir(parents=True, exist_ok=True)
    examples_dir.mkdir(parents=True, exist_ok=True)

    sync_standalone_c_header(src_c_header, inc_dir / "bigint_c.h")
    shutil.copy2(src_c_header, inc_numeric_dir / "c_api.h")
    (examples_dir / "example_c.c").write_text(EXAMPLE_C_SOURCE, encoding="utf-8")
    (examples_dir / "example_legacy_cpp.cpp").write_text(EXAMPLE_LEGACY_CPP_SOURCE, encoding="utf-8")
    (examples_dir / "example_python.py").write_text(EXAMPLE_PYTHON_SOURCE, encoding="utf-8")
    (output_dir / "README.md").write_text(EXPORT_README, encoding="utf-8")

    with tempfile.TemporaryDirectory(prefix="bigint_capi_build_") as tmp_build:
        build_dir = Path(tmp_build) / "build"
        build_c_api_library(repo_root, build_dir, build_type, compiler)
        collect_built_artifacts(build_dir, output_dir, build_type)

    print(f"[SUCCESS] C-API library bundle exported to: {output_dir}")
    if verify:
        verify_exported_bundle(output_dir)


def main() -> None:
    parser = argparse.ArgumentParser(description="Export CPP-BigInt C-API library bundle for FFI and legacy C++.")
    parser.add_argument("-o", "--output", help="Output bundle directory (default: dist/c_api)")
    parser.add_argument("--build-type", default="Release", choices=["Release", "Debug"], help="CMake build type")
    parser.add_argument("--compiler", default="auto", choices=["auto", "msvc", "gcc", "clang"], help="C++ compiler toolchain")
    parser.add_argument("--verify", action="store_true", help="Compile & run C99, legacy C++, and Python tests against exported bundle")
    args = parser.parse_args()

    repo_root = find_repo_root(Path(__file__).resolve().parent)
    output_dir = Path(args.output).resolve() if args.output else (repo_root / "dist" / "c_api")

    try:
        export_bundle(repo_root, output_dir, args.build_type, args.compiler, args.verify)
    except Exception as exc:
        print(f"[ERROR] C-API export failed: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
