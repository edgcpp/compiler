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
  and `dev_tools/pylibs` on `PYTHONPATH` (direnv /
  [HACKING.md](../../../HACKING.md)).

## Indentation and wrapping

- **2-space** indentation (not PEP 8’s 4 spaces).
- Prefer wrapping near **79** columns when practical (same habit as the
  front end); do not mass-reflow unrelated lines.
- Wrap long signatures, calls, and `typing` imports with parentheses; align
  continuations with neighboring EDG Python, not Black/ruff defaults.

### Function calls and wrapped message text

Call-site wrapping differs from the declaration wrapping below. When a
call's arguments need multiple lines:

- Break immediately after the opening `(`; put the arguments on following
  lines, indented **two spaces** from the statement's indentation.
- Once a call is split over multiple lines, put **each positional and
  keyword argument on its own line**, even when several would fit together.
  A single argument may span multiple lines (for example, a long message),
  but do not put another argument on any of those lines.
- Put the closing `)` on its own line, aligned with the statement's start
  (including `raise`, `return`, or `await`). Keep suffixes such as `from
  None` or a `with` statement's `:` after that closing parenthesis.
- Do not leave the first argument beside the opening parenthesis and hang
  subsequent arguments under it. Declaration-style right alignment does
  not apply to calls.

After moving arguments onto new lines, **reflow their text for the new
available width**. Count indentation, quotes, and any `f` prefix against
79 columns. Merge adjacent message literals when the whole argument fits
on one line; otherwise split at word boundaries using the available width.
Do not preserve old string breaks just because they belonged to the old
hanging layout. Preserve exact spaces, interpolation, and rendered text;
implicit concatenation must not introduce a newline into the message.

For example, replace this hanging call:

```python
raise ConfigError(f"{host_key} must be a host name, without a scheme, "
                  f"port, or path")
```

with this layout, whose message now fits on one argument line:

```python
raise ConfigError(
  f"{host_key} must be a host name, without a scheme, port, or path"
)
```

When the message still needs wrapping, reflow the words inside the call:

```python
raise ConfigError(
  f"{port_key} is required for a host other than localhost "
  f"({TLS_PORT} for TLS)"
)
```

For a call with multiple arguments, keep each argument separate:

```python
return replace(
  self,
  service_bind = service_bind,
  builder_service_url = builder_service_url,
  publisher_service_url = publisher_service_url
)
```

Review argument separation, parentheses, and text wrapping in added or
modified calls. An unchanged syntax tree verifies behavior, not readable
layout.

### Multiline collections

Optimize for readability and small future diffs, not fewer lines. In a
multiline list, tuple, set, or dictionary literal, put **one element or
key-value pair per line**, even when several would fit. Keep trailing
commas so adding or removing an element changes only its own line. This
also applies to collections passed to calls such as `frozenset`.
Naturally short single-line collections can remain on one line.

```python
_INT_KEYS = frozenset((
  'max_artifact_bytes',
  'max_extracted_bytes',
  'max_artifact_entries',
))
```

### Function parameter lists

For `def` / `async def` signatures, adapt the **Wrapping Function
Declarations** rules in
[the EDG C++ style guide](../../../doc/source/code_style.rst):

1. First try wrapping with continuation parameters **left-aligned just to
   the right of the opening `(`**, in the first parameter's column. Keep
   initial parameters on the declaration line when they fit.
2. Use that layout only if every resulting line fits within **79 columns**.
   If the declaration overflows and a parameter cannot fit at that column,
   break immediately after the opening `(` and move all parameters below it.
3. In this fallback layout, use a **common indentation column chosen by
   right-aligning the longest continuation line to column 79**. Shorter
   lines start in that same column; do not right-justify each line
   independently. Include closing punctuation and any return annotation
   when measuring the longest line.
4. Treat the keyword-only separator `*,` as belonging to the preceding
   parameter when wrapping. Keep it on that parameter's line whenever it
   fits within 79 columns, **including the first declaration line**:
   write `def with_wiring(self, *,` rather than putting `*,` below `self`.
   The same applies after `cls` or another positional parameter, even when
   all following keyword-only parameters wrap. Do not give `*,` its own
   line when the preceding line has room for it.

When reviewing a wrapped signature, check lines beginning with `*,` and
move the separator onto the preceding line if it fits.

Normal left alignment (including `ForgeStore.open`):

```python
def get_output_filename(env: Dict[str, str],
                        case_number: int,
                        command_number: int) -> str:

async def open(cls, config: DatabaseConfig, *,
               password: Optional[str] = None) -> 'ForgeStore':

def with_wiring(self, *,
                service_bind: Optional[str] = None) -> 'ForgeServiceConfig':
```

Fallback when left alignment would overflow:

```python
async def invoke_recorded_process(
                     base_command: List[str],
                     command_args: List[str],
                     working_dir: Path,
                     injected_command_args: Optional[List[str]] = None) -> int:
```

Return annotations (`-> …`) stay on the last parameter line when they fit
within 79 columns; otherwise wrap similarly to neighbors. These declaration
rules do not change the call-site rules above.

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

## Count checks

When a condition tests whether a collection's element count is zero or
nonzero, prefer explicit `len(items) == 0` or `len(items) != 0` over
collection truthiness (`if not items:` or `if items:`). For example, write
`if len(removed) != 0:` when deciding whether there are removed settings to
report. This rule concerns counts, not boolean flags or optional values.

## Structure

- Modules are flat and explicit; classes are fine for stateful helpers.
- Private helpers / module constants: leading `_`.
- Prefer `async def` + `asyncio` patterns already used in edgy / edgtest over
  ad-hoc threads, unless matching an existing sync API.
- Fail loudly with clear errors; do not add speculative try/except “just in
  case.”

## What not to do

- Do not run Black/ruff/autopep8 in a way that converts the tree to 4-space
  PEP 8, strips spaces around `=`, or replaces the two declaration-wrapping
  layouts above with fixed four-space continuations.
- Do not copy unrelated front-end **C** syntax/layout conventions into
  Python; use the declaration-wrapping adaptation above.
- Do not invent a second string-quoting scheme; follow `f"..."` / `'...'`.

## Related skills

- **acknowledg-client** / **acknowledg-bench** — domain code that should
  follow this style
- **edg-cpfe-build-test** — running tools that live under `dev_tools/`
- **edg-cpfe-style** — C/C++ front-end style only (`doc/source/code_style.rst`)
