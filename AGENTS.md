# EDG C++ Front End (cpfe) — agent notes

This monorepo is the **Edison Design Group C and C++ front end**: a large
C/C++ codebase under `src/` (mostly `.c` compiled as C++), an in-tree test
kit under `tests/`, Docker/dev tooling under `dev_tools/`, and benchmarks
under `benchmarks/`. Detect the project root via `src/cfe.c` and `dev_tools/`.

Human docs: [HACKING.md](HACKING.md), [BUILD.md](BUILD.md),
[CONTRIBUTING.md](CONTRIBUTING.md), [dev_annex/tutorials/](dev_annex/tutorials/),
https://edgcpp.org/doc/.

## Version control

**Never commit** in this repository unless the user explicitly asks for a
commit *in this conversation*. Finish the work, leave it staged or in the
working tree, and say so. This holds even when the change is complete,
tested, and obviously correct, and even when a commit was just requested for
a different repository or topic.

**Never push** to a shared remote unless the user explicitly asks to push.

Tests live in-tree at `tests/` (same clone). A request to commit or push
test changes still requires an explicit ask; it does not authorize unrelated
`src/` commits.

## Monorepo layout

| Path | Role |
| --- | --- |
| `src/` | Front end sources (`cfe.c` is `main`) |
| `tests/` | Test home: `tests/tests/`, `tests/expectations/`, `tests/runs/` |
| `dev_tools/bin/` | CLIs (`edg-exec`, `edg-docker-test`, …); put on `PATH` via direnv |
| `dev_tools/pylibs/` | EDG Python libraries |
| `benchmarks/` | Cachegrind benches, runs, baselines |
| `bases/docker/dev-env/gcc` | Default `EDG_BASE` for docker day posture |
| `build/gcc` | Default day-posture build dir (`linux-gcc-debug`) |
| `util/` | Front-end utilities |

Optional env: `EDG_BASE`, `EDG_CONFIG`, `EDG_TEST_HOME` (override test home;
must contain `.edgy/config.json`). Default test home is `<repo>/tests`.

## Build and test

Prefer the **Docker** workflow. Do not build or run the project test suite on
the host toolchain for normal development; use `edg-exec` /
`edg-docker-test`. Setup: direnv (or add `dev_tools/bin` to `PATH` and
`dev_tools/pylibs` to `PYTHONPATH`), then `dev-init.py docker`. See
[HACKING.md](HACKING.md).

For full recipes (presets, filters, recording), read the project skill
**edg-cpfe-build-test**. For good/bad commit bisection, read
**edg-cpfe-bisect**.

Agent-friendly defaults:

- `edg-docker-test --non-interactive …` — does not launch AcknowlEDG.
- `edg-docker-test-bisect` — automatic regression/fix bisect (skill
  **edg-cpfe-bisect**;
  [TEST_BISECT.md](dev_annex/tutorials/TEST_BISECT.md)).

## Source overview (`src/`)

`cfe.c` is the `main` path; `fe_init.c` / `fe_common.h` wire initialization.
Shared **IL** lives in `il.h` / `il.c` and `il_*` / `lower_*` modules.
Large TUs: `decls.c`, `expr.c`, `statements.c`, `types.c`. Lexer:
`lexical.c` / `preproc.c` / `macro.c`. Symbols: `symbol_tbl.c`; lookup:
`lookup.c`. Templates: `templates.c` (scan/rescan, substitution). Overload
resolution: `overload.c` (sensitive; broad changes may need full-suite
checks).

Details: skills **edg-cpfe-frontend** (module map) and **edg-cpfe-style**
(coding style). Changelog encoding: skill **edg-changes-example**.

## Comments and coding style (`src/`)

Canonical rules: [doc/source/code_style.rst](doc/source/code_style.rst).
Agent workflow and section map: skill **edg-cpfe-style**. Philosophy notes:
[doc/source/int_overview.rst](doc/source/int_overview.rst) (Coding Philosophy).

