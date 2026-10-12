---
name: edg-cpfe-bisect
description: >-
  Finds the commit that introduced a test regression or improved behavior
  between known good and bad front-end revisions using
  edg-docker-test-bisect (and edg-bisect for manual steps). Use when
  troubleshooting a failing or newly-passing test across two commits,
  searching for the first bad commit, or locating a fix commit.
---

# EDG cpfe test bisection

Prefer Docker. Put `dev_tools/bin` on `PATH` (direnv /
[HACKING.md](../../../HACKING.md)). Project root = directory containing
`src/cfe.c` and `dev_tools/`.

Full tutorial: [dev_annex/tutorials/TEST_BISECT.md](../../../dev_annex/tutorials/TEST_BISECT.md).
Build/test/record recipes: skill **edg-cpfe-build-test**.

## When to use

You need a **test that differs** between two revisions, plus:

| Goal | Newer tip | Older tip |
| --- | --- | --- |
| Find the **regressing** commit | bad (fails) | good (passes) |
| Find the **fixing** commit | good (passes) | bad (fails) |

The CLI detects which situation you mean after verifying that the test
behaves differently on the two commits.

## Automatic bisect

Canonical CLI: **`edg-docker-test-bisect`** (not `edg-docker-bisect`).

```bash
edg-docker-test-bisect [--posture POSTURE] <bad_commit> <good_commit> <test_path>
```

- **Overwrites** front-end sources during the run; restores them to **HEAD**
  afterward (HEAD itself is not rewritten as history).
- Tip: `--posture=quick-c-fe` for speed (C-generating `edg_x86_64` only).

### Test path forms

- `sandbox/foo.sft.cpp` (short form; preferred)
- `tests/sandbox/foo.sft.cpp`
- `tests/tests/sandbox/foo.sft.cpp`
- Or a cwd-relative / absolute path into a valid suite

If the reproducer is throwaway, put it under `sandbox`.

## Manual bisect

Wrapper around git-bisect style steps: `edg-bisect`
(`start|good|bad|skip|reset|…`). Use when automatic mode is too heavy or you
need to mark skips yourself.

## Checklist

1. Confirm the test **fails on bad** and **passes on good** (or the inverse
   for a fix hunt) with `edg-docker-test --non-interactive …`.
2. Run `edg-docker-test-bisect` with those commits and the test path.
3. After it finishes, sources should match **HEAD**; verify with `git status`.
4. Inspect the identified commit; do not commit or push unless asked.
