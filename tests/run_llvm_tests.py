#!/usr/bin/env python3
"""Runs all LLVM backend unit/integration tests and enforces 100% llvm-cov coverage."""

import glob
import os
import re
import subprocess
import sys


def main() -> None:
    """Builds and runs LLVM backend tests and verifies 100.00% coverage via llvm-cov."""
    print("Running LLVM Backend Tests and Real llvm-cov Coverage Analysis...")

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    cov_script = os.path.join(root_dir, "tests", "test_llvm_coverage.sh")

    res = subprocess.run(["bash", cov_script], cwd=root_dir, capture_output=True, text=True)
    if res.returncode != 0:
        print(res.stdout)
        print(res.stderr, file=sys.stderr)
        sys.exit(res.returncode)

    print(res.stdout)

    m = re.search(
        r"^TOTAL\s+\d+\s+\d+\s+([0-9.]+%)\s+\d+\s+\d+\s+([0-9.]+%)\s+\d+\s+\d+\s+([0-9.]+%)\s+\d+\s+\d+\s+([0-9.]+%)",
        res.stdout,
        re.MULTILINE,
    )
    if not m:
        print("Error: Could not parse TOTAL coverage line from llvm-cov report.", file=sys.stderr)
        sys.exit(1)

    region_cov, func_cov, line_cov, branch_cov = m.group(1), m.group(2), m.group(3), m.group(4)
    print(f"Region coverage: {region_cov}")
    print(f"Function coverage: {func_cov}")
    print(f"Line coverage: {line_cov}")
    print(f"Branch coverage: {branch_cov}")

    if (region_cov, func_cov, line_cov, branch_cov) != (
        "100.00%",
        "100.00%",
        "100.00%",
        "100.00%",
    ):
        print("Error: 100.00% coverage requirement not met!", file=sys.stderr)
        sys.exit(1)

    print("100% Real llvm-cov Test Coverage Verified.")
    sys.exit(0)


if __name__ == "__main__":
    main()
