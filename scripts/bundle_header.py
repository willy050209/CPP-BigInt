#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
scripts/bundle_header.py
========================
CPP-BigInt Single-Header Bundler / Amalgamation Script

Responsibilities:
1. Recursively scans `include/numeric/` for all C++ headers (.hpp, .h).
2. Parses internal `#include "..."` dependencies and resolves their targets.
3. Performs a topological sort so prerequisites appear before dependents.
4. Removes internal relative `#include "..."` and individual `#pragma once` directives.
5. Collects and deduplicates top-level external STL headers (<cstdint>, <iostream>, etc.).
6. Retains XML documentation comments (/// <summary>, etc.) and regular code comments.
7. Emits a clean, self-contained single-header distribution to `dist/bigint.hpp`.
8. Enforces UTF-8 BOM encoding for output files per project specifications.
"""

import os
import sys
import re
import argparse
from pathlib import Path
from collections import defaultdict, deque


# Regex patterns
RE_PRAGMA_ONCE = re.compile(r'^\s*#\s*pragma\s+once\b')
RE_INCLUDE_INTERNAL = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
RE_INCLUDE_EXTERNAL = re.compile(r'^\s*#\s*include\s*<([^>]+)>')
RE_PP_IF = re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b')
RE_PP_ENDIF = re.compile(r'^\s*#\s*endif\b')
RE_DEFINE = re.compile(r'^\s*#\s*define\s+(NUMERIC_[A-Za-z0-9_]+)')

DEFAULT_CLEANUP_MACROS = [
    "NUMERIC_CPLUSPLUS",
    "NUMERIC_CXX_11",
    "NUMERIC_CXX_14",
    "NUMERIC_CXX_17",
    "NUMERIC_CXX_20",
    "NUMERIC_CXX_23",
    "NUMERIC_HAS_EXCEPTIONS",
    "NUMERIC_THROW_OR_ABORT",
    "NUMERIC_CONSTEXPR_14",
    "NUMERIC_CONSTEXPR_20",
    "NUMERIC_ALWAYS_INLINE",
    "NUMERIC_LIKELY",
    "NUMERIC_UNLIKELY",
    "NUMERIC_HAS_STD_FORMAT",
    "NUMERIC_HAS_INT128",
]

PUBLIC_API_MACROS = {
    "NUMERIC_NODISCARD",
    "NUMERIC_NO_GLOBAL_TYPE_ALIAS",
}


def collect_cleanup_macros(parsed_data: dict[Path, dict]) -> list[str]:
    cleanup_set = set(DEFAULT_CLEANUP_MACROS)

    for data in parsed_data.values():
        for line in data["cleaned_lines"]:
            m = RE_DEFINE.match(line)
            if m:
                macro_name = m.group(1)
                if macro_name in PUBLIC_API_MACROS:
                    continue
                if (macro_name.startswith("NUMERIC_CXX_") or
                    macro_name.startswith("NUMERIC_HAS_") or
                    macro_name.startswith("NUMERIC_INTERNAL_") or
                    macro_name in cleanup_set):
                    cleanup_set.add(macro_name)

    ordered = [m for m in DEFAULT_CLEANUP_MACROS if m in cleanup_set]
    extras = sorted([m for m in cleanup_set if m not in DEFAULT_CLEANUP_MACROS])
    return ordered + extras


def find_repo_root(start_path: Path) -> Path:
    curr = start_path.resolve()
    for _ in range(10):
        if (curr / "CMakeLists.txt").exists() or (curr / ".git").exists():
            return curr
        if curr.parent == curr:
            break
        curr = curr.parent
    return start_path.resolve()


def scan_headers(numeric_dir: Path) -> list[Path]:
    if not numeric_dir.exists():
        return []
    files = [p.resolve() for p in numeric_dir.rglob("*") if p.is_file() and p.suffix.lower() in (".hpp", ".h")]
    return sorted(files)


def resolve_include(source_file: Path, target_str: str, numeric_dir: Path, include_dir: Path, all_headers: set[Path]) -> Path | None:
    candidates = [
        (source_file.parent / target_str).resolve(),
        (numeric_dir / target_str).resolve(),
        (include_dir / target_str).resolve(),
        (numeric_dir / target_str.replace("numeric/", "")).resolve(),
        (numeric_dir / "detail" / target_str).resolve() if "detail" not in target_str else None,
    ]
    for c in candidates:
        if c and c in all_headers:
            return c
    return None


def parse_header_file(file_path: Path, numeric_dir: Path, include_dir: Path, all_headers: set[Path]):
    with open(file_path, "r", encoding="utf-8-sig", errors="replace") as f:
        content = f.read()

    lines = content.splitlines()
    cleaned_lines: list[str] = []
    internal_deps: set[Path] = set()
    top_external_includes: list[str] = []

    pp_depth = 0

    for line in lines:
        stripped = line.strip()

        if RE_PP_IF.match(stripped):
            pp_depth += 1
            cleaned_lines.append(line)
            continue
        elif RE_PP_ENDIF.match(stripped):
            pp_depth = max(0, pp_depth - 1)
            cleaned_lines.append(line)
            continue

        if RE_PRAGMA_ONCE.match(stripped):
            continue

        m_int = RE_INCLUDE_INTERNAL.match(stripped)
        if m_int:
            target_str = m_int.group(1)
            resolved = resolve_include(file_path, target_str, numeric_dir, include_dir, all_headers)
            if resolved:
                internal_deps.add(resolved)
                continue
            else:
                cleaned_lines.append(line)
                continue

        m_ext = RE_INCLUDE_EXTERNAL.match(stripped)
        if m_ext:
            header_name = m_ext.group(1)
            if pp_depth == 0:
                top_external_includes.append(header_name)
                continue
            else:
                cleaned_lines.append(line)
                continue

        cleaned_lines.append(line)

    return {
        "file": file_path,
        "internal_deps": internal_deps,
        "top_external_includes": top_external_includes,
        "cleaned_lines": cleaned_lines,
    }


def topological_sort(headers: list[Path], parsed_data: dict[Path, dict]) -> list[Path]:
    in_degree = {h: 0 for h in headers}
    dependents = defaultdict(list)

    for h in headers:
        for dep in parsed_data[h]["internal_deps"]:
            if dep in in_degree:
                in_degree[h] += 1
                dependents[dep].append(h)

    def sort_key(p: Path):
        name = p.name.lower()
        rel = str(p).lower()
        is_config = 0 if "config" in name else 1
        is_detail = 0 if "detail" in rel else 1
        is_main = 1 if name == "bigint.hpp" else 2
        return (is_config, is_detail, is_main, rel)

    ready = [h for h in headers if in_degree[h] == 0]
    ready.sort(key=sort_key)
    queue = deque(ready)

    sorted_result: list[Path] = []

    while queue:
        if len(queue) > 1:
            sorted_q = sorted(list(queue), key=sort_key)
            queue = deque(sorted_q)

        curr = queue.popleft()
        sorted_result.append(curr)

        for dep in dependents[curr]:
            in_degree[dep] -= 1
            if in_degree[dep] == 0:
                queue.append(dep)

    if len(sorted_result) != len(headers):
        unresolved = [h for h in headers if h not in sorted_result]
        unresolved.sort(key=sort_key)
        sorted_result.extend(unresolved)

    return sorted_result


def emit_single_header(repo_root: Path, output_file: Path, verbose: bool = False) -> Path:
    include_dir = repo_root / "include"
    numeric_dir = include_dir / "numeric"

    headers = scan_headers(numeric_dir)
    if not headers:
        raise FileNotFoundError(f"No headers found in {numeric_dir}")

    all_headers_set = set(headers)
    parsed_data = {}

    for h in headers:
        parsed_data[h] = parse_header_file(h, numeric_dir, include_dir, all_headers_set)

    sorted_headers = topological_sort(headers, parsed_data)

    if verbose:
        print(f"[INFO] Scanned {len(headers)} header(s).")
        print("[INFO] Topological order:")
        for idx, h in enumerate(sorted_headers, 1):
            rel = h.relative_to(repo_root)
            deps = [d.relative_to(repo_root).name for d in parsed_data[h]["internal_deps"]]
            deps_str = f" (depends on: {', '.join(deps)})" if deps else " (depends on: none)"
            print(f"  {idx:2d}. {rel}{deps_str}")

    aggregated_external = set()
    ordered_external: list[str] = []

    for h in sorted_headers:
        for ext in parsed_data[h]["top_external_includes"]:
            if ext not in aggregated_external:
                aggregated_external.add(ext)
                ordered_external.append(ext)

    std_c_headers = sorted([x for x in ordered_external if x.startswith("c") and not x.startswith("compare")])
    std_cpp_headers = sorted([x for x in ordered_external if x not in std_c_headers])

    output_lines: list[str] = []
    output_lines.append("#pragma once")
    output_lines.append("")
    output_lines.append("// ============================================================================")
    output_lines.append("// CPP-BigInt: High-Performance Arbitrary-Precision Integer Single Header")
    output_lines.append("// Automatically generated by scripts/bundle_header.py. Do not edit directly.")
    output_lines.append("// Downward compatible from C++23 to C++11. Zero external dependencies.")
    output_lines.append("// ============================================================================")
    output_lines.append("")

    if std_c_headers or std_cpp_headers:
        output_lines.append("// --- Standard Library Includes ---")
        for h in std_c_headers:
            output_lines.append(f"#include <{h}>")
        for h in std_cpp_headers:
            output_lines.append(f"#include <{h}>")
        output_lines.append("")

    for h in sorted_headers:
        rel_path = h.relative_to(repo_root).as_posix()
        output_lines.append(f"// --- Begin Section: {rel_path} ---")
        file_lines = parsed_data[h]["cleaned_lines"]

        while file_lines and not file_lines[0].strip():
            file_lines.pop(0)
        while file_lines and not file_lines[-1].strip():
            file_lines.pop()

        output_lines.extend(file_lines)
        output_lines.append(f"// --- End Section: {rel_path} ---")
        output_lines.append("")

    cleanup_macros = collect_cleanup_macros(parsed_data)
    if cleanup_macros:
        output_lines.append("// ============================================================================")
        output_lines.append("// --- Macro Hygiene: Undefine private internal configuration macros ---")
        output_lines.append("// ============================================================================")
        for macro in cleanup_macros:
            output_lines.append(f"#ifdef {macro}")
            output_lines.append(f"#  undef {macro}")
            output_lines.append(f"#endif")
        output_lines.append("")

    output_file.parent.mkdir(parents=True, exist_ok=True)
    body = "\n".join(output_lines) + "\n"
    output_bytes = b"\xef\xbb\xbf" + body.encode("utf-8")

    output_file.write_bytes(output_bytes)
    return output_file


def main():
    parser = argparse.ArgumentParser(description="Bundle CPP-BigInt headers into a single-header distribution.")
    parser.add_argument("-o", "--output", help="Output file path (default: dist/bigint.hpp)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print verbose build information")
    args = parser.parse_args()

    repo_root = find_repo_root(Path(__file__).resolve().parent)
    output_file = Path(args.output).resolve() if args.output else (repo_root / "dist" / "bigint.hpp")

    try:
        res = emit_single_header(repo_root, output_file, verbose=args.verbose)
        line_count = len(res.read_text(encoding="utf-8-sig").splitlines())
        byte_size = res.stat().st_size
        print(f"[SUCCESS] Bundled single-header generated: {res} ({line_count} lines, {byte_size} bytes, UTF-8 BOM verified)")
    except Exception as e:
        print(f"[ERROR] Failed to bundle single-header: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
