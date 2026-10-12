---
name: edg-cpfe-style
description: >-
  Applies EDG C++ front-end coding style under src/ (whitespace, wrapping,
  naming, functions, classes, statements, preprocessor). Use when writing or
  reviewing front-end C/C++ sources, formatting new helpers, or checking
  comment and layout conventions. Canonical rules live in the RST style guide.
---

# EDG cpfe coding style

Canonical guide (read and follow; do not invent parallel rules):

- [doc/source/code_style.rst](../../../doc/source/code_style.rst)

Supporting philosophy / host–target placement notes:

- [doc/source/int_overview.rst](../../../doc/source/int_overview.rst)
  (section **Coding Philosophy**)

Always-on project notes also list a short subset in
[AGENTS.md](../../../AGENTS.md); when the guide and local neighbors disagree
on formatting trivia, **match surrounding code** and prefer the RST for new
prose.

## How to use this skill

1. Open `doc/source/code_style.rst` and apply the section that matches what
   you are editing (whitespace, wrapping, naming, variables, functions,
   lambdas, classes, statements, preprocessor, …).
2. Keep edits within the touch radius of the change; do not reflow unrelated
   code to “fix style.”
3. For changelog encoding (`Changes` files), use skill **edg-changes-example**
   — that is not covered by the C style guide.

## Enumerator constant casts

The code base has many casts of an enumerator constant to its own enum type,
e.g. `(a_type_kind)tk_integer`. This is a historical leftover; do not
preserve it.

- **Do not add** new casts of an enumerator constant to its own enum type.
  Write the bare enumerator (`tk_integer`).
- **Remove** such casts in code adjacent to your change (within the touch
  radius; see "How to use this skill"). Do not sweep the rest of the file or
  tree just to delete them.
- Casts that convert to a *different* type (e.g., to `int` or `char`, or to
  an enum other than the enumerator's own) are not covered by this rule.

## Quick orientation (not a substitute for the RST)

| Topic | Where in `code_style.rst` |
| --- | --- |
| 79 columns, 2-space indent, tab width 8 | General Whitespace Rules |
| Condition / statement wrapping | General Line Wrapping |
| Blank lines around decls / functions | General Line Breaks |
| Snake case; `a_` / `an_` type prefixes | Naming Convention |
| Global/namespace variables + comments | Writing a Variable |
| Parameter columns, docs after `)`, return | Writing a Function |
| Lambdas | Writing a Lambda |
| Classes, members, initializers | Writing a Class |
| `for` / `if` / `switch` / do-while | Statements |
| `#if` / includes layout | Preprocessor Directives |

## Related skills

- **edg-cpfe-frontend** — `src/` module map, options, Changes pointers
- **edg-changes-example** — UTF-8 / non-UTF-8 `Changes` encoding
- **edg-cpfe-build-test** — rebuild and test after style-sensitive edits
- **edg-python-style** — Python under `dev_tools/` (separate conventions)
