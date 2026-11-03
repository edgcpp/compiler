/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

// Display content as providers send it: `content`, maybe with
// `lines_truncated`, the 1-based numbers of the lines they cut (see
// MAX_DISPLAY_LINE_SIZE in PROTOCOL.md). Nothing in the content marks a cut;
// the lines are marked here.

// U+2026 HORIZONTAL ELLIPSIS (JavaScript has no named escapes).
const TRUNCATION_MARK = '\u2026'

// The lines of `content`, those numbered in `linesTruncated` ending in an
// ellipsis; none for null content.
export const displayLines = (content, linesTruncated) => {
  if (content === null || content === undefined) {
    return []
  }
  const lines = content.split('\n')
  for (const lineNumber of linesTruncated) {
    lines[lineNumber - 1] += TRUNCATION_MARK
  }
  return lines
}
