// Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://edgcpp.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Finds the source locations EDG diagnostics point at in front-end output.
//
// Recognized forms (EDG's own output only):
//
//   "file.c", line 12: error: message
//   "file.c", line 12 (col. 5): warning: message
//       detected during instantiation of "..." at line 17
//       the other declaration is at line 3 of "tu2.cpp"
//
// A whole diagnostic looks like this:
//
//   "Test_name.c", line 8: error: bad thing        <- header (line i)
//       int line8(void);                           <- source excerpt (i+1)
//       ^                                          <- caret line (i+2)
//             detected during instantiation of "f" at line 22
//                                                  <- continuation (i+3...)
//
// Every header, and every continuation that names a location, becomes one
// result, so each can get its own marker in the UI.  The header's own file
// and line are what a marker opens; the excerpt and caret are not parsed,
// since the source itself can be shown instead.

// The header of a diagnostic.  Groups: 1 file, 2 line, 3 column (optional),
// 4 severity ("error", "warning", "note", "remark", "catastrophic error",
// "internal error", ...).  Starts with a quote, so most lines are rejected
// by the first character.
const HEADER = /^"([^"]+)", line (\d+)(?: \(col\. (\d+)\))?: ([a-z][a-z -]*): /
// A location named on an indented continuation line, in another file:
//   the other declaration is at line 3 of "tu2.cpp"
const CONT_OF_FILE = /\bline (\d+) of "([^"]+)"/
// ...or in the same file as the header, which is not repeated:
//   detected during instantiation of "..." at line 17
const CONT_SAME_FILE = /\bat line (\d+)\b/
// The caret line under a source excerpt: nothing but whitespace and "^".
const CARET = /^\s*\^\s*$/

// Returns [{ outputLine, file, line, col, severity }], with `outputLine`
// zero-based (an index into `text.split('\n')`) and `col` possibly null.
// Continuation lines report severity 'context'.
export const parseDiagnostics = (text) => {
  const lines = text.split('\n')
  const found = []
  // The file named by the most recent header, while its diagnostic is still
  // in progress (so "at line N" continuations can be attributed to it); null
  // otherwise.
  let headerFile = null
  // The source excerpt and caret under a header are program text, not
  // diagnostic text.  A line of source could contain anything, including
  // `line 3 of "x.h"`, which would be misread as a continuation; so, while
  // `i <= skipUntil`, lines are ignored.
  let skipUntil = -1
  for (let i = 0; i < lines.length; i++) {
    const lineText = lines[i]
    // A header: record it, then move on to the next line.
    if (lineText.charCodeAt(0) === 34 /* " */) {
      const m = HEADER.exec(lineText)
      if (m !== null) {
        headerFile = m[1]
        found.push({
          outputLine: i,
          file: m[1],
          line: parseInt(m[2], 10),
          col: m[3] === undefined ? null : parseInt(m[3], 10),
          severity: m[4]
        })
        // The excerpt (i + 1) and caret (i + 2) follow.  Recognized by the
        // caret, so that a header with no excerpt (brief output) does not
        // swallow its continuations.
        if (i + 2 < lines.length && CARET.test(lines[i + 2])) {
          skipUntil = i + 2
        }
        continue
      }
    }
    // Skip excerpt/caret lines, and anything outside a diagnostic.
    if (i <= skipUntil || headerFile === null) {
      continue
    }
    const c = lineText.charCodeAt(0)
    if (c !== 32 && c !== 9) {
      // Continuations are indented; anything else ends the diagnostic.
      headerFile = null
      continue
    }
    // An indented line may name another location, either in another file...
    let m = CONT_OF_FILE.exec(lineText)
    if (m !== null) {
      found.push({
        outputLine: i,
        file: m[2],
        line: parseInt(m[1], 10),
        col: null,
        severity: 'context'
      })
      continue
    }
    // ...or in the header's file.
    m = CONT_SAME_FILE.exec(lineText)
    if (m !== null) {
      found.push({
        outputLine: i,
        file: headerFile,
        line: parseInt(m[1], 10),
        col: null,
        severity: 'context'
      })
    }
  }
  return found
}

// As `parseDiagnostics`, for a diff of outputs: only the added ("+") lines
// are searched, and `outputLine` indexes the diff's lines.
export const parseDiffDiagnostics = (text) => {
  // Other lines are blanked rather than dropped, keeping indices stable and
  // ending any diagnostic in progress.
  const added = text.split('\n').map(
    (lineText) => lineText.charCodeAt(0) === 43 /* + */ ?
      lineText.slice(1) : ''
  )
  return parseDiagnostics(added.join('\n'))
}

// The name the test harness compiles the primary source file as, unless the
// test's //name: directive says otherwise.  The server reports the actual
// name (`primary_name` of `test.src.list-assoc-files`).
export const DEFAULT_PRIMARY_SOURCE_NAME = 'Test_name.c'

// Maps a file named by a diagnostic to the value of `assoc_file` for a
// `test.src.show-src` request: `null` is the test's primary file.
//
// `assocFiles` and `primaryName` come from the `test.src.list-assoc-files`
// reply; until it arrives (`assocFiles` is null) nothing can be located.
//
// Returns `{ assocFile }` when the file can be located, or `null` when it
// cannot (not in `assocFiles`, or several candidates with that name).
export const resolveDiagnosticFile = (file, assocFiles, primaryName) => {
  if (assocFiles === null) {
    return null
  }
  if (file === primaryName) {
    return { assocFile: null }
  }
  const wanted = file.replace(/^(\.\/)+/, '')
  if (assocFiles.includes(wanted)) {
    return { assocFile: wanted }
  }
  const baseOf = (path) => path.slice(path.lastIndexOf('/') + 1)
  const matches = assocFiles.filter((f) => baseOf(f) === baseOf(wanted))
  return matches.length === 1 ? { assocFile: matches[0] } : null
}
