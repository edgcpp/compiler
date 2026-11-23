#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions. See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Cross-backend differential torture test runner for LLVM and GCC backends.

For every test case under tests/differential/cases/ (single-TU .c/.cpp sources
and multi-TU cross-backend .manifest.json descriptors), compiles and links five
independent native binaries:
  1. cpfe/edg++ --backend=llvm -O0
  2. cpfe/edg++ --backend=llvm -O2
  3. cpfe/edg++ --backend=gcc  -O0
  4. cpfe/edg++ --backend=gcc  -O2
  5. System reference compiler (clang/clang++ or gcc/g++) -O2

Executes all five binaries and verifies 100% identical exit code, stdout, and
stderr across all configurations.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
import shutil
import subprocess
import sys
import tempfile
from typing import Any

NL = os.linesep


def find_reference_compilers() -> tuple[str, str]:
    """Locates system C and C++ reference compilers for ground-truth checks."""
    c_cand = shutil.which("clang") or shutil.which("gcc") or shutil.which("cc")
    cxx_cand = shutil.which("clang++") or shutil.which("g++") or shutil.which("c++")
    if not c_cand or not cxx_cand:
        raise RuntimeError("System C/C++ reference compiler not found in PATH.")
    return c_cand, cxx_cand


def discover_cases(cases_root: str, filter_substr: str) -> list[str]:
    """Discovers all standalone .c/.cpp and .manifest.json test descriptors."""
    manifests: list[str] = []
    manifest_dirs: set[str] = set()
    for dirpath, _, filenames in os.walk(cases_root):
        for fn in sorted(filenames):
            if fn.endswith(".manifest.json"):
                full = os.path.join(dirpath, fn)
                manifests.append(full)
                manifest_dirs.add(dirpath)

    standalone: list[str] = []
    for dirpath, _, filenames in os.walk(cases_root):
        if dirpath in manifest_dirs:
            continue
        for fn in sorted(filenames):
            if fn.endswith(".c") or fn.endswith(".cpp"):
                standalone.append(os.path.join(dirpath, fn))

    all_cases = sorted(standalone) + sorted(manifests)
    if filter_substr:
        all_cases = [c for c in all_cases if filter_substr in c]
    return all_cases


def compile_and_execute(
    cmd: list[str],
    exe_path: str,
    env: dict[str, str],
) -> tuple[bool, str, int, str, str]:
    """Compiles via cmd and runs exe_path, returning (ok, err, rc, out, err_s)."""
    comp = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if comp.returncode != 0:
        msg = (
            f"Compile failed (rc={comp.returncode}): {' '.join(cmd)}{NL}"
            f"STDOUT:{NL}{comp.stdout}{NL}STDERR:{NL}{comp.stderr}"
        )
        return False, msg, comp.returncode, "", ""
    run_res = subprocess.run([exe_path], capture_output=True, text=True, env=env)
    return True, "", run_res.returncode, run_res.stdout, run_res.stderr


def parse_case_directives(case_path: str, is_cxx: bool) -> tuple[str, list[str]]:
    """Extracts optional DIFF_STD and DIFF_FLAGS directives from file header."""
    std_flag = "-std=c++20" if is_cxx else "-std=gnu17"
    extra: list[str] = []
    with open(case_path, "r", encoding="utf-8") as f:
        for _ in range(8):
            line = f.readline()
            if not line:
                break
            if "DIFF_STD:" in line:
                val = line.split("DIFF_STD:", 1)[1].strip().rstrip("*/").strip()
                if val:
                    std_flag = val
            if "DIFF_FLAGS:" in line:
                raw = line.split("DIFF_FLAGS:", 1)[1].strip().rstrip("*/").strip()
                extra.extend(tok for tok in raw.split() if tok)
    return std_flag, extra


def resolve_openmp_ref_cmd(
    ref_drv: str, is_cxx: bool, std_flag: str, extra_flags: list[str], case_path: str
) -> list[str]:
    """Builds reference compiler command supporting -fopenmp on macOS and Linux."""
    if "-fopenmp" in extra_flags and sys.platform == "darwin":
        hb_bin = "/opt/homebrew/opt/llvm/bin/" + ("clang++" if is_cxx else "clang")
        omp_lib = "/opt/homebrew/opt/libomp/lib"
        if os.path.exists(hb_bin) and os.path.isdir(omp_lib):
            return [
                hb_bin,
                std_flag,
                "-O2",
                *extra_flags,
                f"-L{omp_lib}",
                f"-Wl,-rpath,{omp_lib}",
                case_path,
            ]
    return [ref_drv, std_flag, "-O2", *extra_flags, case_path]


