#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Verifies 100% Doxygen documentation (file, module, type, member, function,
arg, return) and error-enum/noexcept policy across all GCC backend files."""

import glob
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from typing import List, Optional, Set
import xml.etree.ElementTree as ET


_ALLOWED_NON_ERROR_FUNCTIONS: Set[str] = {
    "main",
    "EDG_MAIN",
    "back_end",
    "__gcc_be_eh_personality_v0",
    "__gcc_be_eh_match_catch",
    "__gcc_be_eh_check_unwind_state",
}

_MODULE_TAG_RE = re.compile(r"@(?:defgroup|ingroup|addtogroup)[ \t]+[A-Za-z0-9_]+")
_FILE_TAG_RE = re.compile(r"@file(?![A-Za-z0-9_])")
_BRIEF_TAG_RE = re.compile(r"@brief(?![A-Za-z0-9_])")
_DETAILS_TAG_RE = re.compile(r"@details(?![A-Za-z0-9_])")
_INT_CAST_ERR_CALL_RE = re.compile(
    r"\(\s*int\s*\)\s*(?:gcc_gen_be_|__edg_gcc_eh_)[A-Za-z0-9_]*\s*\("
)


def _has_text(element: Optional[ET.Element]) -> bool:
    """Returns True when an XML element has non-empty, non-placeholder text."""
    if element is None:
        return False
    text = "".join(element.itertext()).strip()
    if "Missing brief" in text or "Missing details" in text:
        return False
    return len(text) > 0


def _collect_target_files(project_root: Path) -> List[Path]:
    """Collects all Agent-B GCC backend, EH runtime, and test source/header files."""
    patterns = (
        "src/gcc_gen_be_*.h",
        "src/gcc_gen_be_*.c",
        "lib_src/gcc_be_eh_rt.h",
        "lib_src/gcc_be_eh_rt.c",
        "lib_src/gcc_be_eh_rt.cpp",
        "tests/gcc_be/test_*.h",
        "tests/gcc_be/test_*.c",
        "tests/gcc_be/test_*.cpp",
    )
    found: List[Path] = []
    seen: Set[Path] = set()
    for pat in patterns:
        for raw in sorted(glob.glob(str(project_root / pat))):
            path = Path(raw).resolve()
            if path.is_file() and path not in seen:
                seen.add(path)
                found.append(path)
    return found


def _check_source_file_headers_and_policy(
    project_root: Path, files: List[Path]
) -> List[str]:
    """Checks @file/@brief/@details/@defgroup|@ingroup tags and error/noexcept policy."""
    errors: List[str] = []
    backend_files: List[str] = []

    for fpath in files:
        rel = fpath.relative_to(project_root)
        content = fpath.read_text(encoding="utf-8", errors="replace")

        if not _FILE_TAG_RE.search(content):
            errors.append(f"FAIL: {rel} is missing @file Doxygen tag.")
        if not _BRIEF_TAG_RE.search(content):
            errors.append(f"FAIL: {rel} is missing file-level @brief Doxygen tag.")
        if not _DETAILS_TAG_RE.search(content):
            errors.append(f"FAIL: {rel} is missing file-level @details Doxygen tag.")
        if not _MODULE_TAG_RE.search(content):
            errors.append(
                f"FAIL: {rel} is missing @defgroup / @ingroup Doxygen module tag."
            )

        rel_str = str(rel)
        if rel_str.startswith("src/gcc_gen_be_") or rel_str.startswith(
            "lib_src/gcc_be_eh_rt"
        ):
            backend_files.append(str(fpath))
            for lineno, line in enumerate(content.splitlines(), start=1):
                if _INT_CAST_ERR_CALL_RE.search(line):
                    errors.append(
                        f"FAIL (policy): {rel}:{lineno}: Forbidden (int) cast on "
                        "error-returning backend call; percolate gcc_gen_be_error_t."
                    )

    if backend_files:
        policy_script = (
            project_root / "dev_tools" / "check_error_and_noexcept_policy.py"
        )
        res = subprocess.run(
            [sys.executable, str(policy_script), *backend_files],
            capture_output=True,
            text=True,
            check=False,
        )
        if res.returncode != 0:
            for stream in (res.stdout, res.stderr):
                for line in stream.splitlines():
                    stripped = line.strip()
                    if stripped:
                        errors.append(f"FAIL (policy): {stripped}")

    return errors


def _run_doxygen(project_root: Path, scratch_dir: Path, files: List[Path]) -> Path:
    """Runs Doxygen into .tmp/agent-gcc/doxygen_check and returns the XML path."""
    if scratch_dir.exists():
        shutil.rmtree(scratch_dir)
    scratch_dir.mkdir(parents=True, exist_ok=True)

    doxy_lines = [
        'PROJECT_NAME           = "EDG GCC Backend"',
        f'OUTPUT_DIRECTORY       = "{scratch_dir}"',
        "GENERATE_XML           = YES",
        "GENERATE_HTML          = NO",
        "GENERATE_LATEX         = NO",
        "EXTRACT_ALL            = YES",
        "EXTRACT_PRIVATE        = YES",
        "EXTRACT_STATIC         = YES",
        "MACRO_EXPANSION        = YES",
        "EXPAND_ONLY_PREDEF     = YES",
        "QUIET                  = YES",
        "WARNINGS               = YES",
        "WARN_IF_UNDOCUMENTED   = YES",
        "WARN_NO_PARAMDOC       = YES",
        "NUM_PROC_THREADS       = 1",
    ]
    for fpath in files:
        doxy_lines.append(f'INPUT                 += "{fpath}"')

    predefs = (
        "BACK_END_IS_GCC_GEN_BE=1",
        "GCC_GEN_BE_NODISCARD=",
        "GCC_GEN_BE_NOEXCEPT=noexcept",
        "EDG_BE_NODISCARD=",
        "EDG_BE_NOEXCEPT=noexcept",
        "BEGIN_EDG_NAMESPACE=",
        "END_EDG_NAMESPACE=",
        "INT128_EXTENSIONS_ALLOWED=1",
        "FIXED_POINT_ALLOWED=1",
        "C99_IL_EXTENSIONS_SUPPORTED=1",
        "GNU_VECTOR_TYPES_ALLOWED=1",
        "NEED_NAME_MANGLING=1",
    )
    for predef in predefs:
        doxy_lines.append(f"PREDEFINED            += {predef}")

    doxyfile_path = scratch_dir / "Doxyfile"
    doxyfile_path.write_text(chr(10).join(doxy_lines) + chr(10), encoding="utf-8")

    res = subprocess.run(
        ["doxygen", str(doxyfile_path)],
        cwd=str(project_root),
        capture_output=True,
        text=True,
        check=False,
    )
    if res.returncode != 0:
        raise RuntimeError(f"doxygen failed with code {res.returncode}: {res.stderr}")
    return scratch_dir / "xml"


def _check_doxygen_xml(xml_dir: Path, target_names: Set[str]) -> List[str]:
    """Inspects Doxygen XML compounds for 100% module, file, type, member, and fn docs."""
    errors: List[str] = []
    if not xml_dir.is_dir():
        return ["FAIL: Doxygen XML output directory was not generated."]

    seen_groups = 0

    for xml_path in sorted(xml_dir.glob("*.xml")):
        if xml_path.name == "index.xml":
            continue
        try:
            tree = ET.parse(xml_path)
        except ET.ParseError as exc:
            errors.append(f"FAIL: Malformed Doxygen XML {xml_path.name}: {exc}")
            continue
            
        root = tree.getroot()
        for compound in root.findall("compounddef"):
            kind = compound.get("kind", "")
            name = compound.findtext("compoundname") or "<unnamed>"

            if kind == "group":
                seen_groups += 1
                brief = compound.find("briefdescription")
                if not _has_text(brief):
                    errors.append(f"FAIL: Module group '{name}' is missing @brief.")
                continue

            if kind == "file":
                if name not in target_names:
                    continue
                brief = compound.find("briefdescription")
                details = compound.find("detaileddescription")
                if not _has_text(brief):
                    errors.append(f"FAIL: File {name} is missing @brief.")
                if not _has_text(details):
                    errors.append(f"FAIL: File {name} is missing @details.")

            if kind in ("struct", "class", "union", "enum"):
                loc_el = compound.find("location")
                loc_file = (
                    Path(loc_el.get("file", "")).name if loc_el is not None else ""
                )
                if loc_file and loc_file not in target_names:
                    continue
                brief = compound.find("briefdescription")
                if not _has_text(brief):
                    errors.append(
                        f"FAIL: {kind} {name} ({loc_file}) is missing @brief."
                    )

            for member in compound.findall(".//memberdef"):
                mkind = member.get("kind", "")
                mname = member.findtext("name") or "<unnamed>"
                mloc = member.find("location")
                mfile = Path(mloc.get("file", "")).name if mloc is not None else name
                if mfile not in target_names:
                    continue

                if mkind in ("enumvalue", "variable", "typedef", "define"):
                    brief = member.find("briefdescription")
                    if not _has_text(brief):
                        errors.append(
                            f"FAIL: {mkind} '{mname}' in {mfile} is missing @brief."
                        )

                if mkind == "enum":
                    ebrief = member.find("briefdescription")
                    if not _has_text(ebrief):
                        errors.append(
                            f"FAIL: enum '{mname}' in {mfile} is missing @brief."
                        )
                    for ev in member.findall("enumvalue"):
                        ev_name = ev.findtext("name") or "<enumvalue>"
                        ev_brief = ev.find("briefdescription")
                        ev_details = ev.find("detaileddescription")
                        if not _has_text(ev_brief) and not _has_text(ev_details):
                            errors.append(
                                f"FAIL: enumerator '{ev_name}' in {mfile} is missing @brief."
                            )

                if mkind == "function":
                    brief = member.find("briefdescription")
                    details = member.find("detaileddescription")
                    if not _has_text(brief):
                        errors.append(
                            f"FAIL: Function '{mname}' in {mfile} is missing @brief."
                        )
                    if not _has_text(details):
                        errors.append(
                            f"FAIL: Function '{mname}' in {mfile} is missing @details."
                        )

                    type_el = member.find("type")
                    type_str = (
                        "".join(type_el.itertext()).strip()
                        if type_el is not None
                        else ""
                    )
                    if (
                        mfile.startswith("gcc_gen_be_")
                        or mfile.startswith("gcc_be_eh_rt")
                    ) and mname not in _ALLOWED_NON_ERROR_FUNCTIONS:
                        if "gcc_gen_be_error_t" not in type_str:
                            errors.append(
                                f"FAIL: Backend function '{mname}' in {mfile} returns "
                                f"'{type_str}' instead of 'gcc_gen_be_error_t'."
                            )
                        argsstr = member.findtext("argsstring") or ""
                        if (
                            "noexcept" not in argsstr
                            and "GCC_GEN_BE_NOEXCEPT" not in argsstr
                        ):
                            errors.append(
                                f"FAIL: Backend function '{mname}' in {mfile} is missing "
                                "GCC_GEN_BE_NOEXCEPT / noexcept."
                            )

                    if type_str not in ("", "void", "static void", "extern void"):
                        ret = (
                            details.find('.//simplesect[@kind="return"]')
                            if details is not None
                            else None
                        )
                        if ret is None or not _has_text(ret):
                            errors.append(
                                f"FAIL: Function '{mname}' in {mfile} returning '{type_str}' "
                                "is missing @return."
                            )

                    documented_params: Set[str] = set()
                    if details is not None:
                        for paramlist in details.findall(
                            './/parameterlist[@kind="param"]'
                        ):
                            for pitem in paramlist.findall("parameteritem"):
                                pdesc = pitem.find("parameterdescription")
                                if not _has_text(pdesc):
                                    continue
                                for pnamelist in pitem.findall("parameternamelist"):
                                    for pnm in pnamelist.findall("parametername"):
                                        if pnm.text:
                                            documented_params.add(pnm.text.strip())

                    for param in member.findall("param"):
                        ptype_el = param.find("type")
                        ptype = (
                            "".join(ptype_el.itertext()).strip()
                            if ptype_el is not None
                            else ""
                        )
                        pname_el = param.find("declname")
                        if pname_el is None or not pname_el.text:
                            if ptype in ("", "void", "..."):
                                continue
                            errors.append(
                                f"FAIL: Function '{mname}' in {mfile} has unnamed parameter "
                                f"of type '{ptype}'."
                            )
                            continue
                        pname = pname_el.text.strip()
                        if pname == "void":
                            continue
                        if pname not in documented_params:
                            errors.append(
                                f"FAIL: Function '{mname}' in {mfile} is missing @param "
                                f"documentation for '{pname}'."
                            )

    if seen_groups == 0:
        errors.append("FAIL: Zero Doxygen @defgroup modules found across GCC backend!")

    deduped: List[str] = []
    seen_err: Set[str] = set()
    for err in errors:
        if err not in seen_err:
            seen_err.add(err)
            deduped.append(err)
    return deduped


def main() -> int:
    """Runs full Doxygen + policy verification for Agent B files."""
    project_root = Path(__file__).resolve().parent.parent
    if not (project_root / "src" / "gcc_gen_be_error.c").is_file():
        print("ERROR: Could not locate src/gcc_gen_be_error.c", file=sys.stderr)
        return 1

    files = _collect_target_files(project_root)
    target_names = {f.name for f in files}
    tmp_parent = project_root / ".tmp"
    tmp_parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(dir=str(tmp_parent)) as tmp_str:
        scratch_dir = Path(tmp_str)
        all_errors = _check_source_file_headers_and_policy(project_root, files)
        xml_dir = _run_doxygen(project_root, scratch_dir, files)
        all_errors.extend(_check_doxygen_xml(xml_dir, target_names))

    if all_errors:
        for msg in all_errors[:120]:
            print(msg)
        if len(all_errors) > 120:
            print(f"... and {len(all_errors) - 120} additional issue(s).")
        print(
            f"Documentation/policy check failed: {len(all_errors)} issue(s) across "
            f"{len(files)} file(s)."
        )
        return 1

    print(
        "100.00% Documentation (file, module, type, member, function, arg, return) "
        f"& Error/noexcept Policy verified across {len(files)} file(s)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
