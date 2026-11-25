#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Unit tests for dev_tools/check_error_and_noexcept_policy.py."""

import importlib.util
import tempfile
import unittest

from pathlib import Path


_NL = chr(10)
_SCRIPT_PATH = (
    Path(__file__).resolve().parent.parent / "check_error_and_noexcept_policy.py"
)
_SPEC = importlib.util.spec_from_file_location(
    "check_error_and_noexcept_policy", _SCRIPT_PATH
)
assert _SPEC is not None and _SPEC.loader is not None
_POLICY = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_POLICY)


class TestErrorAndNoexceptPolicy(unittest.TestCase):
    """Verifies both compliant C++ code and detection of all forbidden patterns."""

    def test_compliant_source_passes(self) -> None:
        """A function returning edg_be_driver_error_t with noexcept and new(std::nothrow) passes."""
        code = _NL.join(
            [
                "#include <new>",
                "edg_be_driver_error_t helper_fn(int* out_val) noexcept {",
                "  int* p = new (std::nothrow) int(42);",
                "  if (p == nullptr) {",
                "    return edg_be_driver_error_t::out_of_memory;",
                "  }",
                "  *out_val = *p;",
                "  delete p;",
                "  return edg_be_driver_error_t::ok;",
                "}",
                "",
            ]
        )
        with tempfile.TemporaryDirectory() as tmp:
            fpath = Path(tmp) / "good.cpp"
            fpath.write_text(code, encoding="utf-8")
            self.assertEqual(_POLICY.check_file_policy(fpath), [])
            self.assertEqual(_POLICY.main([str(fpath)]), 0)

    def test_detects_void_cast_and_static_cast_void(self) -> None:
        """Detects (void)fn() and static_cast<void>(fn())."""
        code = _NL.join(
            [
                "edg_be_driver_error_t bad_fn(void) noexcept {",
                "  (void)other_call();",
                "  static_cast<void>(other_call());",
                "  return edg_be_driver_error_t::ok;",
                "}",
                "",
            ]
        )
        with tempfile.TemporaryDirectory() as tmp:
            fpath = Path(tmp) / "bad_void.cpp"
            fpath.write_text(code, encoding="utf-8")
            errs = _POLICY.check_file_policy(fpath)
            self.assertEqual(len(errs), 2)
            self.assertEqual(_POLICY.main([str(fpath)]), 1)

    def test_detects_exceptions_missing_noexcept_wrong_return_and_throwing_new(
        self,
    ) -> None:
        """Detects missing noexcept, non-enum return, try/catch/throw, and throwing new."""
        code = _NL.join(
            [
                "int wrong_sig(int x) {",
                "  int* p = new int(x);",
                "  try { throw 1; } catch (...) {}",
                "  return *p;",
                "}",
                "",
            ]
        )
        with tempfile.TemporaryDirectory() as tmp:
            fpath = Path(tmp) / "bad_exc.cpp"
            fpath.write_text(code, encoding="utf-8")
            errs = _POLICY.check_file_policy(fpath)
            self.assertGreaterEqual(len(errs), 4)


if __name__ == "__main__":
    unittest.main()