Hard always-on reminders (details in the RST / skill):

- `/* … */` comments; function prose **after** the declarator; **79** columns.
- Snake case; type names use `a_` / `an_` prefixes. Casts like `(char*)p`.
- At most one `return`, at the end of the function. Match neighbors; do not
  reflow unrelated code.

## Editing `Changes` files

Any `Changes` changelog in the tree (not only `src/Changes`) must be written
and kept as **valid UTF-8**. Prefer UTF-8 writes when adding or editing
entries.

Examples that need non-UTF-8 bytes must use `edg-changes-example` (`«HHHH…»`
markers) and the standard note line — see skill **edg-changes-example**. After
edits, verify with Python that the file still decodes as UTF-8 (see skill
**edg-changes-example**).

## Interesting options

`--c++11` … `--c++26`; `--gnu=XXYYZZ`; `--clang_v=XXYYZZ`;
`--microsoft_v=XXYYZZ`; `-d-xyz` / `-dN` (needs a **DEBUG** build).

## AcknowlEDG (`dev_tools/`)

Interactive review UI + provider CLIs for tests and benchmarks. Canonical
Python package: `dev_tools/pylibs/edgacknowledg/`. The path
`dev_tools/services/acknowledg/server/edgacknowledg` is a **symlink** to that
package — edit pylibs only. Wire protocol:
[dev_tools/services/acknowledg/PROTOCOL.md](dev_tools/services/acknowledg/PROTOCOL.md).

Skills: **acknowledg-client**, **acknowledg-bench**. Python conventions for
`dev_tools/` (no formal RST): skill **edg-python-style**.

## Agent skills (portable)

Project skills follow the open [Agent Skills](https://agentskills.io/)
format (`SKILL.md` + `name` / `description` frontmatter) and live only under
[`.agents/skills/`](.agents/skills/). Clients that implement the standard
(Codex, Zed, VS Code, Cursor, and others) discover them there.

If an agent does not auto-load skills, open the matching
`.agents/skills/<name>/SKILL.md` when the task matches its description.

| Skill | Path | Load when |
| --- | --- | --- |
| `edg-cpfe-build-test` | [`.agents/skills/edg-cpfe-build-test/SKILL.md`](.agents/skills/edg-cpfe-build-test/SKILL.md) | Building, `edg-docker-test`, recording |
| `edg-cpfe-bisect` | [`.agents/skills/edg-cpfe-bisect/SKILL.md`](.agents/skills/edg-cpfe-bisect/SKILL.md) | Good/bad commit bisect for a test regression or fix |
| `edg-cpfe-frontend` | [`.agents/skills/edg-cpfe-frontend/SKILL.md`](.agents/skills/edg-cpfe-frontend/SKILL.md) | Editing `src/`, IL/templates/overload, `Changes` |
| `edg-cpfe-style` | [`.agents/skills/edg-cpfe-style/SKILL.md`](.agents/skills/edg-cpfe-style/SKILL.md) | Front-end coding style (`doc/source/code_style.rst`) |
| `edg-python-style` | [`.agents/skills/edg-python-style/SKILL.md`](.agents/skills/edg-python-style/SKILL.md) | Python under `dev_tools/` (no formal RST; match in-tree) |
| `edg-changes-example` | [`.agents/skills/edg-changes-example/SKILL.md`](.agents/skills/edg-changes-example/SKILL.md) | Encoding non-UTF-8 examples in any `Changes` file |
| `acknowledg-client` | [`.agents/skills/acknowledg-client/SKILL.md`](.agents/skills/acknowledg-client/SKILL.md) | Batch/Direct client, reconnect, provider CLIs |
| `acknowledg-bench` | [`.agents/skills/acknowledg-bench/SKILL.md`](.agents/skills/acknowledg-bench/SKILL.md) | `.cgout` / Ir / baselines / bench review |
