#!/usr/bin/env python3
"""Builds, runs, and verifies 100% function, line, region, and branch coverage on the EDG LLVM backend."""

import glob
import os
import re
import shutil
import subprocess
import sys


def resolve_llvm_tool(tool_name: str) -> list[str]:
    """Resolves llvm-profdata or llvm-cov across macOS (xcrun) and Linux."""
    env_override = os.environ.get(tool_name.upper().replace("-", "_"))
    if env_override:
        return [env_override]
    if sys.platform == "darwin" and shutil.which("xcrun"):
        return ["xcrun", tool_name]
    if shutil.which(tool_name):
        return [tool_name]
    for ver in range(22, 13, -1):
        cand = f"{tool_name}-{ver}"
        if shutil.which(cand):
            return [cand]
    return [tool_name]


def main() -> int:
    """Runs all test_llvm_* binaries, merges profiles, and asserts 100% llvm-cov coverage."""
    default_build = ".tmp/build_tests" if os.path.isdir(".tmp/build_tests/bin") else "build/coverage"
    build_dir = os.environ.get("EDG_COVERAGE_BUILD_DIR", default_build)
    bin_dir = os.path.join(build_dir, "bin")
    profraw_dir = os.path.join(build_dir, "profraw")
    merged_prof = os.path.join(build_dir, "merged.profdata")

    cmake_cache = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cmake_cache):
        cmake_args = [
            "cmake",
            "-S",
            ".",
            "-B",
            build_dir,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DEDG_ENABLE_LLVM_BACKEND=ON",
            "-DBUILD_TESTING=ON",
            "-DEDG_LLVM_BE_COVERAGE=ON",
            "-DEDG_BUILD_LIB=OFF",
        ]
        if "LLVM_DIR" in os.environ:
            cmake_args.append(f"-DLLVM_DIR={os.environ['LLVM_DIR']}")
        elif os.path.isdir("/opt/homebrew/opt/llvm/lib/cmake/llvm"):
            cmake_args.append("-DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm")
        if shutil.which("clang") and shutil.which("clang++"):
            cmake_args.extend(["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"])
        subprocess.run(cmake_args, check=True)

    llvm_targets: list[str] = []
    tests_cmake = os.path.join("tests", "CMakeLists.txt")
    if os.path.isfile(tests_cmake):
        with open(tests_cmake, encoding="utf-8") as cmake_f:
            llvm_targets = sorted(
                set(
                    re.findall(
                        r"add_executable\((test_llvm_[A-Za-z0-9_]+)", cmake_f.read()
                    )
                )
            )
    if not llvm_targets:
        llvm_targets = [
            os.path.basename(b)
            for b in sorted(glob.glob(os.path.join(bin_dir, "test_llvm_*")))
        ]
    build_cmd = ["cmake", "--build", build_dir]
    if llvm_targets:
        build_targets = (
            ["cpfe", *llvm_targets]
            if "test_llvm_e2e_execution" in llvm_targets
            else llvm_targets
        )
        build_cmd.extend(["--target", *build_targets])
    subprocess.run(build_cmd, check=True)

    test_bins = sorted(glob.glob(os.path.join(bin_dir, "test_llvm_*")))

    if not test_bins:
        print(f"Error: No test_llvm_* binaries found in {bin_dir}", file=sys.stderr)
        return 1

    os.makedirs(profraw_dir, exist_ok=True)
    for old_prof in glob.glob(os.path.join(profraw_dir, "*.profraw")):
        os.remove(old_prof)

    for test_bin in test_bins:
        name = os.path.basename(test_bin)
        env = os.environ.copy()
        env["LLVM_PROFILE_FILE"] = os.path.join(profraw_dir, f"{name}-%p.profraw")
        res = subprocess.run(
            [test_bin],
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if res.returncode != 0:
            print(f"Error: Test binary failed: {test_bin}", file=sys.stderr)
            return 1

    profraw_files = sorted(glob.glob(os.path.join(profraw_dir, "*.profraw")))
    profdata_cmd = resolve_llvm_tool("llvm-profdata")
    subprocess.run(
        [*profdata_cmd, "merge", "-sparse", *profraw_files, "-o", merged_prof],
        check=True,
    )

    src_files = sorted(glob.glob("src/llvm_gen_be*.cpp") + glob.glob("src/llvm_gen_be*.h"))
    cov_cmd = resolve_llvm_tool("llvm-cov")
    cmd = [*cov_cmd, "report", test_bins[0]]
    for tb in test_bins[1:]:
        cmd.extend(["-object", tb])
    cmd.extend([f"-instr-profile={merged_prof}", "--sources", *src_files])

    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    print(proc.stdout)

    for line in proc.stdout.splitlines():
        if line.startswith("TOTAL"):
            parts = line.split()
            # TOTAL Regions Missed Cover Functions Missed Executed Lines Missed Cover Branches Missed Cover
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
                print(
                    f"Coverage check failed: missed_funcs={missed_funcs}, "
                    f"missed_lines={missed_lines}, missed_regions={missed_regions}, "
                    f"missed_branches={missed_branches}",
                    file=sys.stderr,
                )
                return 1
            print(
                "100.00% Function, Line, Region, and Branch Coverage Verified across all LLVM backend files!"
            )
            return 0

    print("Error: Could not parse TOTAL line from llvm-cov report", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
