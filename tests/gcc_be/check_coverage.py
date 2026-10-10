#!/usr/bin/env python3
import sys
import subprocess
import os
import glob
import re

print("Checking coverage for gcc_gen_be_*.c...")

def run_gcov():
    src_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'src'))
    files = glob.glob(os.path.join(src_dir, 'gcc_gen_be_*.c'))
    
    if not files:
        print("No source files found!")
        return False
        
    failed = False
    
    for f in files:
        # Run gcov with branch probabilities
        result = subprocess.run(['gcov', '-b', '-c', f], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run gcov on {f}")
            continue
            
        output = result.stdout
        
        # Parse output
        lines_match = re.search(r'Lines executed:([\d\.]+)% of (\d+)', output)
        branches_match = re.search(r'Branches executed:([\d\.]+)% of (\d+)', output)
        
        lines_pct = float(lines_match.group(1)) if lines_match else 100.0
        branches_pct = float(branches_match.group(1)) if branches_match else 100.0
        
        print(f"File: {os.path.basename(f)}")
        print(f"  Lines: {lines_pct}%")
        print(f"  Branches: {branches_pct}%")
        
        if lines_pct < 100.0 or branches_pct < 100.0:
            failed = True
            
            # Print unexecuted lines using the generated .gcov file
            gcov_file = os.path.basename(f) + '.gcov'
            if os.path.exists(gcov_file):
                print(f"  [!] Missing coverage in {gcov_file}:")
                with open(gcov_file, 'r') as gf:
                    for line in gf:
                        if line.startswith('#####'):
                            print("    " + line.strip())
                        elif line.startswith('branch') and 'never executed' in line:
                            print("    " + line.strip())
                        elif line.startswith('branch') and 'taken 0%' in line:
                            print("    " + line.strip())
                            
    if failed:
        print("Coverage failed: Did not reach 100.0% coverage on all files.")
        return False
        
    print("Success: Reached 100.0% coverage!")
    return True

if __name__ == '__main__':
    if not run_gcov():
        sys.exit(1)
    sys.exit(0)
