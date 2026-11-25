#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Static policy gate enforcing centralized error enums, zero (void) ignores,
and noexcept-everywhere / zero-C++-exceptions invariants."""

import argparse
import re
import sys

from pathlib import Path
from typing import List, Optional, Sequence, Set, Tuple


_NL = chr(10)
_BS = chr(92)

_ALLOWED_NON_ERROR_FUNCTIONS: Set[str] = {
    "main",
    "EDG_MAIN",
    "LLVMFuzzerTestOneInput",
    "back_end",
    "c_gen_be",
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
    "__gcc_be_eh_personality_v0",
    "__gcc_be_eh_match_catch",
    "__gcc_be_eh_check_unwind_state",
}

_VALID_ERROR_RETURNS: Set[str] = {
    "edg_be_driver_error_t",
    "llvm_gen_be_error_t",
    "gcc_gen_be_error_t",
}


def strip_comments_and_strings(source: str) -> str:
    """Replaces C/C++ comments and string/character literals with spaces while
    preserving line breaks so line numbers remain accurate."""
    out: List[str] = []
    i = 0
    n = len(source)
    while i < n:
        ch = source[i]
        if ch == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != _NL:
                out.append(" ")
                i += 1
        elif ch == "/" and i + 1 < n and source[i + 1] == "*":
            out.append(" ")
            out.append(" ")
            i += 2
            while i < n and not (
                source[i] == "*" and i + 1 < n and source[i + 1] == "/"
            ):
                out.append(_NL if source[i] == _NL else " ")
                i += 1
            if i < n:
                out.append(" ")
                out.append(" ")
                i += 2
        elif ch in ('"', "'"):
            quote = ch
            out.append(quote)
            i += 1
            while i < n and source[i] != quote:
                if source[i] == _BS and i + 1 < n:
                    out.append(_NL if source[i] == _NL else " ")
                    out.append(_NL if source[i + 1] == _NL else " ")
                    i += 2
                    continue
                out.append(_NL if source[i] == _NL else " ")
                i += 1
            if i < n:
                out.append(quote)
                i += 1
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def check_file_policy(filepath: Path, *, is_test_file: bool = False) -> List[str]:
    """Checks a single C/C++ header or source file against all five global
    error-enum, zero-(void)-ignore, and noexcept/no-exceptions rules."""
    raw = filepath.read_text(encoding="utf-8")
    if "#include <stdexcept>" in raw:
        return [f"{filepath}:1: Forbidden <stdexcept> header inclusion"]

    cleaned = strip_comments_and_strings(raw)
    lines = cleaned.splitlines()
    violations: List[str] = []

    ws = "[" + _BS + "s]*"
    wb = _BS + "b"
    lp = _BS + "("
    rp = _BS + ")"
    lb = _BS + "["
    rb = _BS + "]"
    lbr = _BS + "{"

    forbidden_patterns: List[Tuple[re.Pattern[str], str]] = [
        (
            re.compile(
                lp + ws + "void" + ws + rp + ws + "[A-Za-z_][A-Za-z0-9_:]*" + ws + lp
            ),
            "Forbidden (void) cast discarding function return value",
        ),
        (
            re.compile(
                wb + "static_cast" + ws + "<" + ws + "void" + ws + ">" + ws + lp
            ),
            "Forbidden static_cast<void>(...) discarding expression result",
        ),
        (
            re.compile(wb + "std" + ws + "::" + ws + "ignore" + ws + "="),
            "Forbidden std::ignore assignment discarding return value",
        ),
        (
            re.compile(wb + "LLVM_GEN_BE_ASSERT_OK" + wb),
            "Forbidden error-swallowing macro LLVM_GEN_BE_ASSERT_OK",
        ),
        (
            re.compile(wb + "(?:try|catch|throw)" + wb),
            "Forbidden C++ exception keyword (try/catch/throw)",
        ),
        (
            re.compile(wb + "dynamic_cast" + ws + "<"),
            "Forbidden RTTI dynamic_cast in compiler/driver code",
        ),
        (
            re.compile(wb + "(?:abort|exit|llvm_unreachable)" + ws + lp),
            "Forbidden process termination / unreachable call (percolate error enum instead)",
        ),
        (
            re.compile(
                wb
                + "new"
                + "["
                + _BS
                + "s]+"
                + "(?!"
                + lp
                + ws
                + "(?:std"
                + ws
                + "::"
                + ws
                + ")?nothrow"
                + ws
                + rp
                + ")"
                + "[A-Za-z_][A-Za-z0-9_:<>]*"
                + ws
                + "(?:"
                + lp
                + "|"
                + lbr
                + ")"
            ),
            "Forbidden throwing operator new (use new (std::nothrow) and check nullptr)",
        ),
    ]

    for line_no, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        for pat, msg in forbidden_patterns:
            if pat.search(line):
                violations.append(f"{filepath}:{line_no}: {msg}")

    fn_re = re.compile(
        "^[ "
        + chr(9)
        + "]*(?:template"
        + ws
        + "<[^>]*>"
        + ws
        + ")?"
        + "(?:(?:static|inline|extern(?:"
        + "["
        + _BS
        + 's]+"C")?|constexpr|'
        + "EDG_BE_NODISCARD|GCC_GEN_BE_NODISCARD|"
        + lb
        + lb
        + "nodiscard"
        + rb
        + rb
        + ")"
        + "["
        + _BS
        + "s]+)*"
        + "([A-Za-z_][A-Za-z0-9_:]*"
        + ws
        + _BS
        + "*{0,2})["
        + _BS
        + "s]+"
        + "([A-Za-z_][A-Za-z0-9_]*)"
        + ws
        + lp
        + "([^;{}]*)"
        + rp
        + ws
        + "([A-Za-z0-9_]*)"
        + ws
        + "(?:"
        + lbr
        + "|;)",
        re.MULTILINE,
    )

    for match in fn_re.finditer(cleaned):
        ret_type = "".join(match.group(1).split())
        fn_name = match.group(2)
        trailing_spec = match.group(4).strip()
        if ret_type in ("return", "typedef", "delete", "case", "else", "goto", "using"):
            continue
        if fn_name in (
            "if",
            "for",
            "while",
            "switch",
            "return",
            "sizeof",
            "alignof",
            "else",
        ):
            continue
        line_no = cleaned.count(_NL, 0, match.start()) + 1
        base_ret = ret_type.split("::")[-1]

        if fn_name not in _ALLOWED_NON_ERROR_FUNCTIONS:
            if base_ret not in _VALID_ERROR_RETURNS:
                violations.append(
                    f"{filepath}:{line_no}: Function '{fn_name}' returns '{ret_type}' "
                    f"instead of centralized error enum"
                )

        if fn_name != "main":
            if trailing_spec not in (
                "noexcept",
                "EDG_BE_NOEXCEPT",
                "GCC_GEN_BE_NOEXCEPT",
            ):
                label = "Test helper" if is_test_file else "Function"
                violations.append(
                    f"{filepath}:{line_no}: {label} '{fn_name}' is missing noexcept / "
                    f"EDG_BE_NOEXCEPT / GCC_GEN_BE_NOEXCEPT qualifier"
                )

    return violations


def collect_target_files(
    repo_root: Path, explicit_paths: Sequence[str], check_all: bool
) -> List[Tuple[Path, bool]]:
    """Collects (path, is_test_file) pairs for either explicit paths, Stage-0
    shared files, or the full repository (--all)."""
    results: List[Tuple[Path, bool]] = []
    if explicit_paths:
        for p_str in explicit_paths:
            p = Path(p_str)
            if p.is_dir():
                for child in sorted(p.rglob("*")):
                    if child.suffix in (".h", ".c", ".cpp"):
                        results.append((child, "tests" in child.parts))
            elif p.exists() and p.suffix in (".h", ".c", ".cpp"):
                results.append((p, "tests" in p.parts))
        return results

    patterns: List[Tuple[str, bool]] = [
        ("src/edg_be_*.h", False),
        ("src/edg_be_*.cpp", False),
        ("tests/driver/*.h", True),
        ("tests/driver/*.cpp", True),
    ]
    if check_all:
        patterns.extend(
            [
                ("src/llvm_gen_be*.h", False),
                ("src/llvm_gen_be*.cpp", False),
                ("src/gcc_gen_be_*.h", False),
                ("src/gcc_gen_be_*.c", False),
                ("lib_src/gcc_be_eh_rt.h", False),
                ("lib_src/gcc_be_eh_rt.c", False),
            ]
        )

    seen: Set[Path] = set()
    for glob_pat, is_test in patterns:
        for matched in sorted(repo_root.glob(glob_pat)):
            if matched not in seen:
                seen.add(matched)
                results.append((matched, is_test))
    return results


def main(argv: Optional[Sequence[str]] = None) -> int:
    """CLI entry point for error-enum, zero-(void)-ignore, and noexcept policy checks."""
    parser = argparse.ArgumentParser(
        description="Verify centralized error enum, zero-(void)-ignore, and noexcept policies."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        help="Explicit files or directories to check (defaults to Stage-0 edg_be_* and tests/driver/*).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Check Stage-0, LLVM backend, and GCC backend files together.",
    )
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent
    targets = collect_target_files(repo_root, args.paths, args.all)
    if not targets:
        print(
            "Error: No target C/C++ files matched for policy inspection.",
            file=sys.stderr,
        )
        return 1

    all_violations: List[str] = []
    for fpath, is_test in targets:
        all_violations.extend(check_file_policy(fpath, is_test_file=is_test))

    if all_violations:
        for v in all_violations:
            print(v, file=sys.stderr)
        print(
            f"Policy check failed: {len(all_violations)} violation(s) across {len(targets)} file(s).",
            file=sys.stderr,
        )
        return 1

    print(f"100% Error-Enum & noexcept Policy Verified across {len(targets)} file(s)!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
