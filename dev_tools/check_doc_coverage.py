#!/usr/bin/env python3
"""Verifies 100% Doxygen documentation coverage across all LLVM backend and test files."""

import glob
import re
import sys


_BS = chr(92)
_REQUIRED_DEFGROUPS = {
    "llvm_gen_be",
    "llvm_gen_be_types",
    "llvm_gen_be_expr",
    "llvm_gen_be_stmt",
    "llvm_gen_be_decl",
    "llvm_gen_be_abi",
    "llvm_gen_be_cxx_abi",
    "llvm_gen_be_eh",
    "llvm_gen_be_coro",
    "llvm_gen_be_omp",
    "llvm_gen_be_debug",
    "llvm_gen_be_driver",
}


def strip_comments_and_strings(text: str) -> str:
    """Replaces C/C++ comments and string/char literals with spaces preserving line numbers."""
    out: list[str] = []
    i = 0
    n = len(text)
    nl = chr(10)
    while i < n:
        ch = text[i]
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != nl:
                out.append(" ")
                i += 1
        elif ch == "/" and i + 1 < n and text[i + 1] == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append(nl if text[i] == nl else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
        elif (
            ch == "R"
            and i + 1 < n
            and text[i + 1] == '"'
            and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_"))
        ):
            paren_pos = text.find("(", i + 2)
            if paren_pos != -1 and nl not in text[i + 2 : paren_pos]:
                delim = text[i + 2 : paren_pos]
                end_marker = ")" + delim + '"'
                end_pos = text.find(end_marker, paren_pos + 1)
                stop = (end_pos + len(end_marker)) if end_pos != -1 else n
                while i < stop:
                    out.append(nl if text[i] == nl else " ")
                    i += 1
            else:
                out.append(ch)
                i += 1
        elif ch in ('"', "'"):
            quote = ch
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == _BS and i + 1 < n:
                    out.append(nl if text[i] == nl else " ")
                    out.append(nl if text[i + 1] == nl else " ")
                    i += 2
                    continue
                out.append(nl if text[i] == nl else " ")
                i += 1
            if i < n:
                out.append(" ")
                i += 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def check_file_coverage(filepath: str) -> list[str]:
    """Checks file, module, macro, struct/enum, field/enumerator, function, noexcept, and error-ignore rules."""
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
    lines = content.splitlines()
    code_only = strip_comments_and_strings(content)
    code_lines = code_only.splitlines()

    errors: list[str] = []
    is_src_file = filepath.startswith("src/")

    # 1. File-level check (@file, @brief, @details, @defgroup / @ingroup)
    if "@file" not in content:
        errors.append("Missing @file Doxygen tag")
    if "@brief" not in content:
        errors.append("Missing @brief Doxygen tag")
    if "@details" not in content:
        errors.append("Missing @details Doxygen tag")
    if "@ingroup" not in content and "@defgroup" not in content:
        errors.append("Missing @defgroup / @ingroup module Doxygen tag")

    if filepath.endswith("llvm_gen_be_error.h"):
        if not re.search(
            r"typedef\s+enum\s+class\s+\[\[nodiscard\]\]\s+llvm_gen_be_error_t\s*:\s*int32_t",
            content,
        ):
            errors.append(
                "llvm_gen_be_error.h must define 'typedef enum class [[nodiscard]] llvm_gen_be_error_t : int32_t'"
            )

    # 1b. Non-include-guard #define Doxygen check
    define_re = re.compile(
        r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)", re.MULTILINE
    )
    for dm in define_re.finditer(code_only):
        mname = dm.group(1)
        if mname.endswith("_H") or mname.endswith("_INCLUDED"):
            continue
        lineno = content.count(chr(10), 0, dm.start()) + 1
        line_str = lines[lineno - 1]
        pre = chr(10).join(lines[max(0, lineno - 12) : lineno - 1]).rstrip()
        if "///<" not in line_str and not pre.endswith("*/"):
            errors.append(f"Line {lineno}: #define '{mname}' missing Doxygen comment")

    # 1c. Forbidden error-swallowing patterns in src/llvm_gen_be* and tests
    if "LLVM_GEN_BE_ASSERT_OK" in code_only:
        errors.append("Forbidden error-swallowing macro LLVM_GEN_BE_ASSERT_OK")
    if is_src_file:
        void_call_re = re.compile(
            r"\(\s*void\s*\)\s*[A-Za-z_][A-Za-z0-9_:]*\s*\("
            r"|static_cast\s*<\s*void\s*>\s*\("
            r"|\bstd\s*::\s*ignore\b"
        )
        for vm in void_call_re.finditer(code_only):
            vln = code_only.count(chr(10), 0, vm.start()) + 1
            errors.append(f"Line {vln}: Forbidden error-ignoring cast/idiom")

    # 2. Struct / Class / Enum and Field / Enumerator check
    in_struct = False
    struct_name = ""
    brace_depth = 0
    for i, raw_line in enumerate(lines, 1):
        s_code = code_lines[i - 1].strip() if i - 1 < len(code_lines) else ""
        s = raw_line.strip()
        m = re.match(
            r"^(?:typedef\s+)?(struct|class|enum(?:\s+class)?)\s+(?:\[\[nodiscard\]\]\s+)?([a-zA-Z0-9_]+)?[^{;]*\{",
            s_code,
        )
        if m and not in_struct:
            kind, name = m.group(1), m.group(2) or "<anon>"
            pre = chr(10).join(lines[max(0, i - 18) : i - 1]).rstrip()
            if not pre.endswith("*/"):
                errors.append(
                    f"Line {i}: {kind} '{name}' is missing preceding /** ... */ Doxygen block"
                )
            else:
                c_start = pre.rfind("/**")
                blk = pre[c_start:] if c_start != -1 else ""
                if "@brief" not in blk:
                    errors.append(f"Line {i}: {kind} '{name}' Doxygen block missing @brief")
            in_struct = True
            struct_name = name
            brace_depth = s_code.count("{") - s_code.count("}")
            continue
        if in_struct:
            brace_depth += s_code.count("{") - s_code.count("}")
            if brace_depth <= 0:
                in_struct = False
                continue
            if (
                s_code
                and not s_code.startswith("public:")
                and not s_code.startswith("private:")
                and not s_code.startswith("protected:")
                and not s_code.startswith("}")
            ):
                if s_code.startswith("if ") or s_code.startswith("return "):
                    continue
                prev_line = lines[i - 2].strip() if i >= 2 else ""
                if "///<" not in s and "/**" not in s and not prev_line.endswith("*/"):
                    errors.append(
                        f"Line {i}: Field/enumerator in '{struct_name}' missing Doxygen comment: {s}"
                    )

    # 3. Function, noexcept, and function parameter check
    fn_pattern = re.compile(
        r"^[ \t]*(?:static[ \t]+|inline[ \t]+|extern[ \t]+)*"
        r"(llvm_gen_be_error_t|int|void|bool|a_boolean|char\*|a_targ_alignment|"
        r"a_targ_size_t|a_routine_ptr|a_source_file_ptr|a_type_qualifier_set|"
        r"a_scope_ptr|a_number_buffer|RunResult)\s+"
        r"([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*(noexcept)?\s*(?:\{|;)",
        re.MULTILINE,
    )
    for m in fn_pattern.finditer(code_only):
        ret_ty = m.group(1)
        fn_name = m.group(2)
        has_noexcept = m.group(4) == "noexcept"
        if fn_name in ("if", "while", "for", "switch", "return"):
            continue
        allowed_non_err_fns = {
            "main",
            "LLVMFuzzerTestOneInput",
            "is_bool_type",
            "f_alignment_of_type",
            "is_trivially_copyable_type",
            "is_class_struct_union_type",
            "type_has_nontrivial_destructor",
            "conv_seq_to_file_and_line",
            "mangled_typeinfo_name",
            "f_get_type_qualifiers",
            "is_reference_type",
            "is_empty_class_type",
            "get_variable_initializer",
            "scope_for_routine",
            "mangled_vtable_name",
            "mangled_vtbl_name",
            "mangled_vtt_name",
            "mangled_virtual_table_table_name",
            "mangled_function_name",
            "get_mangled_function_name",
            "key_function_for_class",
            "vtbl_decider_function_for_class",
            "skip_typerefs",
            "int_type_is_signed",
            "int_constant_value",
            "uns_constant_value",
            "type_is_nonreal",
            "il_lowering_needed",
            "il_read",
            "make_number_buffer",
            "fp_to_string",
            "free_general",
            "alloc_general",
            "assertion_failed",
            "insufficient_address_space",
            "derived_name",
        }
        if ret_ty != "llvm_gen_be_error_t" and fn_name not in allowed_non_err_fns:
            errors.append(
                f"Function '{fn_name}' returns '{ret_ty}' instead of 'llvm_gen_be_error_t'"
            )
        if not has_noexcept and fn_name not in allowed_non_err_fns:
            errors.append(f"Function '{fn_name}' is missing noexcept specifier")
        params_raw = m.group(3).strip()
        pre = content[: m.start()].rstrip()
        if not pre.endswith("*/"):
            errors.append(f"Function '{fn_name}' is missing preceding /** ... */ Doxygen block")
            continue
        c_start = pre.rfind("/**")
        comment = pre[c_start:] if c_start != -1 else ""
        if "@brief" not in comment:
            errors.append(f"Function '{fn_name}' is missing @brief")
        if "@details" not in comment:
            errors.append(f"Function '{fn_name}' is missing @details")
        if ret_ty != "void" and "@return" not in comment:
            errors.append(f"Function '{fn_name}' is missing @return")
        if ret_ty == "void" and "@return" in comment:
            errors.append(f"Void function '{fn_name}' must not have @return")

        doc_params = re.findall(r"@param(?:\[[a-z,]+\])?\s+([A-Za-z0-9_.]+)", comment)
        seen_doc_params: set[str] = set()
        for dp in doc_params:
            if dp in seen_doc_params:
                errors.append(f"Function '{fn_name}' has duplicate @param '{dp}'")
            seen_doc_params.add(dp)

        if params_raw and params_raw != "void":
            for p in params_raw.split(","):
                p_clean = p.strip()
                if p_clean == "..." or not p_clean:
                    continue
                p_no_attr = re.sub(r"\[\[[^\]]*\]\]", "", p_clean.split("=")[0])
                p_no_arr = re.sub(r"\[[^\]]*\]", "", p_no_attr).strip()
                tokens = re.findall(r"[a-zA-Z0-9_]+", p_no_arr)
                if not tokens:
                    continue
                pname = tokens[-1]
                if not re.search(r"@param(?:\[[a-z,]+\])?\s+" + re.escape(pname) + r"\b", comment):
                    errors.append(
                        f"Function '{fn_name}' parameter '{pname}' is missing @param documentation"
                    )

    # 4. Lambda noexcept and raw throwing new checks
    for lm in re.finditer(r"\[[^\];{}]*\]\s*\([^;{}]*\)\s*([^{;]*)\{", code_only):
        before = code_only[max(0, lm.start() - 20) : lm.start()].rstrip()
        if before and (before[-1].isalnum() or before[-1] in "_)]"):
            continue
        if content[lm.start() : lm.start() + 2] == "[[":
            continue
        if "noexcept" not in lm.group(1):
            ln = code_only.count("\n", 0, lm.start()) + 1
            errors.append(f"Line {ln}: Lambda is missing noexcept specifier")

    for idx, line in enumerate(code_lines, 1):
        if line.strip().startswith("#"):
            continue
        for nm in re.finditer(r"\bnew\b", line):
            after = line[nm.end() :].lstrip()
            if not after.startswith("(") or "nothrow" not in after[:30]:
                errors.append(
                    f"Line {idx}: Raw operator new without (std::nothrow): {lines[idx - 1].strip()}"
                )

    return errors


