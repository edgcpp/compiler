#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Builds, runs, and verifies 100.00% function, line, region, and branch
coverage across src/edg_be_common.h, src/edg_be_sdk.{h,cpp}, and
src/edg_be_driver.{h,cpp}."""

import glob
import os
import shutil
import subprocess
import sys

from pathlib import Path
from typing import Optional


def resolve_llvm_tool(tool_name: str) -> Optional[str]:
    """Resolves clang++, llvm-profdata, or llvm-cov across Homebrew, macOS, and Linux."""
    prefixes = [
        "/opt/homebrew/opt/llvm/bin",
        "/usr/local/opt/llvm/bin",
        "/usr/lib/llvm-21/bin",
        "/usr/lib/llvm-20/bin",
        "/usr/lib/llvm-19/bin",
        "/usr/lib/llvm-18/bin",
    ]
    for p in prefixes:
        cand = os.path.join(p, tool_name)
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    found = shutil.which(tool_name)
    if found:
        return found
    if sys.platform == "darwin":
        xcrun = shutil.which("xcrun")
        if xcrun:
            res = subprocess.run(
                [xcrun, "--find", tool_name],
                capture_output=True,
                text=True,
                check=False,
            )
            if res.returncode == 0 and res.stdout.strip():
                return res.stdout.strip()
    return None


def main() -> int:
    """Configures a standalone coverage build, executes test_edg_be_driver, and verifies 0 missed branches."""
    repo_root = Path(__file__).resolve().parent.parent
    os.chdir(repo_root)

    clangxx = resolve_llvm_tool("clang++")
    clang = resolve_llvm_tool("clang")
    llvm_profdata = resolve_llvm_tool("llvm-profdata")
    llvm_cov = resolve_llvm_tool("llvm-cov")
    if not clangxx or not clang or not llvm_profdata or not llvm_cov:
        print(
            "Error: clang, clang++, llvm-profdata, and llvm-cov are required.",
            file=sys.stderr,
        )
        return 1

    build_dir = Path(
        os.environ.get("EDG_DRIVER_COV_BUILD_DIR", ".tmp/agent-stage0/driver-cov-build")
    )
    prof_dir = build_dir / "profiles"
    if prof_dir.exists():
        shutil.rmtree(prof_dir)
    prof_dir.mkdir(parents=True, exist_ok=True)

    cmake_cmd = [
        "cmake",
        "-S",
        ".",
        "-B",
        str(build_dir),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_C_COMPILER={clang}",
        f"-DCMAKE_CXX_COMPILER={clangxx}",
        "-DEDG_ENABLE_LLVM_BACKEND=OFF",
        "-DEDG_ENABLE_GCC_BACKEND=OFF",
        "-DEDG_ENABLE_C_BACKEND=ON",
        "-DBUILD_TESTING=ON",
        "-DEDG_DRIVER_ENABLE_COVERAGE=ON",
    ]
    subprocess.run(cmake_cmd, check=True)
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "test_edg_be_driver"],
        check=True,
    )

    test_bin = build_dir / "tests/driver/test_edg_be_driver"
    if not test_bin.exists():
        test_bin = build_dir / "bin/test_edg_be_driver"

    env = os.environ.copy()
    env["LLVM_PROFILE_FILE"] = str(prof_dir / "driver-%p.profraw")
    subprocess.run([str(test_bin)], env=env, check=True)

    profraws = sorted(glob.glob(str(prof_dir / "*.profraw")))
    if not profraws:
        print("Error: No .profraw files generated!", file=sys.stderr)
        return 1

    profdata = build_dir / "driver_merged.profdata"
    subprocess.run(
        [llvm_profdata, "merge", "-sparse", *profraws, "-o", str(profdata)], check=True
    )

    target_sources = sorted(glob.glob("src/edg_be_*.h") + glob.glob("src/edg_be_*.cpp"))
    rep = subprocess.run(
        [
            llvm_cov,
            "report",
            f"-instr-profile={profdata}",
            str(test_bin),
            *target_sources,
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    print(rep.stdout)

    for line in rep.stdout.splitlines():
        if line.startswith("TOTAL"):
            parts = line.split()
            missed_regions = int(parts[2])
            missed_funcs = int(parts[5])
            missed_lines = int(parts[8])
            missed_branches = int(parts[11])
            if (
                missed_regions != 0
                or missed_funcs != 0
                or missed_lines != 0
                or missed_branches != 0
            ):
                show_res = subprocess.run(
                    [
                        llvm_cov,
                        "show",
                        f"-instr-profile={profdata}",
                        "--show-branches=count",
                        "--show-line-counts-or-regions",
                        str(test_bin),
                        *target_sources,
                    ],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                (build_dir / "uncovered.txt").write_text(
                    show_res.stdout, encoding="utf-8"
                )
                print(
                    f"Coverage failed: missed_funcs={missed_funcs}, missed_lines={missed_lines}, "
                    f"missed_regions={missed_regions}, missed_branches={missed_branches}",
                    file=sys.stderr,
                )
                return 1
            print(
                "100.00% Function, Line, Region, and Branch Coverage Verified on Stage-0 Driver & SDK!"
            )
            return 0

    print("Error: Failed to parse TOTAL line from llvm-cov report.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
