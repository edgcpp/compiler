#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Verifies 100% real compiler-instrumented coverage for the GCC backend."""

import argparse
import glob
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Dict, List, Optional, Set, Tuple


def _resolve_llvm_tool(tool_name: str) -> Optional[str]:
    """Resolves llvm-profdata or llvm-cov across PATH, xcrun, and Homebrew."""
    direct = shutil.which(tool_name)
    if direct:
        return direct
    if sys.platform == "darwin":
        res = subprocess.run(
            ["xcrun", "-f", tool_name], capture_output=True, text=True, check=False
        )
        if res.returncode == 0 and res.stdout.strip():
            return res.stdout.strip()
    for prefix in (
        "/opt/homebrew/opt/llvm/bin",
        "/usr/local/opt/llvm/bin",
        "/usr/lib/llvm-18/bin",
        "/usr/lib/llvm-17/bin",
        "/usr/lib/llvm-16/bin",
    ):
        candidate = Path(prefix) / tool_name
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return str(candidate)
    return None


import os


def _expected_backend_sources(project_root: Path) -> List[Path]:
    """Returns all backend and EH runtime implementation units that must hit 100%."""
    sources: List[Path] = sorted(
        Path(p).resolve()
        for p in glob.glob(str(project_root / "src" / "gcc_gen_be_*.c"))
    )
    for rt_ext in ("c", "cpp"):
        rt_path = project_root / "lib_src" / f"gcc_be_eh_rt.{rt_ext}"
        if rt_path.is_file():
            sources.append(rt_path.resolve())
    return sources


def _find_instrumented_binaries(build_dir: Path) -> List[Path]:
    """Finds compiled test/backend binaries inside build_dir with LLVM covmap."""
    candidates: List[Path] = []
    for pattern in (
        str(build_dir / "bin" / "test_*"),
        str(build_dir / "tests" / "gcc_be" / "test_*"),
        str(build_dir / "bin" / "cpfe"),
    ):
        for raw in sorted(glob.glob(pattern)):
            path = Path(raw)
            if (
                path.is_file()
                and os.access(path, os.X_OK)
                and path.suffix in ("", ".exe")
            ):
                candidates.append(path)
    seen: Set[Path] = set()
    unique: List[Path] = []
    for cand in candidates:
        resolved = cand.resolve()
        if resolved not in seen:
            seen.add(resolved)
            unique.append(resolved)
    return unique


