---
name: edg-cpfe-build-test
description: >-
  Builds and tests the EDG C++ front end in Docker via edg-exec and
  edg-docker-test, and records expectations with -W. Use when building
  cpfe, running edgy filters, recording output, or choosing postures.
  For good/bad commit bisection, use skill edg-cpfe-bisect.
---

# EDG cpfe build and test

Prefer Docker. Put `dev_tools/bin` on `PATH` (direnv / [HACKING.md](../../../HACKING.md)).
Project root = directory containing `src/cfe.c` and `dev_tools/`.

## Build

Day posture (debug GCC):

```bash
edg-exec -- cmake --preset linux-gcc-debug
edg-exec --wdir build/gcc -- ninja
edg-exec --wdir build/gcc -- ninja cpfe      # C++ → lowered C (.int.c)
edg-exec --wdir build/gcc -- ninja cpfe-cp   # C++ → C++ from IL
```

- Default dirs: `build/gcc`, `bases/docker/dev-env/gcc` (`EDG_BASE`).
- Clang: preset `linux-clang-debug` → `build/clang`.
- Shell in container (cwd → `/edg/workspace`): `edg-exec` or `edg-docker-shell`.
- If `src/defines.h` is swapped for reproduction, one binary may not build;
  that can be expected. Prefer CMake presets from `CMakePresets.json` when
  possible ([BUILD.md](../../../BUILD.md)).

## Test

```bash
edg-docker-test --non-interactive              # day posture, no AcknowlEDG
edg-docker-test --non-interactive rma          # ~2k tests, quick smoke
edg-docker-test --non-interactive --posture=quick-c-fe sandbox/foo
edg-docker-test --dry-run rma
```

- **`--non-interactive`**: do not launch AcknowlEDG (required for unattended
  agent runs).
- Filters: one edgy-style argument (comma-separated OK).
- Postures: `day`, `night`, `quick-c-fe`, `extensive`, or `--posture=@file.json`.
- Results: `tests/runs/<timestamp>/<config>/` (e.g. `changes.elog`,
  `executed.elist`). Empty / no unexpected diffs means a clean match.
- Optional override: `EDG_TEST_HOME` (must contain `.edgy/config.json`).

First-test walkthrough: [dev_annex/tutorials/FIRST_TEST.md](../../../dev_annex/tutorials/FIRST_TEST.md).

## Recording expected output

1. Add or fix the test under `tests/tests/<suite>/`.
2. Record:

   ```bash
   edg-docker-test --non-interactive -W path/within/suite
   ```

3. Recordings live beside the test in a `.<name>.rto/` directory. Prefer
   committing **`default.*`** expectation files, not config-specific names
   that are gitignored. See edgy helpers in `dev_tools/pylibs/edgtest`.
4. Re-run without `-W` to confirm a clean match before asking to commit.

## Finding a regressing or fixing commit

Use skill **edg-cpfe-bisect** (`edg-docker-test-bisect` /
[TEST_BISECT.md](../../../dev_annex/tutorials/TEST_BISECT.md)).

## Broad / risky changes

For fixes that may change diagnostics or IL widely (e.g. `overload.c`):

1. Rebuild in Docker and compare with
   `edg-docker-test --non-interactive …`. Do **not** invent a local `-W`
   baseline just for the change. Expectations on **`origin/main`** are kept
   stable by CI and the Docker workflow, so a clean tree at that tip should
   match recorded output; diffs that appear in this run’s elog files are
   (with high confidence) caused by the change under test.
2. Inspect `tests/runs/<timestamp>/<config>/changes.elog` (and
   `improvements.elog` / `regressions.elist`). Expect a few intentional
   diffs; no true regressions.
3. Only use `-W` when deliberately updating expectations for those
   intentional diffs — never as a substitute for the `origin/main`
   baseline.

Full-suite runs are slow (~tens of minutes); prefer targeted filters when safe.
