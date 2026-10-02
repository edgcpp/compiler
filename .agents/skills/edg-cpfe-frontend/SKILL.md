---
name: edg-cpfe-frontend
description: >-
  Navigates and edits the EDG C++ front end under src/: IL, lexer, symbols,
  lookup, templates, overload resolution, and Changes changelogs. Use when
  changing front-end C sources or debugging semantics. For coding style,
  use skill edg-cpfe-style (canonical doc/source/code_style.rst). For Changes
  encoding, defer to edg-changes-example.
---

# EDG cpfe front-end sources

All paths are relative to the monorepo root.

## Module map

| Area | Primary files |
| --- | --- |
| Entry / init | `src/cfe.c`, `fe_init.c`, `fe_common.h`, `fe_wrapup.c` |
| IL | `il.h`, `il.c`, `il_*`, `lower_*` |
| Decls / expr / stmts | `decls.c`, `expr.c`, `exprutil.c`, `statements.c` |
| Types | `types.c`, related headers |
| Lexer / preprocessor | `lexical.c`, `preproc.c`, `macro.c` |
| Symbols | `symbol_tbl.c` / `a_symbol` |
| Lookup | `lookup.c` (`normal_id_lookup`, `class_qualified_id_lookup`, `IDL_*`) |
| Templates | `templates.c` (scan vs rescan; `copy_type_with_substitution`, …) |
| Overload | `overload.c` (`a_conv_descr`, candidates, conversions) |

Lexer token sequence numbers feed caching, correspondence, and diagnostics.
Templates often separate a first **scan** from **rescans** when earlier
parameters must be known. Overload work is historically regression-sensitive;
prefer the **edg-cpfe-build-test** skill (targeted tests, then broader runs /
bisect) when changing it.

## Style

Do not invent style rules here. Follow skill **edg-cpfe-style** and
[doc/source/code_style.rst](../../../doc/source/code_style.rst).

## Interesting options

Language: `--c++11` … `--c++26`. Emulation: `--gnu=XXYYZZ`,
`--clang_v=XXYYZZ`, `--microsoft_v=XXYYZZ`. Debug: `-d-xyz` /
`db_flag_is_set("xyz")`, `-dN` (levels 1–5). Requires a **DEBUG** build.

## Editing `Changes` files

Write every `Changes` file as **UTF-8** (`src/Changes`, `util/Changes`, and
any other `Changes` in the tree). For non-UTF-8 example bytes, `«HHHH…»`
markers, the required note line, encode/decode commands, and verification,
follow skill **edg-changes-example** — do not invent a parallel procedure here.

## Related skills

- **edg-cpfe-style** — coding style; [code_style.rst](../../../doc/source/code_style.rst).
- **edg-changes-example** — UTF-8 Changes writes; `edg-changes-example` CLI.
- **edg-cpfe-build-test** — rebuild, `edg-docker-test`, recording.
- **edg-cpfe-bisect** — `edg-docker-test-bisect` between good/bad commits.
- Do not invent a separate defect-tracker workflow; use the project's current
  GitHub Discussions / issues process ([CONTRIBUTING.md](../../../CONTRIBUTING.md)).
