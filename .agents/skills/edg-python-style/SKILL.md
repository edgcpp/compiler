---
name: edg-python-style
description: >-
  Applies EDG Python conventions under dev_tools/ (pylibs and bin CLIs).
  Use when writing or reviewing Python for edgtest, edgutil, edgacknowledg,
  edgdocker, edgy, or other EDG tools. There is no separate formal Python
  style RST — match existing code and these conventions.
---

# EDG Python style

There is **no** dedicated Python style RST. Treat mature trees as normative,
especially:

- [`dev_tools/bin/edgy`](../../../dev_tools/bin/edgy)
- [`dev_tools/pylibs/edgutil/`](../../../dev_tools/pylibs/edgutil/)
- [`dev_tools/pylibs/edgtest/`](../../../dev_tools/pylibs/edgtest/)

Prefer matching the file you are editing over inventing a new dialect.

## Layout and headers

- New scripts under `dev_tools/bin/` start with `#!/usr/bin/env python3` when
  executable.
- Keep the Apache-2.0 WITH LLVM-exception license / SPDX header used by
  neighbors.
- Package code lives under `dev_tools/pylibs/<pkg>/` (importable as
  `edgtest`, `edgutil`, `edgacknowledg`, …). Put `dev_tools/bin` on `PATH`
  and `dev_tools/pylibs` on `PYTHONPATH` (direnv / [HACKING.md](../../../HACKING.md)).

## Indentation and wrapping

- **2-space** indentation (not PEP 8’s 4 spaces).
- Prefer wrapping near **79** columns when practical (same habit as the
  front end); do not mass-reflow unrelated lines.
- Wrap long signatures, calls, and `typing` imports with parentheses; align
  continuations with neighboring EDG Python, not Black/ruff defaults.

### Function parameter lists

If a `def` / `async def` signature does not fit on one line, wrap the
parameter list in the same spirit as EDG C++ (`code_style.rst` — Wrapping
Function Declarations):

1. Prefer keeping the `def` name and opening `(` on the first line when the
   first parameter(s) still fit; continue parameters on following lines.
2. When that is not enough (long name, many parameters, long annotations),
   break immediately after `(` and put parameters on the next lines.
3. **Right-indent** wrapped parameter lines so they end at **column 79**
   (right-aligned to the margin), packing as many parameters per line as
   fit. Do not use a fixed left hanging indent (PEP 8 / Black’s 4-space
   continuation).

```python
def get_output_filename(env: Dict[str, str],
                        case_number: int,
                        command_number: int) -> str:

async def invoke_recorded_process(
                    base_command: List[str], command_args: List[str],
                    working_dir: Path, env: Dict[str, str],
                    build_constants: BuildConstants,
                    output_file_handle, *,
                    filter_command: Optional[str] = None, debug: bool,
                    driver_debug: bool = False,
                    injected_command_args: List[str] = None) -> int:

def parse_test_status_diff(
                         status_line: str) -> Tuple[str, ParsedTestStatusDiff]:
```

Return annotations (`-> …`) stay on the last parameter line when they fit
within 79 columns; otherwise wrap similarly to neighbors.

## Spacing around `=`

Put spaces around `=` in **keyword arguments**, defaults, and similar
bindings (distinct from PEP 8):

```python
proc = await asyncio.create_subprocess_exec(
  *cmd,
  stdout = asyncio.subprocess.PIPE,
  stderr = asyncio.subprocess.STDOUT
)
parser.add_argument('--jobs', dest = 'job_count', type = int)
```

Same for dict/kw literals in project style: `maxsize = None`,
`debug = True`.

## Strings

- Formatted: **`f"..."`** (double quotes).
- Plain / non-interpolated: **`'...'`** (single quotes).
- Prefer f-strings over `%` or `.format()` for new code.

## Docstrings and comments

- Prefer **`'''...'''`** (triple single quotes) for docstrings, matching
  `edgutil` / `edgtest`.
- Comments are concise `#` lines; explain non-obvious intent, not the prior
  state of the code.

## Typing and APIs

- Annotate public functions and `__init__` (`-> None` on constructors).
- Use `typing` names common in-tree (`Dict`, `List`, `Optional`, `Tuple`, …)
  unless the file already uses newer builtins (`list`, `dict`) — match the
  file.
- Keyword-only parameters after `*` are common for options:
  `async def run(..., *, debug: bool)`.
- Prefer `pathlib.Path` over raw path strings for filesystem work.

## Imports

Typical order:

1. Future / preamble hooks if required (e.g. ALSR disable in `edgy`)
2. Stdlib
3. Blank line
4. In-tree packages (`edgtest`, `edgutil`, …)
5. Blank line
6. `from … import …` (stdlib then local), with parenthesized multi-line
   `typing` imports when long

Avoid unused imports; do not introduce heavy formatters that rewrite the
whole tree.

## Structure

- Modules are flat and explicit; classes are fine for stateful helpers.
- Private helpers / module constants: leading `_`.
- Prefer `async def` + `asyncio` patterns already used in edgy / edgtest over
  ad-hoc threads, unless matching an existing sync API.
- Fail loudly with clear errors; do not add speculative try/except “just in
  case.”

## What not to do

- Do not run Black/ruff/autopep8 in a way that converts the tree to 4-space
  PEP 8, strips spaces around `=`, or replaces right-aligned-to-79
  parameter wraps with fixed hanging indents.
- Do not copy front-end **C** style (`edg-cpfe-style`) into Python.
- Do not invent a second string-quoting scheme; follow `f"..."` / `'...'`.

## Related skills

- **acknowledg-client** / **acknowledg-bench** — domain code that should
  follow this style
- **edg-cpfe-build-test** — running tools that live under `dev_tools/`
- **edg-cpfe-style** — C/C++ front-end style only (`doc/source/code_style.rst`)
