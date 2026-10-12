---
name: edg-changes-example
description: >-
  Encodes and decodes non-UTF-8 example bytes in Changes changelogs with
  edg-changes-example («HHHH...» markers) and writes Changes files as UTF-8.
  Use when editing any Changes file (src/Changes, util/Changes, …), adding
  examples with Latin-1/EUC-JP/Shift-JIS/other non-UTF-8 bytes, or verifying
  changelog encoding.
---

# `edg-changes-example` and Changes files

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
# Whole file:
edg-changes-example decode src/Changes > /tmp/Changes.raw
```

Decode replaces each `«HHHH…»` (or legacy `«HH»«HH»`) with the corresponding
bytes. No other escapes are interpreted.

## Workflow for a new changelog entry

1. Draft the entry as UTF-8 prose.
2. If an example needs non-UTF-8 bytes, author the example in a small raw
   file (or produce the markers by hand only when the bytes are already
   known), then run `edg-changes-example encode` and paste the UTF-8 result
   into the entry.
3. Immediately after that example, add:

   `Note: The above example must be decoded using the edg-changes-example tool.`

4. Insert the entry into the appropriate `Changes` file with a **UTF-8**
   write (keep the rest of the file valid UTF-8).
5. Verify the file still decodes as UTF-8 (raises on failure):

   ```bash
   python3 -c "open('path/to/Changes', encoding='utf-8').read()"
   ```

   Use the real path (for example `src/Changes`). A clean exit means the
   whole file is valid UTF-8; a `UnicodeDecodeError` means the write
   introduced non-UTF-8 bytes and must be fixed before continuing.

## Do not

- Commit raw Latin-1 / Shift-JIS / etc. bytes inside a `Changes` file.
- Invent alternate note wording; match the existing sentence above.
- Run `encode` on the entire historical `src/Changes` and rewrite it unless
  that is an explicit, reviewed task — encode **new example fragments**, then
  splice the UTF-8 text into the entry.

## Related

Always-on summary: [AGENTS.md](../../../AGENTS.md). Front-end context:
skill **edg-cpfe-frontend**.
