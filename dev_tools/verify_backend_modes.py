#!/usr/bin/env python3
"""Verifies CMake backend option combinations and smoke-tests compiled cpfe binaries.

Supports verifying a single build directory (`--build-dir <dir> --llvm <0|1> --gcc <0|1>`)
as well as configuring, building, and smoke-testing all four backend modes
(`--all-modes`).
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def check_dump_configuration(
    cpfe_bin: str, expect_llvm: bool, expect_gcc: bool, expect_c: bool = True
) -> None:
    """Verifies that `cpfe --dump_configuration` reports expected BACK_END_IS_* macros."""
    proc = subprocess.run(
        [cpfe_bin, "--dump_configuration"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    out = proc.stdout
    expected_lines = [
        f"#define BACK_END_IS_C_GEN_BE {1 if expect_c else 0}",
        f"#define BACK_END_IS_LLVM_GEN_BE {1 if expect_llvm else 0}",
        f"#define BACK_END_IS_GCC_GEN_BE {1 if expect_gcc else 0}",
    ]
    for line in expected_lines:
        if line not in out:
            raise RuntimeError(
                f"Expected '{line}' in {cpfe_bin} --dump_configuration output, but not found."
            )


def smoke_test_binary(
    cpfe_bin: str, expect_llvm: bool, expect_gcc: bool, expect_c: bool = True
) -> None:
    """Runs a compilation smoke test on `cpfe_bin` for each enabled backend."""
    check_dump_configuration(cpfe_bin, expect_llvm, expect_gcc, expect_c)

    with tempfile.TemporaryDirectory() as tmpdir:
        src_file = os.path.join(tmpdir, "smoke.c")
        with open(src_file, "w", encoding="utf-8") as f:
            f.write("int edg_smoke_add(int a, int b) { return a + b; }\n")

        # 1. Front-end only check (--fe_only) works in all modes
        subprocess.run(
            [cpfe_bin, "--c", "--clear_flag=use_predefined_macro_file", "--fe_only", src_file],
            check=True,
        )

        # 2. If LLVM backend is enabled, emit LLVM IR
        if expect_llvm:
            ll_file = os.path.join(tmpdir, "smoke.ll")
            subprocess.run(
                [
                    cpfe_bin,
                    "--c",
                    "--clear_flag=use_predefined_macro_file",
                    "--llvm-be",
                    f"--gen_llvm_file_name={ll_file}",
                    src_file,
                ],
                check=True,
            )
            if not os.path.exists(ll_file) or os.path.getsize(ll_file) == 0:
                raise RuntimeError("LLVM backend failed to emit non-empty LLVM IR file.")

        # 3. If GCC backend is enabled, emit assembly or object via libgccjit
        if expect_gcc:
            has_gccjit_runtime = True
            if sys.platform == "win32":
                has_gccjit_runtime = bool(
                    shutil.which("libgccjit-0.dll")
                    or shutil.which("libgccjit.dll")
                )
            if has_gccjit_runtime:
                asm_file = os.path.join(tmpdir, "smoke.s")
                subprocess.run(
                    [
                        cpfe_bin,
                        "--c",
                        "--clear_flag=use_predefined_macro_file",
                        "--gcc-be",
                        "-S",
                        "-o",
                        asm_file,
                        src_file,
                    ],
                    check=True,
                )
                if not os.path.exists(asm_file) or os.path.getsize(asm_file) == 0:
                    raise RuntimeError("GCC backend failed to emit non-empty assembly file.")

        # 4. If C-generating backend is enabled, emit .int.c via --c-be
        if expect_c:
            gen_c_file = os.path.join(tmpdir, "smoke.int.c")
            subprocess.run(
                [
                    cpfe_bin,
                    "--c",
                    "--clear_flag=use_predefined_macro_file",
                    "--c-be",
                    f"--gen_c_file_name={gen_c_file}",
                    src_file,
                ],
                check=True,
            )
            if not os.path.exists(gen_c_file) or os.path.getsize(gen_c_file) == 0:
                raise RuntimeError(
                    "C-generating backend failed to emit non-empty .int.c file."
                )

        # 5. Verify disabled backends cleanly return non-zero
        if not expect_llvm:
            res_llvm = subprocess.run(
                [
                    cpfe_bin,
                    "--backend=llvm",
                    "-c",
                    src_file,
                    "-o",
                    os.path.join(tmpdir, "dis_llvm.o"),
                ],
                capture_output=True,
                text=True,
            )
            if res_llvm.returncode == 0:
                raise RuntimeError(
                    "Expected --backend=llvm to fail when LLVM backend is disabled."
                )
        if not expect_gcc:
            res_gcc = subprocess.run(
                [
                    cpfe_bin,
                    "--backend=gcc",
                    "-c",
                    src_file,
                    "-o",
                    os.path.join(tmpdir, "dis_gcc.o"),
                ],
                capture_output=True,
                text=True,
            )
            if res_gcc.returncode == 0:
                raise RuntimeError(
                    "Expected --backend=gcc to fail when GCC backend is disabled."
                )


def resolve_or_build_mode_dir(
    candidates: list[str],
    llvm_on: bool,
    gcc_on: bool,
    c_on: bool,
) -> str:
    """Finds an existing build directory from candidates (or configures candidates[0]) and builds cpfe."""
    chosen = candidates[0]
    for cand in candidates:
        if os.path.exists(os.path.join(cand, "CMakeCache.txt")):
            chosen = cand
            break
    if not os.path.exists(os.path.join(chosen, "CMakeCache.txt")):
        cfg_cmd = [
            "cmake",
            "-S",
            ".",
            "-B",
            chosen,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug",
            f"-DEDG_ENABLE_LLVM_BACKEND={'ON' if llvm_on else 'OFF'}",
            f"-DEDG_ENABLE_GCC_BACKEND={'ON' if gcc_on else 'OFF'}",
            f"-DEDG_ENABLE_C_BACKEND={'ON' if c_on else 'OFF'}",
            "-DEDG_CPFE_WARNINGS_ARE_ERRORS=ON",
            "-DBUILD_TESTING=OFF",
        ]
        if (
            llvm_on
            and sys.platform == "darwin"
            and os.path.isdir("/opt/homebrew/opt/llvm/lib/cmake/llvm")
        ):
            cfg_cmd.append("-DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm")
        subprocess.run(cfg_cmd, check=True)
    else:
        subprocess.run(
            ["cmake", chosen, "-DEDG_CPFE_WARNINGS_ARE_ERRORS=ON"],
            check=True,
        )
    subprocess.run(
        ["cmake", "--build", chosen, "--target", "cpfe", "-j", "8"], check=True
    )
    return chosen


def main() -> int:
    """CLI entry point for CMake backend mode verification."""
    parser = argparse.ArgumentParser(description="Verify EDG CMake backend option modes.")
    parser.add_argument("--cpfe", help="Path to cpfe executable to verify.")
    parser.add_argument("--llvm", type=int, choices=[0, 1], default=0, help="Expected LLVM backend state.")
    parser.add_argument("--gcc", type=int, choices=[0, 1], default=0, help="Expected GCC backend state.")
    parser.add_argument(
        "--c",
        type=int,
        choices=[0, 1],
        default=1,
        help="Expected C-generating backend state.",
    )
    parser.add_argument("--all-modes", action="store_true", help="Verify all 4 build directories under build/.")
    args = parser.parse_args()

    if args.all_modes:
        modes = [
            ("Mode A (Neither)", ["build/mode-a-neither"], False, False, True),
            (
                "Mode B (LLVM Only)",
                ["build/llvm-debug", "build/llvm-agent"],
                True,
                False,
                True,
            ),
            (
                "Mode C (GCC Only)",
                ["build/gcc-be-debug", "build/gcc-agent"],
                False,
                True,
                True,
            ),
            (
                "Mode D (Both LLVM and GCC)",
                ["build/multibackend-final", "build/multibackend-debug"],
                True,
                True,
                True,
            ),
        ]
        for label, cand_dirs, llvm_on, gcc_on, c_on in modes:
            bdir = resolve_or_build_mode_dir(cand_dirs, llvm_on, gcc_on, c_on)
            exe = "cpfe.exe" if sys.platform == "win32" else "cpfe"
            cpfe_path = os.path.join(bdir, "bin", exe)
            if not os.path.exists(cpfe_path):
                raise RuntimeError(f"{label}: binary not found at {cpfe_path}")
            smoke_test_binary(cpfe_path, llvm_on, gcc_on, c_on)
            print(f"[PASS] {label} ({cpfe_path})")
        return 0

    if not args.cpfe:
        parser.error("--cpfe is required when --all-modes is not specified.")
    smoke_test_binary(args.cpfe, bool(args.llvm), bool(args.gcc), bool(args.c))
    print(
        f"[PASS] Verified cpfe binary ({args.cpfe}) [LLVM={args.llvm}, GCC={args.gcc}, C={args.c}]"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