def run_single_source_case(
    case_path: str,
    cpfe_c: str,
    edg_cxx: str,
    ref_c: str,
    ref_cxx: str,
    scratch_root: str,
    env: dict[str, str],
) -> tuple[str, bool, str]:
    """Runs 5-way differential build+execution on a single .c or .cpp source."""
    is_cxx = case_path.endswith(".cpp")
    edg_drv = edg_cxx if is_cxx else cpfe_c
    ref_drv = ref_cxx if is_cxx else ref_c
    std_flag, extra_flags = parse_case_directives(case_path, is_cxx)
    if ("cat10_atomics_tls" in case_path or "cat06_eh_cleanup" in case_path) and (
        sys.platform != "darwin" and "-pthread" not in extra_flags
    ):
        extra_flags.append("-pthread")

    workdir = tempfile.mkdtemp(prefix="diff_", dir=scratch_root)
    try:
        configs = [
            (
                "llvm_O0",
                [edg_drv, "--backend=llvm", std_flag, "-O0", *extra_flags, case_path],
            ),
            (
                "llvm_O2",
                [edg_drv, "--backend=llvm", std_flag, "-O2", *extra_flags, case_path],
            ),
            (
                "gcc_O0",
                [edg_drv, "--backend=gcc", std_flag, "-O0", *extra_flags, case_path],
            ),
            (
                "gcc_O2",
                [edg_drv, "--backend=gcc", std_flag, "-O2", *extra_flags, case_path],
            ),
            (
                "ref_O2",
                resolve_openmp_ref_cmd(
                    ref_drv, is_cxx, std_flag, extra_flags, case_path
                ),
            ),
        ]
        results: dict[str, tuple[int, str, str]] = {}
        for label, base_cmd in configs:
            exe_sfx = ".exe" if sys.platform == "win32" else ""
            exe_path = os.path.join(workdir, f"bin_{label}{exe_sfx}")
            ok, err_msg, rc, out_s, err_s = compile_and_execute(
                [*base_cmd, "-o", exe_path], exe_path, env
            )
            if not ok:
                return case_path, False, f"[{label}] {err_msg}"
            results[label] = (rc, out_s, err_s)

        ref_tuple = results["ref_O2"]
        for label, tup in results.items():
            if tup != ref_tuple:
                detail = (
                    f"Output mismatch in {label} vs ref_O2:{NL}"
                    f"  {label}: rc={tup[0]} stdout={tup[1]!r} stderr={tup[2]!r}{NL}"
                    f"  ref_O2:  rc={ref_tuple[0]} stdout={ref_tuple[1]!r} "
                    f"stderr={ref_tuple[2]!r}"
                )
                return case_path, False, detail
        return case_path, True, ""
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def run_manifest_case(
    manifest_path: str,
    cpfe_c: str,
    edg_cxx: str,
    ref_c: str,
    ref_cxx: str,
    scratch_root: str,
    env: dict[str, str],
) -> tuple[str, bool, str]:
    """Runs 5-way differential verification on a cross-backend manifest."""
    mdir = os.path.dirname(manifest_path)
    with open(manifest_path, "r", encoding="utf-8") as f:
        spec: dict[str, Any] = json.load(f)

    if spec.get("kind") == "cxx20_module":
        workdir = tempfile.mkdtemp(prefix="mdiff_", dir=scratch_root)
        try:
            iface_src = os.path.join(mdir, spec["iface"])
            main_src = os.path.join(mdir, spec["main"])
            mod_name = spec["mod_name"]
            exe_sfx = ".exe" if sys.platform == "win32" else ""
            results: dict[str, tuple[int, str, str]] = {}
            for label, be, opt_flag in [
                ("llvm_O0", "llvm", "-O0"),
                ("llvm_O2", "llvm", "-O2"),
                ("gcc_O0", "gcc", "-O0"),
                ("gcc_O2", "gcc", "-O2"),
            ]:
                sub = os.path.join(workdir, label)
                os.makedirs(sub, exist_ok=True)
                ifc_path = os.path.join(sub, f"{mod_name}.ifc")
                m_obj = os.path.join(sub, "iface.o")
                main_obj = os.path.join(sub, "main.o")
                exe_path = os.path.join(sub, f"app{exe_sfx}")
                for step_cmd in [
                    [
                        edg_cxx,
                        f"--backend={be}",
                        "-std=c++20",
                        opt_flag,
                        f"-fmodule-output={ifc_path}",
                        "-c",
                        iface_src,
                        "-o",
                        m_obj,
                    ],
                    [
                        edg_cxx,
                        f"--backend={be}",
                        "-std=c++20",
                        opt_flag,
                        f"-fprebuilt-module-path={sub}",
                        "-c",
                        main_src,
                        "-o",
                        main_obj,
                    ],
                ]:
                    c_res = subprocess.run(
                        step_cmd, capture_output=True, text=True, env=env
                    )
                    if c_res.returncode != 0:
                        return (
                            manifest_path,
                            False,
                            f"[{label}] module compile failed: {' '.join(step_cmd)}{NL}"
                            f"{c_res.stderr}",
                        )
                ok, err_msg, rc, out_s, err_s = compile_and_execute(
                    [edg_cxx, f"--backend={be}", main_obj, m_obj, "-o", exe_path],
                    exe_path,
                    env,
                )
                if not ok:
                    return manifest_path, False, f"[{label}] {err_msg}"
                results[label] = (rc, out_s, err_s)

            ref_sub = os.path.join(workdir, "ref_O2")
            os.makedirs(ref_sub, exist_ok=True)
            ref_mod_cxx = ref_cxx
            if sys.platform == "darwin" and os.path.exists(
                "/opt/homebrew/opt/llvm/bin/clang++"
            ):
                ref_mod_cxx = "/opt/homebrew/opt/llvm/bin/clang++"
            pcm_path = os.path.join(ref_sub, f"{mod_name}.pcm")
            rm_obj = os.path.join(ref_sub, "iface.o")
            rmain_obj = os.path.join(ref_sub, "main.o")
            rexe = os.path.join(ref_sub, f"app{exe_sfx}")
            for step_cmd in [
                [
                    ref_mod_cxx,
                    "-std=c++20",
                    "-O2",
                    "-x",
                    "c++-module",
                    "--precompile",
                    iface_src,
                    "-o",
                    pcm_path,
                ],
                [ref_mod_cxx, "-std=c++20", "-O2", "-c", pcm_path, "-o", rm_obj],
                [
                    ref_mod_cxx,
                    "-std=c++20",
                    "-O2",
                    f"-fmodule-file={mod_name}={pcm_path}",
                    "-c",
                    main_src,
                    "-o",
                    rmain_obj,
                ],
            ]:
                c_res = subprocess.run(
                    step_cmd, capture_output=True, text=True, env=env
                )
                if c_res.returncode != 0:
                    return (
                        manifest_path,
                        False,
                        f"[ref_O2] module compile failed: {' '.join(step_cmd)}{NL}"
                        f"{c_res.stderr}",
                    )
            ok, err_msg, rc, out_s, err_s = compile_and_execute(
                [ref_mod_cxx, rmain_obj, rm_obj, "-o", rexe], rexe, env
            )
            if not ok:
                return manifest_path, False, f"[ref_O2] {err_msg}"
            results["ref_O2"] = (rc, out_s, err_s)
            ref_tuple = results["ref_O2"]
            for label, tup in results.items():
                if tup != ref_tuple:
                    return (
                        manifest_path,
                        False,
                        f"Module mismatch in {label} vs ref_O2: {tup} vs {ref_tuple}",
                    )
            return manifest_path, True, ""
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

    units: list[dict[str, str]] = spec["units"]
    any_cxx = any(u["src"].endswith(".cpp") for u in units)
    edg_link = edg_cxx if any_cxx else cpfe_c
    ref_link = ref_cxx if any_cxx else ref_c

    workdir = tempfile.mkdtemp(prefix="xdiff_", dir=scratch_root)
    try:
        modes = [
            ("mix_fwd_O0", False, "-O0", False),
            ("mix_fwd_O2", False, "-O2", False),
            ("mix_rev_O0", True, "-O0", False),
            ("mix_rev_O2", True, "-O2", False),
            ("ref_O2", False, "-O2", True),
        ]
        results: dict[str, tuple[int, str, str]] = {}
        for label, swap_be, opt_flag, use_ref in modes:
            objs: list[str] = []
            for idx, unit in enumerate(units):
                src_path = os.path.join(mdir, unit["src"])
                u_cxx = src_path.endswith(".cpp")
                std_flag = "-std=c++20" if u_cxx else "-std=gnu17"
                obj_path = os.path.join(workdir, f"{label}_{idx}.o")
                if use_ref:
                    u_drv = ref_cxx if u_cxx else ref_c
                    c_cmd = [u_drv, std_flag, opt_flag, "-c", src_path, "-o", obj_path]
                else:
                    be = unit["backend"]
                    if swap_be:
                        be = "gcc" if be == "llvm" else "llvm"
                    u_drv = edg_cxx if u_cxx else cpfe_c
                    c_cmd = [
                        u_drv,
                        f"--backend={be}",
                        std_flag,
                        opt_flag,
                        "-c",
                        src_path,
                        "-o",
                        obj_path,
                    ]
                comp = subprocess.run(c_cmd, capture_output=True, text=True, env=env)
                if comp.returncode != 0:
                    return (
                        manifest_path,
                        False,
                        f"[{label}] compile failed: {' '.join(c_cmd)}{NL}{comp.stderr}",
                    )
                objs.append(obj_path)

            exe_sfx = ".exe" if sys.platform == "win32" else ""
            exe_path = os.path.join(workdir, f"bin_{label}{exe_sfx}")
            if use_ref:
                l_cmd = [ref_link, *objs, "-o", exe_path]
            else:
                l_cmd = [edg_link, "--backend=gcc", *objs, "-o", exe_path]
            ok, err_msg, rc, out_s, err_s = compile_and_execute(l_cmd, exe_path, env)
            if not ok:
                return manifest_path, False, f"[{label}] {err_msg}"
            results[label] = (rc, out_s, err_s)

        ref_tuple = results["ref_O2"]
        for label, tup in results.items():
            if tup != ref_tuple:
                detail = (
                    f"Cross-ABI mismatch in {label} vs ref_O2:{NL}"
                    f"  {label}: rc={tup[0]} stdout={tup[1]!r} stderr={tup[2]!r}{NL}"
                    f"  ref_O2:  rc={ref_tuple[0]} stdout={ref_tuple[1]!r} "
                    f"stderr={ref_tuple[2]!r}"
                )
                return manifest_path, False, detail
        return manifest_path, True, ""
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def main() -> int:
    """CLI entry point for the cross-backend differential torture runner."""
    repo_root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    )
    default_bdir = os.path.join(repo_root, "build", "multibackend-final")
    default_cases = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases")
    default_scratch = os.path.join(repo_root, ".tmp", "agent-differential-runs")

    parser = argparse.ArgumentParser(
        description="Run 5-way LLVM vs GCC vs Reference differential torture suite."
    )
    parser.add_argument(
        "--build-dir",
        default=default_bdir,
        help="Unified Mode-D build directory containing bin/cpfe and bin/edg++.",
    )
    parser.add_argument(
        "--cases-dir",
        default=default_cases,
        help="Root directory of differential test cases.",
    )
    parser.add_argument(
        "--scratch-dir",
        default=default_scratch,
        help="Scratch directory for temporary compiled binaries.",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=min(8, os.cpu_count() or 4),
        help="Parallel worker count.",
    )
    parser.add_argument(
        "--filter",
        default="",
        help="Optional substring filter on case file paths.",
    )
    args = parser.parse_args()

    exe_sfx = ".exe" if sys.platform == "win32" else ""
    cpfe_c = os.path.abspath(os.path.join(args.build_dir, "bin", f"cpfe{exe_sfx}"))
    edg_cxx = os.path.abspath(os.path.join(args.build_dir, "bin", f"edg++{exe_sfx}"))
    if not os.path.exists(cpfe_c) or not os.path.exists(edg_cxx):
        sys.stderr.write(f"error: Mode-D binaries not found under {args.build_dir}{NL}")
        return 1

    ref_c, ref_cxx = find_reference_compilers()
    cases = discover_cases(args.cases_dir, args.filter)
    if not cases:
        sys.stderr.write(f"error: no differential cases found in {args.cases_dir}{NL}")
        return 1

    os.makedirs(args.scratch_dir, exist_ok=True)
    child_env = dict(os.environ)
    child_env["TMPDIR"] = os.path.abspath(args.scratch_dir)

    passed = 0
    failures: list[tuple[str, str]] = []

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = {}
        for cpath in cases:
            if cpath.endswith(".manifest.json"):
                fut = pool.submit(
                    run_manifest_case,
                    cpath,
                    cpfe_c,
                    edg_cxx,
                    ref_c,
                    ref_cxx,
                    args.scratch_dir,
                    child_env,
                )
            else:
                fut = pool.submit(
                    run_single_source_case,
                    cpath,
                    cpfe_c,
                    edg_cxx,
                    ref_c,
                    ref_cxx,
                    args.scratch_dir,
                    child_env,
                )
            futs[fut] = cpath

        for idx, fut in enumerate(as_completed(futs), 1):
            cpath, ok, detail = fut.result()
            rel = os.path.relpath(cpath, repo_root)
            if ok:
                passed += 1
            else:
                failures.append((rel, detail))
                sys.stderr.write(f"[FAIL] {rel}{NL}{detail}{NL}")
            if idx % 25 == 0 or idx == len(cases):
                print(
                    f"[{idx}/{len(cases)}] Differential progress: "
                    f"{passed} passed, {len(failures)} failed"
                )

    shutil.rmtree(args.scratch_dir, ignore_errors=True)

    if failures:
        sys.stderr.write(
            f"{NL}DIFFERENTIAL SUITE FAILED: {len(failures)} / {len(cases)} failed.{NL}"
        )
        return 1

    print(
        f"{NL}[PASS] Differential Torture Suite: {passed}/{len(cases)} cases "
        f"({passed * 5} native binaries) matched 100% across LLVM -O0/-O2, "
        f"GCC -O0/-O2, and Reference -O2!"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
