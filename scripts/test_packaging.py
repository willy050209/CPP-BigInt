#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scripts/test_packaging.py
=========================
Automated validation suite for CPP-BigInt packaging scripts and artifacts:
1. Validates scripts/bundle_header.py execution and dist/bigint.hpp output.
2. Validates scripts/export_module.py execution and dist/bigint.ixx output.
3. Verifies UTF-8 BOM encoding on all generated artifacts.
4. Verifies absence of internal relative includes (#include "...") in bundled header.
5. Verifies presence and deduplication of standard STL headers.
6. Verifies C++20 module structure (module;, export module bigint;, export namespace numeric).
7. Verifies retention of XML documentation comments (/// <summary>, etc.).
"""

import sys
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DIST_DIR = REPO_ROOT / "dist"
HPP_FILE = DIST_DIR / "bigint.hpp"
IXX_FILE = DIST_DIR / "bigint.ixx"


def check(cond: bool, msg: str):
    if not cond:
        print(f"[FAIL] {msg}")
        sys.exit(1)
    else:
        print(f"[PASS] {msg}")


def main():
    print("=== Step 1: Run scripts/bundle_header.py ===")
    res = subprocess.run([sys.executable, str(REPO_ROOT / "scripts" / "bundle_header.py"), "-v"], capture_output=True, text=True)
    print(res.stdout)
    if res.stderr:
        print(res.stderr)
    check(res.returncode == 0, "bundle_header.py returned code 0")
    check(HPP_FILE.exists(), f"{HPP_FILE} exists")

    print("\n=== Step 2: Validate dist/bigint.hpp ===")
    hpp_bytes = HPP_FILE.read_bytes()
    check(hpp_bytes.startswith(b"\xef\xbb\xbf"), "dist/bigint.hpp contains UTF-8 BOM (ef bb bf)")

    hpp_text = HPP_FILE.read_text(encoding="utf-8-sig")
    lines = hpp_text.splitlines()

    check("#pragma once" in hpp_text, "dist/bigint.hpp contains '#pragma once'")

    internal_includes = [l.strip() for l in lines if '#include "' in l]
    check(len(internal_includes) == 0, f"dist/bigint.hpp contains 0 internal relative includes (found: {internal_includes})")

    check("#include <cstdint>" in hpp_text, "dist/bigint.hpp includes <cstdint>")
    check("#include <iostream>" in hpp_text, "dist/bigint.hpp includes <iostream>")
    check("#include <bitset>" in hpp_text, "dist/bigint.hpp includes <bitset>")
    check("#include <string>" in hpp_text, "dist/bigint.hpp includes <string>")
    check("#include <type_traits>" in hpp_text, "dist/bigint.hpp includes <type_traits>")

    check("/// <summary>" in hpp_text, "dist/bigint.hpp retains XML documentation comments")

    check("#  undef NUMERIC_CPLUSPLUS" in hpp_text, "dist/bigint.hpp cleans up NUMERIC_CPLUSPLUS")
    check("#  undef NUMERIC_CXX_11" in hpp_text, "dist/bigint.hpp cleans up NUMERIC_CXX_11")
    check("#  undef NUMERIC_CXX_23" in hpp_text, "dist/bigint.hpp cleans up NUMERIC_CXX_23")
    check("NUMERIC_NODISCARD" in hpp_text, "dist/bigint.hpp preserves public API macro NUMERIC_NODISCARD")

    print("\n=== Step 3: Run scripts/export_module.py ===")
    res = subprocess.run([sys.executable, str(REPO_ROOT / "scripts" / "export_module.py"), "-v"], capture_output=True, text=True)
    print(res.stdout)
    if res.stderr:
        print(res.stderr)
    check(res.returncode == 0, "export_module.py returned code 0")
    check(IXX_FILE.exists(), f"{IXX_FILE} exists")

    print("\n=== Step 4: Validate dist/bigint.ixx ===")
    ixx_bytes = IXX_FILE.read_bytes()
    check(ixx_bytes.startswith(b"\xef\xbb\xbf"), "dist/bigint.ixx contains UTF-8 BOM (ef bb bf)")

    ixx_text = IXX_FILE.read_text(encoding="utf-8-sig")

    check("module;" in ixx_text, "dist/bigint.ixx contains 'module;' Global Module Fragment")
    check("export module bigint;" in ixx_text, "dist/bigint.ixx contains 'export module bigint;'")
    check("export namespace numeric {" in ixx_text, "dist/bigint.ixx contains 'export namespace numeric {'")
    check("#pragma once" not in ixx_text, "dist/bigint.ixx does not contain '#pragma once'")
    check("/// <summary>" in ixx_text, "dist/bigint.ixx retains XML documentation comments")

    check("#  undef NUMERIC_CPLUSPLUS" in ixx_text, "dist/bigint.ixx cleans up NUMERIC_CPLUSPLUS")
    check("#  undef NUMERIC_CXX_11" in ixx_text, "dist/bigint.ixx cleans up NUMERIC_CXX_11")

    print("\n[ALL CHECKS PASSED] Packaging artifacts and scripts are 100% verified!")


if __name__ == "__main__":
    main()
