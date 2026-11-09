---
name: edg-agent-temp-files
description: >-
  Where agents put temporary files and directories (scratchpad, the project's
  .tmp/agent-* directory, or the container's /tmp), how to share them with the
  docker container via edg-exec, and how to run edgy ad hoc with --runs-dir
  (and a scratch EDG_TEST_HOME) instead of edg-docker-test. Use when creating
  scratch inputs or outputs, running throwaway tests or recordings, or cleaning
  up temporary state.
---

# Temporary state and the docker boundary

Tools run either on the host or in the docker container (`edg-exec`). The
container sees **only the project root**, mounted at `/edg/workspace`; the host
`/tmp` and your scratchpad are not visible to it. Project root = directory
containing `src/cfe.c` and `dev_tools/`.

## Where to put temporary state

| Need | Put it in | Notes |
| --- | --- | --- |
| Host-only (scripts, outputs of host tools) | your scratchpad directory | no permission prompts |
| Shared with the container | `<root>/.tmp/agent-<purpose>` | seen as `/edg/workspace/.tmp/agent-<purpose>`; `.tmp/` is gitignored |
| Only needed inside the container | the container's `/tmp` | ephemeral, nothing is left on the host |

- Everything you create in `.tmp/` **must** be named `agent-<purpose>` (the
  rest of `.tmp/` is the user's host-to-docker bridge; see `.tmp/README.md`).
- Do not use `build/`, `tests/runs/`, or the host `/tmp` for scratch state.
- Files created in the container are owned by the host user, so they are
  directly usable from the host.

## edg-exec rules

- Run `edg-exec` from the project root (the current directory is mounted as
  `/edg/workspace`).
- `edg-exec` joins its arguments with spaces and **does not preserve
  quoting**: `edg-exec -- sh -c '. env.sh; cmd'` runs `sh -c .` and drops the
  rest. Pass a compound command as ONE quoted string:

  ```bash
  edg-exec -- "mkdir -p .tmp/agent-foo && echo hi > .tmp/agent-foo/x"
  edg-exec --dry-run -- "..."     # shows exactly what the container will run
  ```

- `edgy` and the other `dev_tools/bin` tools are on the container `PATH`.

## Running edgy outside the standard workflow

`edg-docker-test` is for the standard front-end change workflow, where the
user wants results kept in `tests/runs/` (see **edg-cpfe-build-test**). For
anything else (experiments, verification of tooling, comparisons), run `edgy`
with `--runs-dir` so nothing lands in `tests/runs/`:

```bash
# In docker: results appear on the host under .tmp/agent-foo/runs/<timestamp>/
edg-exec -- ". build/gcc/environment.sh && edgy --runs-dir=/edg/workspace/.tmp/agent-foo/runs regressions/foo.sft.cpp"
```

- `. build/<dir>/environment.sh` provides `CPFE`/`CPFE_CP`, which `edgy` uses
  to find the front end (use the build directory you are testing). The default
  configs need both `cpfe` and `cpfe-cp` built; `-c edg_x86_64` runs the C
  config alone.
- Use an **absolute** container path for `--runs-dir`. `edgy` copies the front
  end under the runs directory and a relative path breaks tools that change
  directory (e.g. `cpfe-cp-gen-be` fails with "No such file or directory").
- Host-side tools that take `--runs-dir` (e.g. `edg-rematerialize-test-run`)
  have no docker constraint: point them at the scratchpad.
- If `edg-docker-test` is still used for a throwaway run, pass
  `--discard-test-results` (results stay in the container's `/tmp`).

## Scratch test home (for `-W` and other mutating experiments)

`edg-docker-test` does not forward a host `EDG_TEST_HOME`, but setting it
**inside** the container command works. Build a small test home and point at
it; never run `-W` against the real tree:

```bash
H=.tmp/agent-foo/home
mkdir -p $H/tests
cp -r tests/.edgy $H/.edgy
cp -r tests/expectations $H/expectations
cp -r tests/tests/<suite> $H/tests/<suite>      # only the suites you need
CMD='. build/gcc/environment.sh && EDG_TEST_HOME=/edg/workspace/.tmp/agent-foo/home edgy --runs-dir=/edg/workspace/.tmp/agent-foo/runs <suite>/<test>'
edg-exec -- "$CMD"                    # read-only first: logs must land under .tmp/agent-foo
edg-exec -- "${CMD/edgy /edgy -W }"   # then record (expectations and .rto files in the scratch home only)
```

Check `git status --short tests` and that `tests/runs/` did not gain an entry.

## Cleanup

- Remove the `.tmp/agent-*` paths **you created** in a batch step as the LAST
  step before reporting results to the user (not after each sub-task), in ONE
  command that lists them (one approval instead of many):

  ```bash
  rm -rf .tmp/agent-foo .tmp/agent-bar
  ```

- Never glob other entries, never remove non-`agent-` names in `.tmp/`, and
  do not delete `tests/runs/` content (it is gitignored and may be the user's).
- Say in the final summary what you created and removed.
