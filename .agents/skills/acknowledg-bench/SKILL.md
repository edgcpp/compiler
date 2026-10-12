---
name: acknowledg-bench
description: >-
  Reads Cachegrind .cgout Ir totals, compares benchmark baselines to runs,
  and supports AcknowlEDG bench review / update-baseline flows. Use when
  editing edg-bench-run-delta, edgacknowledg.client.bench, .cgout summary
  parsing, or benchmark UI ops fields.
---

# AcknowlEDG / Cachegrind benchmarks

## Metric

**Ir** (instruction reads) is the first `summary:` integer in a `.cgout`.
CLIs and the UI call it **OPs**. Lower run Ir vs baseline = faster.

```text
events: Ir I1mr ILmr Dr ...
...
summary: 3696685 6148 4719 ...
```

With cache-sim enabled there are multiple counts; **always use the first**
(Ir). Do not sum fields or take a later column.

Shared regex (keep both call sites in sync):

```python
_REGEX_CGOUT_SUMMARY = re.compile(r'^summary: ([0-9]+)(?: [0-9]+)*\n$')
```

## Readers

| Location | Function |
| --- | --- |
| [`dev_tools/bin/edg-bench-run-delta`](../../../dev_tools/bin/edg-bench-run-delta) | `read_program_totals` |
| [`dev_tools/pylibs/edgacknowledg/client/bench/__init__.py`](../../../dev_tools/pylibs/edgacknowledg/client/bench/__init__.py) | same |

Readers scan the last ~10 lines for `summary:`. Missing/malformed → skip that
benchmark. Do not invent a third independent parser.

Percent change:

```text
((baseline - run) / max(1, baseline)) * 100
```

## Layout

Bench home: `$EDG_BENCH_HOME` or `<repo>/benchmarks`.

| Path | Role |
| --- | --- |
| `benchmarks/benchmarks/**/*.bnch.cpp` | Sources |
| `benchmarks/runs/<YYYY.MM.DD-HH.MM.SS>/…/*.bnch.cgout` | Runs |
| `benchmarks/baselines/project/<tag>/` | Project baselines |
| `benchmarks/baselines/user/<name>/` | User baselines (`@name` tag) |
| `benchmarks/.acknowledg-cache/` | `cg_annotate` disk cache |

Relative `.cgout` paths must exist under both run and baseline dirs to compare.

## Review / UI principles

Wire payloads should carry raw `baseline_ops` / `run_ops` (and baseline tag
when needed). Derive in the client:

- op delta, percent change, faster/slower badges
- mean / median / population stdev over the list
- sort order

Do **not** ship redundant counts, samples arrays, or precomputed change
comments when the UI already has the ops list.

Bench annotate detail uses `cg_annotate --no-annotate --diff` (delta output,
not a line-oriented source diff).

## Update baseline

Bench editor copies the selected run `.cgout` into the session’s baseline tag
directory and refreshes statuses (`BenchEditHandler` /
`edg-bench-review`).

## Protocol

Tags `bench.src.*` / `bench.edit.*`: see
[PROTOCOL.md](../../../dev_tools/services/acknowledg/PROTOCOL.md).
Client architecture: skill **acknowledg-client**.
Python under `dev_tools/`: skill **edg-python-style**.
