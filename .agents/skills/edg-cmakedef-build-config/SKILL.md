---
name: edg-cmakedef-build-config
description: >-
  Explains EDG macro/build configurations: the .cmakedef files under
  cmake/macro-conf, the edg-cmakedef resolver, EDG_MACRO_CONF selection, and
  CMakePresets.json. Use when adding or changing a macro for a build, creating
  a new configuration or variant, finding out why a macro is (not) set, or
  choosing a preset. For running builds and tests, use edg-cpfe-build-test.
---

# EDG cmakedef build configurations

A **macro configuration** is a named set of `.cmakedef` files that decide
which preprocessor macros each binary is compiled with. CMake resolves them
with `dev_tools/bin/edg-cmakedef`. Project root = directory containing
`src/cfe.c` and `dev_tools/`.

Source of truth (read these rather than trusting this summary):

- `dev_tools/bin/edg-cmakedef` — the resolver / language.
- `cmake/macro-conf.cmake` — CMake glue.
- `cmake/host-defaults.cmake` — auto-selection when nothing is specified.
- `cmake/macro-conf/` — the configurations.
- `CMakePresets.json` — presets that pick a configuration.

## The cmakedef language

One statement per line; blank lines and `#` comments are ignored.

| Line | Meaning |
| --- | --- |
| `NAME` | define `NAME` (emitted as `NAME=1` in `#define` output) |
| `NAME=value` | define with a value (may be empty) |
| `import <dir/file>` | process `cmake/macro-conf/dir/file.cmakedef` here (root-relative, no extension) |
| `delete NAME` | remove a prior definition; **error** if not set |
| `delete_if_set NAME` | remove a prior definition; message only if not set |

- Later lines override earlier ones (including across imports), so order
  matters: import the base first, then add overrides.
- `@VAR@` in a value is expanded by CMake (`string(CONFIGURE)`).
- Any unparsable line or failing `delete` makes the file invalid; the
  CMake configure then fails with "Failed to process cmakedef."

## Layout of `cmake/macro-conf/`

- `<config>/<target>.cmakedef`, one file per target. Targets and who reads
  them: `base` (`src/CMakeLists.txt`, emitted as a generated
  `cmake_defines.h` plus `USE_CMAKE_DEFINES`), `cpfe`, `cpfe-cp`, `cdisp`
  (`src/cmake/<target>/CMakeLists.txt`) and `cpprtlib`
  (`lib_src/CMakeLists.txt`); the last four become `-D` flags.
- `default/` — baseline each config imports first
  (e.g. `default/cpfe` sets `BACK_END_IS_C_GEN_BE`, `DO_IL_LOWERING`).
  `default/base.cmakedef` is a "should not be used" stub.
- `support/` — reusable fragments: `platform/`, `arch/`, `compiler/`,
  `build-type/`, `simple-conf/`, `feature/` (`on`/`off`), `target/`.
  Fragments are per-target too (e.g. `support/build-type/debug/cpfe`).
- Named configs (`linux-gcc`, `linux-gcc-release`, `linux-clang`,
  `linux-i686-gcc`, `macos-arm-clang`, `windows-msvc`, …). Variants layer on
  a parent: `linux-gcc-multi-thread-release/cpfe.cmakedef` imports
  `<linux-gcc-release/cpfe>` and then appends its own macros.
- A config directory missing a target file is auto-populated by CMake with
  `import <default/<target>>`; don't rely on that for real configs.

## Selection

- Single-config generators (Ninja): cache var `EDG_MACRO_CONF`.
- Multi-config generators: `EDG_DEBUG_MACRO_CONF` and
  `EDG_RELEASE_MACRO_CONF`.
- If empty, `host-defaults.cmake` picks one from OS, arch, compiler, and
  build type (`detect_host_defaults`); otherwise CMake warns.
- Presets set it explicitly, e.g. `linux-gcc-debug` →
  `EDG_MACRO_CONF=linux-gcc`, binary dir `build/gcc`.
- Every cmakedef file reached is added to `CMAKE_CONFIGURE_DEPENDS`, so
  editing one re-runs configure on the next build.

## Recipes

Inspect what a config resolves to (host Python, read-only, no build):

```bash
edg-cmakedef cmake/macro-conf linux-gcc/cpfe.cmakedef print-values
edg-cmakedef --verbose cmake/macro-conf linux-gcc/cpfe.cmakedef print-values
edg-cmakedef cmake/macro-conf linux-gcc/base.cmakedef print-defines
edg-cmakedef cmake/macro-conf linux-gcc/cpfe.cmakedef print-paths
```

- The target is resolved against the root if relative (CMake passes
  absolute paths); never repeat the root in it.
- `print-values` → `NAME=value` lines; `print-defines` → `#ifndef/#define`
  blocks (the `base` form); `print-paths` → the files visited.
- `--verbose` writes the import chain and each Setting / Overwriting /
  Deleting step to **stderr**, which shows exactly which file last set a
  macro. Use it to answer "why is X set?".
- Compare two configs by diffing the `print-values` output (sort first).

Change a macro for one build: edit the narrowest file that covers it.
Shared by everything → `default/<target>`; one platform / build type /
compiler → the matching `support/…` fragment; one named config or variant →
that config's `<target>.cmakedef`. Check all targets that need it
(`base` vs `cpfe` vs `cpprtlib` are separate files).

Add a new configuration:

1. Copy the nearest sibling directory under `cmake/macro-conf/` with all
   its target files; keep imports of `default/` and `support/` fragments.
2. If it is a variant, import the parent's file instead of copying it.
3. Add a preset to `CMakePresets.json` (inherit `linux-common` or the
   platform's common preset; set `EDG_MACRO_CONF`, `binaryDir`).
4. If it should be auto-selected, add a branch in
   `cmake/host-defaults.cmake` (and note release builds use the
   `<name>-release` directory).
5. Validate with the `print-values` recipe for every target, then
   configure/build in Docker (see **edg-cpfe-build-test**):

   ```bash
   edg-exec -- cmake --preset <preset>
   edg-exec --wdir build/<dir> -- ninja
   ```

## Pitfalls

- `import <…>` paths are relative to `cmake/macro-conf`, with no extension.
- `delete` of a never-set key fails the configure; use `delete_if_set` when
  the key might legitimately be absent.
- A macro in a cmakedef only matters if the source consumes it; the generated
  `cmake_defines.h` is only used when `USE_CMAKE_DEFINES` is set. Editing
  `src/defines.h` (BUILD.md) does not change what a CMake configuration
  produces.
- `print-paths` output order is unspecified (set iteration).
- Windows uses `dev_tools/win-bin/edg-cmakedef.bat`; the Python is
  type-checked by `dev_tools/typecheck-python.sh` (follow
  **edg-python-style** if editing the tool).
