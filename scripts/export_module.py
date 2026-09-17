#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scripts/export_module.py
========================
CPP-BigInt C++20 Module Exporter Script

Responsibilities:
1. Takes the unified single-header `dist/bigint.hpp` (or runs `bundle_header.py`).
2. Converts and packages it into a standard C++20 Module Interface Unit `dist/bigint.ixx`.
3. Places standard headers and configuration directives in the Global Module Fragment:
     module;
     #include <...>
     export module bigint;
4. Transforms `namespace numeric` blocks into:
     export namespace numeric { ... }
   so all functions, types, and templates are cleanly exported.
5. Retains XML documentation comments (/// <summary>, etc.) and regular comments.
6. Enforces UTF-8 BOM encoding for the output file `dist/bigint.ixx`.
"""

import os
import sys
import re
import argparse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
try:
    import bundle_header
except ImportError:
    bundle_header = None


RE_PRAGMA_ONCE = re.compile(r'^\s*#\s*pragma\s+once\b')
RE_INCLUDE_EXTERNAL = re.compile(r'^\s*#\s*include\s*<([^>]+)>')
RE_NAMESPACE_NUMERIC = re.compile(r'^(?P<indent>\s*)namespace\s+numeric\b(?!\s*::)(?P<rest>.*)$')
RE_NAMESPACE_NUMERIC_DETAIL = re.compile(r'^(?P<indent>\s*)namespace\s+numeric::detail\b(?P<rest>.*)$')


def find_repo_root(start_path: Path) -> Path:
    curr = start_path.resolve()
    for _ in range(10):
        if (curr / "CMakeLists.txt").exists() or (curr / ".git").exists():
            return curr
        if curr.parent == curr:
            break
        curr = curr.parent
    return start_path.resolve()


def export_cxx20_module(repo_root: Path, hpp_input: Path, ixx_output: Path, verbose: bool = False) -> Path:
    if not hpp_input.exists():
        if verbose:
            print(f"[INFO] {hpp_input} not found. Running bundle_header.py...")
        if bundle_header is not None:
            bundle_header.emit_single_header(repo_root, hpp_input, verbose=verbose)
        else:
            raise FileNotFoundError(f"Missing {hpp_input} and cannot import bundle_header.py")

    with open(hpp_input, "r", encoding="utf-8-sig", errors="replace") as f:
        content = f.read()

    lines = content.splitlines()

    gmf_headers: list[str] = []
    body_lines: list[str] = []

    in_macro_cleanup = False

    for line in lines:
        stripped = line.strip()

        if RE_PRAGMA_ONCE.match(stripped):
            continue

        if "Macro Hygiene: Undefine private internal configuration macros" in line:
            in_macro_cleanup = True

        m_ext = RE_INCLUDE_EXTERNAL.match(stripped)
        if m_ext:
            header_name = m_ext.group(1)
            if not in_macro_cleanup:
                if header_name not in gmf_headers:
                    gmf_headers.append(header_name)
                continue

        m_ns_detail = RE_NAMESPACE_NUMERIC_DETAIL.match(line)
        if m_ns_detail:
            indent = m_ns_detail.group("indent")
            rest = m_ns_detail.group("rest")
            body_lines.append(f"{indent}export namespace numeric::detail{rest}")
            continue

        m_ns = RE_NAMESPACE_NUMERIC.match(line)
        if m_ns:
            indent = m_ns.group("indent")
            rest = m_ns.group("rest")
            body_lines.append(f"{indent}export namespace numeric{rest}")
            continue

        body_lines.append(line)

    ixx_lines: list[str] = []
    ixx_lines.append("module;")
    ixx_lines.append("")
    ixx_lines.append("// ============================================================================")
    ixx_lines.append("// Global Module Fragment (GMF): Preprocessor directives & standard includes")
    ixx_lines.append("// ============================================================================")

    std_c = sorted([x for x in gmf_headers if x.startswith("c") and not x.startswith("compare")])
    std_cpp = sorted([x for x in gmf_headers if x not in std_c])

    for h in std_c:
        ixx_lines.append(f"#include <{h}>")
    for h in std_cpp:
        ixx_lines.append(f"#include <{h}>")

    ixx_lines.append("")
    ixx_lines.append("// ============================================================================")
    ixx_lines.append("// Module Purview")
    ixx_lines.append("// ============================================================================")
    ixx_lines.append("export module bigint;")
    ixx_lines.append("")

    while body_lines and not body_lines[0].strip():
        body_lines.pop(0)

    ixx_lines.extend(body_lines)

    ixx_output.parent.mkdir(parents=True, exist_ok=True)
    body = "\n".join(ixx_lines) + "\n"
    output_bytes = b"\xef\xbb\xbf" + body.encode("utf-8")

    ixx_output.write_bytes(output_bytes)
    return ixx_output


def main():
    parser = argparse.ArgumentParser(description="Export CPP-BigInt single header into C++20 Module Interface Unit.")
    parser.add_argument("-i", "--input", help="Input single-header file path (default: dist/bigint.hpp)")
    parser.add_argument("-o", "--output", help="Output module file path (default: dist/bigint.ixx)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print verbose export information")
    args = parser.parse_args()

    repo_root = find_repo_root(Path(__file__).resolve().parent)
    hpp_file = Path(args.input).resolve() if args.input else (repo_root / "dist" / "bigint.hpp")
    ixx_file = Path(args.output).resolve() if args.output else (repo_root / "dist" / "bigint.ixx")

    try:
        res = export_cxx20_module(repo_root, hpp_file, ixx_file, verbose=args.verbose)
        line_count = len(res.read_text(encoding="utf-8-sig").splitlines())
        byte_size = res.stat().st_size
        print(f"[SUCCESS] C++20 Module Interface Unit generated: {res} ({line_count} lines, {byte_size} bytes, UTF-8 BOM verified)")
    except Exception as e:
        print(f"[ERROR] Failed to export C++20 module: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
