---
name: edg-changes-example
description: >-
  Writes Changes entries as pending_changes/*.txt files (collected
  nightly by edg-collect-changes, checked with edg-collect-changes --check),
  and encodes/decodes non-UTF-8 example bytes with edg-changes-example
  («HHHH...» markers). Use when adding a Changes entry, editing any Changes
  file (src/Changes, util/Changes, …) or pending_changes/ entry, adding
  examples with Latin-1/EUC-JP/Shift-JIS/other non-UTF-8 bytes, or verifying
  changelog encoding.
---

# Changes entries, `edg-collect-changes`, and `edg-changes-example`

## Policy (all `Changes` files)

Any path named `Changes` under the monorepo (for example `src/Changes`,
`util/Changes`, `lib_src/Changes`, `include_c++/Changes`, `nt_util/Changes`)
must remain **valid UTF-8** on disk.

- Write and save Changes entries as **UTF-8**.
- Ordinary Unicode that is valid UTF-8 may appear as real characters (or as
  C/C++ escapes such as `\u00b6` in examples). Do **not** wrap valid UTF-8
  in `«…»` unless you intentionally need a raw-byte example
  (`encode --escape-all-non-ascii`).
- Examples that need **non-UTF-8** bytes (legacy Latin-1, EUC-JP, Shift-JIS,
  etc.) must use `«HHHH…»` markers produced by `edg-changes-example`, never
  raw non-UTF-8 bytes in the committed file.

CLI: `dev_tools/bin/edg-changes-example` (on `PATH` when direnv is active).

## Note line (required for encoded examples)

When an entry’s example contains one or more `«…»` hex markers, add this
note on its own line after the example, matching existing changelog style:

```text
Note: The above example must be decoded using the edg-changes-example tool.
```

Use that exact wording. Place it immediately after the example block (blank
line before the next dated entry is fine).

## Encode (raw → UTF-8 Changes text)

Given a fragment or file that still contains raw non-UTF-8 bytes:

```bash
edg-changes-example encode raw-fragment.txt > utf8-fragment.txt
# or:
edg-changes-example encode -o utf8-fragment.txt raw-fragment.txt
printf '...' | edg-changes-example encode
```

Default encode keeps maximal runs of bytes `>= 0x80` that form valid UTF-8,
and replaces only non-UTF-8 runs with one marker `«` + hex + `»` (two hex
digits per byte, no separators). `\uXXXX` in the text is left alone.

Optional: `--escape-all-non-ascii` escapes every byte `>= 0x80`, including
valid UTF-8 sequences (rare; only when the example must show raw bytes).

## Decode (UTF-8 Changes text → raw bytes)

To rebuild the real example bytes (for local reproduction, not for commit):

```bash
edg-changes-example decode utf8-fragment.txt > raw-fragment.txt
edg-changes-example decode -o raw-fragment.txt utf8-fragment.txt
# Whole file (write host-only output to your scratchpad, see
# edg-agent-temp-files):
edg-changes-example decode src/Changes > <scratchpad>/Changes.raw
```

Decode replaces each `«HHHH…»` (or legacy `«HH»«HH»`) with the corresponding
bytes. No other escapes are interpreted.

## Workflow for a new changelog entry

New entries are **not** written into a `Changes` file. Each `Changes` file
has a `pending_changes/` directory beside it (`src/pending_changes/`,
`util/pending_changes/`, …); add one entry file there per change, named
`*.txt` (suggested: `gh-<N>-<short-slug>.txt`). A nightly job
(`.github/workflows/collect-changes.yml`, midnight US Eastern) runs
`edg-collect-changes`, which moves every entry into its `Changes` file under
the newest `Version x.y` line, adds the date, orders that night's entries by
summary line, and deletes the entry files.

Entry file format:

```text
//allowed_words: reflectable memoized
[GH #335]
Type transformations of qualified function types

Previously, the __add_lvalue_reference, __add_rvalue_reference, ...
For example, with --c++11:

  using F = void() const;
  using F = __add_pointer(F);  // Previously a spurious error, now okay.
```

- Optional leading `//key: value` directives (EDG C++ line directive
  syntax). `//allowed_words:` lists space-separated words the spell check
  should accept (identifiers, jargon); repeat the line as needed. Words the
  target `Changes` file already uses (`constexpr`, `lvalue`, …) are accepted
  without it. Directives are dropped when the entry is collected.
- The tag block: one or more comma-separated `GH #N` tags in brackets
  (`[GH #12,GH #34]`; only `GH #N`, never `[]`). It may wrap onto several
  lines; the tool re-wraps it the way existing entries are wrapped.
- **No date.** The tool adds it (`m/d/yy`, padded to 8 columns) in front of
  the tags.
- The summary line, one blank line, then the description, in the same style
  as existing entries (unindented prose wrapped at 79 columns, two spaces
  after periods, examples indented 2 spaces).
- UTF-8, no tabs, at most **79** columns per line. Trailing whitespace is
  tolerated (it's stripped on collection).

Steps:

1. Draft the entry as UTF-8 prose in
   `<dir>/pending_changes/<name>.txt` (the `pending_changes/` beside
   the `Changes` file the entry belongs in, usually `src/`).
2. If an example needs non-UTF-8 bytes, author the example in a small raw
   file (or produce the markers by hand only when the bytes are already
   known), then run `edg-changes-example encode` and paste the UTF-8 result
   into the entry.
3. Immediately after that example, add:

   `Note: The above example must be decoded using the edg-changes-example tool.`

4. Check the entry (format, spelling, typos, line length). This is the
   check `edg-check-file-changes` does for `pending_changes/*.txt` files,
   which pull request CI runs and which makes the pre-push hook refuse the
   push (no override) if anything is wrong:

   ```bash
   edg-exec -- edg-collect-changes --check src/Changes src/pending_changes
   ```

   (Name the `Changes` file and `pending_changes/` directory the entry is
   in. `edg-exec` runs it in the dev-env container, which has `aspell`.) For
   misspellings that are really identifiers or terms, add them to an
   `//allowed_words:` line at the very top of the entry, as the failure
   message shows; fix real misspellings instead.
5. Optionally preview the result without changing anything:
   `edg-collect-changes --dry-run src/Changes src/pending_changes`.

Edit a `Changes` file directly only to correct an entry that has already
been collected; then write it as **UTF-8** and verify the file still decodes
(raises on failure):

```bash
python3 -c "open('path/to/Changes', encoding='utf-8').read()"
```

## Do not

- Add new entries directly to a `Changes` file, or put a date in a
  `pending_changes/` entry.
- Commit raw Latin-1 / Shift-JIS / etc. bytes inside a `Changes` file.
- Invent alternate note wording; match the existing sentence above.
- Run `encode` on the entire historical `src/Changes` and rewrite it unless
  that is an explicit, reviewed task — encode **new example fragments**, then
  splice the UTF-8 text into the entry.

## Related

Always-on summary: [AGENTS.md](../../../AGENTS.md). Front-end context:
skill **edg-cpfe-frontend**.