def _verify_llvm_coverage(
    project_root: Path,
    build_dir: Path,
    scratch_dir: Path,
    profraw_files: List[Path],
    profdata_path: Optional[Path],
) -> int:
    """Verifies 100% function, line, region, and branch coverage via llvm-cov."""
    llvm_profdata = _resolve_llvm_tool("llvm-profdata")
    llvm_cov = _resolve_llvm_tool("llvm-cov")
    if not llvm_profdata or not llvm_cov:
        print(
            "ERROR: llvm-profdata / llvm-cov not found for Clang coverage check.",
            file=sys.stderr,
        )
        return 1

    merged_profdata = scratch_dir / "merged.profdata"
    if profraw_files:
        merge_cmd = [
            llvm_profdata,
            "merge",
            "-sparse",
            *[str(p) for p in profraw_files],
            "-o",
            str(merged_profdata),
        ]
        merge_res = subprocess.run(
            merge_cmd, capture_output=True, text=True, check=False
        )
        if merge_res.returncode != 0:
            print("ERROR: llvm-profdata merge failed:", file=sys.stderr)
            print(merge_res.stderr, file=sys.stderr)
            return 1
    elif profdata_path is not None and profdata_path.is_file():
        merged_profdata = profdata_path
    else:
        print("ERROR: No .profraw or .profdata files available.", file=sys.stderr)
        return 1

    all_bins = _find_instrumented_binaries(build_dir)
    if not all_bins:
        print(
            f"ERROR: No instrumented test binaries found under {build_dir}.",
            file=sys.stderr,
        )
        return 1

    expected_sources = _expected_backend_sources(project_root)
    if not expected_sources:
        print("ERROR: No backend source files found.", file=sys.stderr)
        return 1

    # Filter binaries to those containing __llvm_covmap instrumentation.
    valid_bins: List[Path] = []
    for bin_path in all_bins:
        probe = subprocess.run(
            [
                llvm_cov,
                "report",
                str(bin_path),
                f"-instr-profile={merged_profdata}",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if probe.returncode == 0:
            valid_bins.append(bin_path)

    if not valid_bins:
        print(
            f"ERROR: None of the binaries in {build_dir} carry -fcoverage-mapping!",
            file=sys.stderr,
        )
        return 1

    obj_args: List[str] = [str(valid_bins[0])]
    for extra_bin in valid_bins[1:]:
        obj_args.extend(["-object", str(extra_bin)])

    report_cmd = [
        llvm_cov,
        "report",
        *obj_args,
        f"-instr-profile={merged_profdata}",
        *[str(s) for s in expected_sources],
    ]
    report = subprocess.run(report_cmd, capture_output=True, text=True, check=False)
    if report.returncode != 0:
        print("ERROR: llvm-cov report failed:", file=sys.stderr)
        print(report.stderr, file=sys.stderr)
        return 1

    print(report.stdout)

    covered_basenames: Set[str] = set()
    total_parsed = False
    failed = False

    for raw_line in report.stdout.splitlines():
        parts = raw_line.split()
        if not parts:
            continue
        if parts[0] == "TOTAL":
            if len(parts) < 13:
                print(
                    f"ERROR: Unexpected TOTAL row format: {raw_line}", file=sys.stderr
                )
                return 1
            total_parsed = True
            missed_regions = int(parts[2])
            missed_funcs = int(parts[5])
            missed_lines = int(parts[8])
            missed_branches = int(parts[11])
            total_funcs = int(parts[4])
            total_lines = int(parts[7])
            if total_funcs == 0 or total_lines == 0:
                print(
                    "ERROR: Zero instrumented functions/lines in llvm-cov report!",
                    file=sys.stderr,
                )
                return 1
            if (
                missed_regions != 0
                or missed_funcs != 0
                or missed_lines != 0
                or missed_branches != 0
            ):
                print(
                    f"ERROR: Coverage incomplete! Missed regions={missed_regions}, "
                    f"funcs={missed_funcs}, lines={missed_lines}, "
                    f"branches={missed_branches}",
                    file=sys.stderr,
                )
                failed = True
        elif parts[0].endswith((".c", ".cpp")):
            covered_basenames.add(Path(parts[0]).name)

    missing_units = [
        src.name for src in expected_sources if src.name not in covered_basenames
    ]
    if missing_units:
        print(
            f"ERROR: Backend source units missing from coverage report: "
            f"{', '.join(missing_units)}",
            file=sys.stderr,
        )
        failed = True

    if not total_parsed:
        print(
            "ERROR: Could not parse TOTAL line from llvm-cov report.", file=sys.stderr
        )
        return 1

    if failed:
        show_cmd = [
            llvm_cov,
            "show",
            *obj_args,
            f"-instr-profile={merged_profdata}",
            "--show-branches=count",
            "--show-line-counts-or-regions",
            *[str(s) for s in expected_sources],
        ]
        show_res = subprocess.run(show_cmd, capture_output=True, text=True, check=False)
        shown = 0
        for sline in show_res.stdout.splitlines():
            if "|      0|" in sline or "[True: 0" in sline or "False: 0]" in sline:
                print(sline, file=sys.stderr)
                shown += 1
                if shown >= 80:
                    break
        return 1

    print(
        "100.00% Function, Line, Region, and Branch Coverage verified "
        f"across {len(expected_sources)} backend units."
    )
    return 0


def _verify_lcov_coverage(
    project_root: Path, build_dir: Path, scratch_dir: Path, gcda_files: List[Path]
) -> int:
    """Verifies 100% function, line, and branch coverage via lcov on .gcda data."""
    if not gcda_files:
        print("ERROR: No .gcda coverage files found.", file=sys.stderr)
        return 1

    lcov_bin = shutil.which("lcov")
    if not lcov_bin:
        print("ERROR: lcov binary not found in PATH.", file=sys.stderr)
        return 1

    raw_info = scratch_dir / "coverage_raw.info"
    cap_res = subprocess.run(
        [
            lcov_bin,
            "--capture",
            "--directory",
            str(build_dir),
            "--output-file",
            str(raw_info),
            "--rc",
            "branch_coverage=1",
            "--quiet",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if cap_res.returncode != 0 or not raw_info.is_file():
        print("ERROR: lcov --capture failed:", file=sys.stderr)
        print(cap_res.stderr, file=sys.stderr)
        return 1

    expected_sources = _expected_backend_sources(project_root)
    expected_set: Set[Path] = set(expected_sources)
    seen_sources: Set[Path] = set()

    funcs_found = 0
    funcs_hit = 0
    lines_found = 0
    lines_hit = 0
    branches_found = 0
    branches_hit = 0
    uncovered_details: List[str] = []
    active_sf: Optional[Path] = None

    for raw_line in raw_info.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if line.startswith("SF:"):
            cand = Path(line[3:]).resolve()
            if cand in expected_set:
                active_sf = cand
                seen_sources.add(cand)
            else:
                active_sf = None
        elif line == "end_of_record":
            active_sf = None
        elif active_sf is not None:
            if line.startswith("FNDA:"):
                parts = line[5:].split(",", 1)
                if len(parts) == 2 and int(parts[0]) == 0:
                    uncovered_details.append(
                        f"UNHIT FUNCTION: {active_sf.name}:{parts[1]}"
                    )
            elif line.startswith("DA:"):
                parts = line[3:].split(",")
                if len(parts) >= 2 and int(parts[1]) == 0:
                    uncovered_details.append(
                        f"UNCOVERED LINE: {active_sf.name}:{parts[0]}"
                    )
            elif line.startswith("BRDA:"):
                parts = line[5:].split(",")
                if len(parts) >= 4 and (parts[3] == "-" or int(parts[3]) == 0):
                    uncovered_details.append(
                        f"UNHIT BRANCH: {active_sf.name}:{parts[0]} "
                        f"(block {parts[1]}, branch {parts[2]})"
                    )
            elif line.startswith("FNF:"):
                funcs_found += int(line[4:])
            elif line.startswith("FNH:"):
                funcs_hit += int(line[4:])
            elif line.startswith("LF:"):
                lines_found += int(line[3:])
            elif line.startswith("LH:"):
                lines_hit += int(line[3:])
            elif line.startswith("BRF:"):
                branches_found += int(line[4:])
            elif line.startswith("BRH:"):
                branches_hit += int(line[4:])

    missing = [s.name for s in expected_sources if s not in seen_sources]
    if missing:
        print(
            f"ERROR: Backend source units missing .gcda instrumentation: "
            f"{', '.join(missing)}",
            file=sys.stderr,
        )
        return 1

    if funcs_found == 0 or lines_found == 0:
        print(
            "ERROR: Zero functions or lines recorded in coverage.info!", file=sys.stderr
        )
        return 1

    func_cov = (funcs_hit / funcs_found) * 100.0
    line_cov = (lines_hit / lines_found) * 100.0
    branch_cov = (branches_hit / branches_found) * 100.0 if branches_found > 0 else 100.0

    print(f"Function Coverage: {func_cov:.2f}% ({funcs_hit}/{funcs_found})")
    print(f"Line Coverage:     {line_cov:.2f}% ({lines_hit}/{lines_found})")
    print(f"Branch Coverage:   {branch_cov:.2f}% ({branches_hit}/{branches_found})")

    for detail in uncovered_details[:80]:
        print(detail, file=sys.stderr)

    if (
        funcs_hit != funcs_found
        or lines_hit != lines_found
        or branches_hit != branches_found
        or uncovered_details
    ):
        print("ERROR: Coverage dropped below 100.00%!", file=sys.stderr)
        return 1

    print(
        "100.00% Function, Line, and Branch Coverage verified "
        f"across {len(expected_sources)} backend units."
    )
    return 0


def check_coverage(build_dir_arg: Optional[str] = None) -> int:
    """Entry logic verifying real compiler-instrumented 100% coverage."""
    project_root = Path(__file__).resolve().parent.parent.parent
    default_build = os.environ.get(
        "EDG_GCC_BE_BUILD_DIR", str(project_root / "build" / "gcc-agent")
    )
    build_dir = Path(build_dir_arg or default_build).resolve()

    scratch_base = project_root / ".tmp" / "agent-gcc"
    profraw_dir = scratch_base / "profraw"
    scratch_cov_dir = scratch_base / "coverage"
    scratch_cov_dir.mkdir(parents=True, exist_ok=True)

    try:
        profraw_files: List[Path] = []
        if profraw_dir.is_dir():
            profraw_files.extend(sorted(profraw_dir.glob("*.profraw")))
        if build_dir.is_dir():
            profraw_files.extend(
                sorted(
                    Path(p)
                    for p in glob.glob(
                        str(build_dir / "**" / "*.profraw"), recursive=True
                    )
                )
            )

        # Deduplicate profraw paths.
        seen_pr: Set[Path] = set()
        unique_profraw: List[Path] = []
        for pr in profraw_files:
            rp = pr.resolve()
            if rp.is_file() and rp not in seen_pr:
                seen_pr.add(rp)
                unique_profraw.append(rp)

        existing_profdata = scratch_cov_dir / "merged.profdata"
        if unique_profraw or existing_profdata.is_file():
            return _verify_llvm_coverage(
                project_root,
                build_dir,
                scratch_cov_dir,
                unique_profraw,
                existing_profdata if existing_profdata.is_file() else None,
            )

        gcda_files: List[Path] = []
        if build_dir.is_dir():
            gcda_files = sorted(
                Path(p).resolve()
                for p in glob.glob(str(build_dir / "**" / "*.gcda"), recursive=True)
            )
        if gcda_files:
            return _verify_lcov_coverage(
                project_root, build_dir, scratch_cov_dir, gcda_files
            )

        print(
            "ERROR: No real compiler-instrumented coverage data (.profraw/.profdata "
            f"or .gcda) found in {profraw_dir} or {build_dir}!",
            file=sys.stderr,
        )
        return 1
    finally:
        for tmp_artifact in ("coverage_raw.info", "merged.profdata"):
            artifact_path = scratch_cov_dir / tmp_artifact
            if artifact_path.exists():
                artifact_path.unlink()


def main() -> int:
    """CLI entry point for GCC backend coverage verification."""
    parser = argparse.ArgumentParser(
        description="Verify 100% instrumented coverage for the EDG GCC backend."
    )
    parser.add_argument(
        "--build-dir",
        dest="build_dir",
        default=None,
        help="Path to the instrumented CMake build directory.",
    )
    args = parser.parse_args()
    return check_coverage(args.build_dir)


if __name__ == "__main__":
    sys.exit(main())
