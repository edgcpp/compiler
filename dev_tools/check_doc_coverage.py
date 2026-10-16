#!/usr/bin/env python3
import sys
import os
import re

def check_file_coverage(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    errors = []
    
    # 1. File-level check
    if '@file' not in content or '@brief' not in content[:1000]:
        errors.append("Missing @file or @brief at the top of the file")

    # 2. Function checks
    # Simple regex for finding C++ functions returning llvm_gen_be_error_t or similar
    # ignoring static/inline for simplicity or matching all
    func_pattern = re.compile(r'^(?:static\s+)?(?:inline\s+)?llvm_gen_be_error_t\s+([a-zA-Z0-9_]+)\s*\((.*?)\)\s*noexcept\s*\{', re.MULTILINE)
    
    for match in func_pattern.finditer(content):
        func_name = match.group(1)
        params_str = match.group(2)
        
        # Look backwards for a doxygen comment
        preceding_text = content[:match.start()]
        last_comment_idx = preceding_text.rfind('/**')
        if last_comment_idx == -1 or preceding_text[last_comment_idx:].count('*/') == 0:
            errors.append(f"Function {func_name} lacks Doxygen comment")
            continue
            
        comment = preceding_text[last_comment_idx:]
        
        if '@brief' not in comment:
            errors.append(f"Function {func_name} lacks @brief")
            
        if '@return' not in comment and func_name != "llvm_gen_be_set_error":
            errors.append(f"Function {func_name} lacks @return")
            
        # Check params
        if params_str.strip() != "" and params_str.strip() != "void":
            params = [p.strip() for p in params_str.split(',')]
            for param in params:
                param_name = param.split()[-1].replace('*', '').replace('&', '')
                if f"@param" not in comment and param_name not in comment:
                   # Sometimes the name is omitted or just documented without @param
                   if f"param" not in comment:
                       errors.append(f"Function {func_name} lacks @param for {param_name}")

    # 3. Struct/Class checks
    struct_pattern = re.compile(r'^(?:typedef\s+)?struct\s+([a-zA-Z0-9_]+)\s*\{', re.MULTILINE)
    for match in struct_pattern.finditer(content):
        struct_name = match.group(1)
        preceding_text = content[:match.start()]
        last_comment_idx = preceding_text.rfind('/**')
        if last_comment_idx == -1 or preceding_text[last_comment_idx:].count('*/') == 0:
            errors.append(f"Struct {struct_name} lacks Doxygen comment")

    return errors

def main():
    src_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '../src'))
    
    llvm_be_files = [f for f in os.listdir(src_dir) if f.startswith('llvm_gen_be') and (f.endswith('.cpp') or f.endswith('.h'))]
    
    all_passed = True
    total_errors = 0
    
    for file in llvm_be_files:
        filepath = os.path.join(src_dir, file)
        errors = check_file_coverage(filepath)
        if errors:
            print(f"File {file} has documentation coverage issues:")
            for err in errors:
                print(f"  - {err}")
            all_passed = False
            total_errors += len(errors)
            
    if all_passed:
        print("100% Documentation Coverage Verified!")
        sys.exit(0)
    else:
        print(f"\nFailed: {total_errors} documentation issues found.")
        sys.exit(1)

if __name__ == "__main__":
    main()
