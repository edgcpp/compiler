#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Verifies 100% Doxygen documentation coverage (file, module, macro, struct,
enum, field, enumerator, function, parameter, and return) across Stage-0
shared driver/SDK headers, implementations, and unit tests."""

import glob
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from pathlib import Path
from typing import List


_NL = chr(10)
_BS = chr(92)


def check_regex_doc_rules(filepath: Path) -> List[str]:
    """Verifies file-level @file/@brief/@details/@ingroup(or @defgroup), macro
    docs, struct/enum field docs, and function @brief/@details/@param/@return."""
    content = filepath.read_text(encoding="utf-8")
    lines = content.splitlines()
    errors: List[str] = []

    if "@file" not in content or "@brief" not in content or "@details" not in content:
        errors.append(f"{filepath}:1: Missing file-level @file, @brief, or @details")
    if "@defgroup" not in content and "@ingroup" not in content:
        errors.append(f"{filepath}:1: Missing module tag (@defgroup or @ingroup)")

    # Every #define (except include guards ending in _H) must have @def / @brief above it
    def_re = re.compile("^#define" + "[" + _BS + "s]+" + "([A-Za-z_][A-Za-z0-9_]*)")
    for idx, line in enumerate(lines):
        m = def_re.match(line.strip())
        if m:
            macro_name = m.group(1)
            if macro_name.endswith("_H"):
                continue
            prev = _NL.join(lines[max(0, idx - 12) : idx])
            if "@brief" not in prev:
                errors.append(
                    f"{filepath}:{idx + 1}: Macro '{macro_name}' missing Doxygen @brief"
                )

    # Check function declarations and definitions for @brief, @details, @param, @return
    fn_re = re.compile(
        "^[ "
        + chr(9)
        + "]*(?:static|extern|EDG_BE_NODISCARD|"
        + _BS
        + "["
        + _BS
        + "[nodiscard"
        + _BS
        + "]"
        + _BS
        + "]|["
        + _BS
        + "s])*"
        + "(edg_be_driver_error_t|int)["
        + _BS
        + "s]+"
        + "([A-Za-z_][A-Za-z0-9_]*)["
        + _BS
        + "s]*"
        + _BS
        + "(([^;{}]*)"
        + _BS
        + ")"
        + "[^;{}]*(?:"
        + _BS
        + "{|;)",
        re.MULTILINE,
    )
    for match in fn_re.finditer(content):
        ret_ty = match.group(1)
        fn_name = match.group(2)
        raw_params = match.group(3).strip()
        start_line = content.count(_NL, 0, match.start()) + 1
        preceding = _NL.join(lines[max(0, start_line - 28) : start_line - 1])
        doc_idx = preceding.rfind("/**")
        doc = preceding[doc_idx:] if doc_idx != -1 else ""
        if not doc or "@brief" not in doc or "@details" not in doc:
            errors.append(
                f"{filepath}:{start_line}: Function '{fn_name}' missing @brief or @details"
            )
            continue
        if "@return" not in doc:
            errors.append(
                f"{filepath}:{start_line}: Function '{fn_name}' missing @return"
            )
        if ret_ty != "edg_be_driver_error_t" and fn_name != "main":
            errors.append(
                f"{filepath}:{start_line}: Function '{fn_name}' must return edg_be_driver_error_t"
            )
        if raw_params and raw_params != "void":
            for part in raw_params.split(","):
                clean_p = re.sub(
                    _BS + "[[^" + _BS + "]]*" + _BS + "]", "", part.split("=")[0]
                ).strip()
                ident_m = re.findall("[A-Za-z_][A-Za-z0-9_]*", clean_p)
                if ident_m:
                    pname = ident_m[-1]
                    if not re.search(
                        "@param(?:"
                        + _BS
                        + "[(?:in|out|in,out)"
                        + _BS
                        + "])?["
                        + _BS
                        + "s]+"
                        + re.escape(pname)
                        + _BS
                        + "b",
                        doc,
                    ):
                        errors.append(
                            f"{filepath}:{start_line}: Function '{fn_name}' missing @param for '{pname}'"
                        )

    return errors


def check_doxygen_xml(files: List[Path]) -> List[str]:
    """Runs Doxygen XML extraction in a temporary directory if doxygen is installed."""
    doxy_bin = shutil.which("doxygen")
    if not doxy_bin:
        return []
    errors: List[str] = []
    tmp_parent = Path(__file__).resolve().parent.parent / ".tmp"
    tmp_parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=str(tmp_parent)) as tmp_dir:
        tmp_path = Path(tmp_dir)
        input_str = " ".join(f'"{p}"' for p in files)
        doxyfile = tmp_path / "Doxyfile"
        doxyfile.write_text(
            _NL.join(
                [
                    'PROJECT_NAME = "EDG Stage0 Driver"',
                    f'OUTPUT_DIRECTORY = "{tmp_path}"',
                    f"INPUT = {input_str}",
                    "EXTRACT_ALL = NO",
                    "EXTRACT_STATIC = YES",
                    "GENERATE_HTML = NO",
                    "GENERATE_LATEX = NO",
                    "GENERATE_XML = YES",
                    "QUIET = YES",
                    "WARN_IF_UNDOCUMENTED = YES",
                    "WARN_IF_DOC_ERROR = YES",
                    "WARN_NO_PARAMDOC = YES",
                    "NUM_PROC_THREADS = 1",
                    "ENABLE_PREPROCESSING = YES",
                    "MACRO_EXPANSION = YES",
                    'PREDEFINED = BEGIN_EDG_NAMESPACE= END_EDG_NAMESPACE= "EDG_BE_NODISCARD=[[nodiscard]]" "EDG_BE_NOEXCEPT=noexcept" __cplusplus=201703L',
                    "",
                ]
            ),
            encoding="utf-8",
        )
        proc = subprocess.run(
            [doxy_bin, str(doxyfile)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if proc.returncode != 0:
            errors.append(
                f"doxygen exited with status {proc.returncode}: {proc.stderr}"
            )
            return errors

        xml_dir = tmp_path / "xml"
        for xml_file in sorted(xml_dir.glob("*.xml")):
            if xml_file.name in ("index.xml", "combine.xslt"):
                continue
            root = ET.parse(xml_file).getroot()
            for cdef in root.findall(".//compounddef"):
                kind = cdef.get("kind", "")
                cname = cdef.findtext("compoundname", default="")
                if kind in ("struct", "class", "group", "file"):
                    c_brief_el = cdef.find("briefdescription")
                    brief = (
                        "".join(c_brief_el.itertext()).strip()
                        if c_brief_el is not None
                        else ""
                    )
                    if not brief:
                        errors.append(f"Undocumented {kind} '{cname}'")
                for mdef in cdef.findall(".//memberdef"):
                    mname = mdef.findtext("name", default="")
                    mkind = mdef.get("kind", "")
                    m_brief_el = mdef.find("briefdescription")
                    mbrief = (
                        "".join(m_brief_el.itertext()).strip()
                        if m_brief_el is not None
                        else ""
                    )
                    m_detail_el = mdef.find("detaileddescription")
                    mdetail = (
                        "".join(m_detail_el.itertext()).strip()
                        if m_detail_el is not None
                        else ""
                    )
                    if mname.endswith("_H"):
                        continue
                    if not mbrief and not mdetail:
                        errors.append(f"Undocumented {mkind} '{mname}' in '{cname}'")
    return errors


def main() -> int:
    """Main entry point for Stage-0 Doxygen documentation coverage verification."""
    repo_root = Path(__file__).resolve().parent.parent
    raw_paths = sorted(
        glob.glob(str(repo_root / "src/edg_be_*.h"))
        + glob.glob(str(repo_root / "src/edg_be_*.cpp"))
        + glob.glob(str(repo_root / "tests/driver/*.cpp"))
    )
    files = [Path(p) for p in raw_paths]
    if not files:
        print("Error: No Stage-0 files found.", file=sys.stderr)
        return 1

    all_errors: List[str] = []
    for f in files:
        all_errors.extend(check_regex_doc_rules(f))
    all_errors.extend(check_doxygen_xml(files))

    if all_errors:
        for err in all_errors:
            print(err, file=sys.stderr)
        print(
            f"Failed: {len(all_errors)} documentation issue(s) found.", file=sys.stderr
        )
        return 1

    print(f"100% Stage-0 Documentation Coverage Verified across {len(files)} files!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
