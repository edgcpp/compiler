#!/usr/bin/env python3
"""
GCC C/C++ Torture Suite Integration Harness for EDG LLVM Backend
This script automates the execution of the GCC torture suite (execute tests)
using the EDG cpfe (C/C++ front end) with the LLVM IR generation backend.

Usage:
  python3 run_gcc_torture_llvm.py <path_to_gcc_torture_c_execute> <path_to_cpfe_bin>
"""

import os
import sys
import subprocess
import tempfile
import json
from pathlib import Path

def run_cmd(cmd, cwd=None):
    try:
        result = subprocess.run(
            cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "Timeout"
    except Exception as e:
        return -2, "", str(e)

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <path_to_gcc_torture_c_execute> <path_to_cpfe_bin>")
        sys.exit(1)

    torture_dir = Path(sys.argv[1])
    cpfe_bin = Path(sys.argv[2]).resolve()

    if not torture_dir.is_dir():
        print(f"Error: Directory not found: {torture_dir}")
        sys.exit(1)
    if not cpfe_bin.is_file():
        print(f"Error: cpfe binary not found: {cpfe_bin}")
        sys.exit(1)

    test_files = list(torture_dir.rglob("*.c"))
    if not test_files:
        print(f"No .c files found in {torture_dir}")
        # Note: If no files, we exit successfully because it might be a dummy run
        sys.exit(0)

    print(f"Found {len(test_files)} tests.")
    
    results = {
        "passed": [],
        "cpfe_failed": [],
        "clang_failed": [],
        "exec_failed": [],
        "crashed_or_malformed": []
    }

    with tempfile.TemporaryDirectory() as tmpdir:
        for idx, test_file in enumerate(test_files, 1):
            print(f"[{idx}/{len(test_files)}] Running {test_file.name}...", end=" ", flush=True)
            
            # 1. Compile to LLVM IR using cpfe
            ll_file = Path(tmpdir) / f"{test_file.stem}.ll"
            cpfe_cmd = [
                str(cpfe_bin),
                "--c99", 
                "--edg_base_dir", "include_c++", # Default base dir
                "--gen_llvm_file_name", str(ll_file),
                str(test_file)
            ]
            
            cpfe_rc, cpfe_out, cpfe_err = run_cmd(cpfe_cmd)
            if cpfe_rc != 0 or not ll_file.exists():
                print("FAIL (cpfe)")
                results["cpfe_failed"].append(str(test_file))
                if "crash" in cpfe_err.lower() or "verify" in cpfe_err.lower():
                    results["crashed_or_malformed"].append(str(test_file))
                continue
                
            # 2. Compile LLVM IR to native executable using clang
            exe_file = Path(tmpdir) / f"{test_file.stem}.out"
            clang_cmd = ["clang", "-w", str(ll_file), "-o", str(exe_file)]
            clang_rc, clang_out, clang_err = run_cmd(clang_cmd)
            
            if clang_rc != 0:
                print("FAIL (clang)")
                results["clang_failed"].append(str(test_file))
                continue
                
            # 3. Execute the resulting binary
            exec_rc, exec_out, exec_err = run_cmd([str(exe_file)])
            if exec_rc != 0:
                print("FAIL (exec)")
                results["exec_failed"].append(str(test_file))
            else:
                print("PASS")
                results["passed"].append(str(test_file))

    print("\n--- Summary ---")
    print(f"Total: {len(test_files)}")
    print(f"Passed: {len(results['passed'])}")
    print(f"Failed in CPFE: {len(results['cpfe_failed'])}")
    print(f"Failed in Clang: {len(results['clang_failed'])}")
    print(f"Failed in Exec: {len(results['exec_failed'])}")
    print(f"Malformed IR / Crashes: {len(results['crashed_or_malformed'])}")
    
    with open("gcc_torture_results.json", "w") as f:
        json.dump(results, f, indent=2)
        
    print("\nDetailed results saved to gcc_torture_results.json")
    
    if len(results["crashed_or_malformed"]) > 0:
        print("\nERROR: Found backend crashes or malformed IR errors.")
        sys.exit(1)

if __name__ == "__main__":
    main()