def main() -> None:
    """Entry point scanning all LLVM backend and test C++ files."""
    files = sorted(
        glob.glob("src/llvm_gen_be*.h")
        + glob.glob("src/llvm_gen_be*.cpp")
        + glob.glob("tests/*llvm*.cpp")
        + glob.glob("tests/llvm_ir_backend_tests/*.cpp")
    )

    if not files:
        print("Error: No LLVM backend or test files found.")
        sys.exit(1)

    all_passed = True
    total_errors = 0
    found_defgroups: set[str] = set()

    for f in files:
        with open(f, "r", encoding="utf-8") as fh:
            for gm in re.finditer(r"@defgroup\s+([A-Za-z0-9_]+)", fh.read()):
                found_defgroups.add(gm.group(1))
        errs = check_file_coverage(f)
        if errs:
            all_passed = False
            total_errors += len(errs)
            print(f"Documentation Coverage Failed for {f}:")
            for e in errs:
                print(f"  - {e}")

    missing_groups = sorted(_REQUIRED_DEFGROUPS - found_defgroups)
    if missing_groups:
        all_passed = False
        total_errors += len(missing_groups)
        for mg in missing_groups:
            print(f"Missing required @defgroup module definition: {mg}")

    if all_passed:
        print(f"100% Documentation Coverage Verified across {len(files)} files!")
        sys.exit(0)
    else:
        print(f"Failed: {total_errors} documentation issues found across {len(files)} files.")
        sys.exit(1)


if __name__ == "__main__":
    main()
