---
name: edg-spelling-allowlist
description: >-
  How the pre-push spell check (edg-check-file-changes, aspell, edgspelling)
  decides what to report, and how to add words to the hierarchical spelling
  allowlist (dev_tools/spelling_allowlist.json). Use when the pre-push hook
  or edg-collect-changes --check reports misspelled words, when choosing
  where a new allowed word belongs, or when editing the allowlist file.
---

# Spell check and the spelling allowlist

## How the check works

The pre-push hook (`.git-hooks/pre-push.py`) runs
`dev_tools/bin/edg-check-file-changes` on every changed file, through
`edg-exec` when docker is preferred.  Its spelling pass:

- Runs `aspell list` on the text.  For code files (`.c`, `.cpp`, `.h`,
  `.hpp`) only the comments are checked (extracted with `get-comments`).
  Other files are checked whole.
- First splits identifiers into words (`edgspelling.preprocess`): `fooBar`
  becomes `foo Bar`.  Underscores already split words, so `foo_bar` is
  checked as `foo` and `bar`.
- Reports only words that are **new** relative to the old version of the
  file, minus the allowlist words for that file's path.
- For pending Changes entries (`*/pending_changes/*.txt`), also accepts
  the entry's `//allowed_words:` directives and any word already in the
  target `Changes` file (see skill **edg-changes-example**).  CI runs the
  same check with `edg-collect-changes --check`.

The hook skips some paths entirely (`EXCLUDES_PATH_PREFIXES` in
`.git-hooks/pre-push.py`), including `AGENTS.md`, `.agents/`, and the
allowlist itself.

## The allowlist file

`dev_tools/spelling_allowlist.json` is a tree of nodes.  Each node may have:

| Key | Meaning |
| --- | --- |
| `words` | Sorted lowercase words, allowed in any capitalization |
| `names` | Sorted words allowed only with exactly this capitalization |
| `paths` | Object mapping one path component (directory or file name) to a child node |

A file's allowed words are the union of the nodes on the way down to it, so a
word in the root node is allowed everywhere.  The format is documented in
`dev_tools/pylibs/edgspelling/__init__.py`.

Rules that `edgspelling.Allowlist` enforces when the file loads (a bad
file makes the check refuse to run and fails `ShippedAllowlistTests` in CI):

- Each list is sorted by code point and has no duplicates.
- `words` entries are lowercase.  Use `names` when the capitalization must
  match (proper nouns like `EDG` or `Coverity`).
- Only `words`, `names`, and `paths` keys are allowed.
- Each entry is one word.  `foo_bar` or `fooBar` would never match, because
  identifiers are split before lookup.  Add the parts instead.

## Choosing where a word goes

Put a word in the **narrowest node** that covers the places it's used
often (see "Prompting is the default" below):

- **Root:** terms used across the project (C++ terminology, `src`, `utf`).
- **A directory node** (`src`, `dev_tools`, `dev_tools/pylibs/edgtest`,
  `doc/source`, …): jargon used only there.  Common Python names like
  `isinstance` and `pathlib`, and the pylib package names (`edgutil`, …),
  live in `dev_tools`.  The rare uses elsewhere (`dev-init.py`,
  `.git-hooks/`, `doc/source/conf.py`) are left to prompt.

A word used in only one file almost never earns an entry.  The check
reports only words that are new to a file, so the entry would save a prompt
only when the word shows up in a second file.  A word used only in a
`Changes` file never needs one, because pending entries already accept
every word in their target `Changes` file.
- **A file node:** a one-off term in a single file.

### Prompting is the default

Each allowed word is a blind spot: a typo that happens to match it is never
reported anywhere the entry applies.  A broader node makes the blind spot
bigger.  Being prompted again costs little, because the check reports only
words that are new to a file.  Weigh the two:

- Prefer leaving a word out and letting people be prompted.  Add it only when
  people would otherwise be prompted for it very often (for example, a term
  used constantly across a directory or the project).
- When a word does earn an entry, choose the narrowest node that stops the
  frequent prompts, even if rare uses elsewhere still prompt.  For example,
  the Python names are in `dev_tools` rather than at the root.
- Be most careful with short words and words that are one letter away from
  a common English word (`len`, `str`, `dir`).  They're the likeliest to hide
  a real typo.

Don't add a real misspelling.  Fix the text.  For a Changes entry, prefer an
`//allowed_words:` directive over a new allowlist word unless the term will
recur.

## Editing the file

Keep the existing formatting (one-space JSON indent, UTF-8, non-ASCII kept as
is).  A Python round trip keeps it exact and keeps the lists sorted:

```python
import json, pathlib
path = pathlib.Path('dev_tools/spelling_allowlist.json')
tree = json.loads(path.read_text(encoding = 'utf-8'))
tree['words'] = sorted(set(tree['words']) | {'newword'})
# For a nested node: tree['paths']['src']['words'] = ...
path.write_text(
  json.dumps(tree, indent = 1, ensure_ascii = False) + '\n',
  encoding = 'utf-8'
)
```

Then check that the file still loads, and run the tests:

```sh
PYTHONPATH=dev_tools/pylibs python3 -c 'import edgspelling; edgspelling.Allowlist()'
PYTHONPATH=dev_tools/pylibs python3 -m unittest discover -s dev_tools/tests \
  -p 'test_edg_check_file_changes.py'
```

To see what aspell rejects in some text:
`printf '%s\n' word1 word2 | aspell list`.
