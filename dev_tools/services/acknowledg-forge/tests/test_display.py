# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Content cut for display to reviewers: whole, and line by line.'''

import contextlib
import time

from typing import Iterator

import pytest

from edgacknowledg import MAX_DISPLAY_LABEL_SIZE
from edgacknowledg.client import shared
from edgacknowledg.client.shared import (
  display_content,
  trunc_display_content,
  trunc_display_label,
)


@pytest.fixture
def small_limits(monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(shared, 'MAX_DISPLAY_CONTENT_SIZE', 40)
  monkeypatch.setattr(shared, 'MAX_DISPLAY_LINE_SIZE', 8)


def test_short_content_is_untouched(small_limits: None) -> None:
  for content in ('', 'a', 'a\nb\n', 'a\n\n\nb', '12345678\n'):
    assert trunc_display_content(content) == {'content': content}


def test_long_lines_are_cut_and_numbered(small_limits: None) -> None:
  result = trunc_display_content('short\n123456789\nok\n' + 'x' * 20)
  assert result == {
    'content': 'short\n12345678\nok\n' + 'x' * 8,
    'lines_truncated': [2, 4],
  }


def test_long_content_is_cut_at_a_line_end(small_limits: None) -> None:
  # Eight lines of six (with their ends) fit six times in 40.
  lines = [f"line{n}" for n in range(8)]
  result = trunc_display_content('\n'.join(lines))
  assert result == {
    'content': '\n'.join(lines[:6]),
    'truncated': True,
  }


def test_cut_lines_count_as_cut(small_limits: None) -> None:
  # Lines cut, then the whole: only lines kept are listed.
  content = '\n'.join(['y' * 20] * 10)
  result = trunc_display_content(content)
  assert result['truncated']
  assert result['content'] == '\n'.join(['y' * 8] * 4)
  assert result['lines_truncated'] == [1, 2, 3, 4]
  assert len(result['content']) <= 40


def test_one_line_larger_than_everything(
    monkeypatch: pytest.MonkeyPatch) -> None:
  monkeypatch.setattr(shared, 'MAX_DISPLAY_CONTENT_SIZE', 10)
  monkeypatch.setattr(shared, 'MAX_DISPLAY_LINE_SIZE', 100)
  assert trunc_display_content('z' * 50 + '\nmore') == {
    'content': 'z' * 10,
    'truncated': True,
    'lines_truncated': [1],
  }


def test_no_characters_are_lost_between_lines(small_limits: None) -> None:
  # The old walk back dropped the character before a line end.
  lines = ['abcdefg'] * 10
  content = trunc_display_content('\n'.join(lines))['content']
  assert all(line == 'abcdefg' for line in content.split('\n'))


def test_large_content_is_linear() -> None:
  # Many short lines past the content limit, and a huge one, go quickly.
  content = 'a\n' * 8_000_000 + 'b' * 20_000_000
  start = time.monotonic()
  result = trunc_display_content(content)
  assert time.monotonic() - start < 20
  assert result['truncated']
  assert 'lines_truncated' not in result


def test_labels_end_in_an_ellipsis() -> None:
  assert trunc_display_label('PASS') == 'PASS'
  fits = 'x' * MAX_DISPLAY_LABEL_SIZE
  assert trunc_display_label(fits) == fits
  label = trunc_display_label(fits + 'x')
  assert len(label) == MAX_DISPLAY_LABEL_SIZE
  assert label.endswith('\N{HORIZONTAL ELLIPSIS}')


def test_display_reads_only_what_fits(small_limits: None) -> None:
  taken: list[str] = []

  def pieces() -> Iterator[tuple[str, bool]]:
    for n in range(1000):
      taken.append(f"{n}")
      yield f"line{n}\n", True

  with contextlib.closing(pieces()) as generator:
    shown = display_content(generator)
  assert shown['truncated']
  # Up to the first line that doesn't fit, and no more.
  assert len(taken) == 7
  assert shown['content'] == '\n'.join(f"line{n}" for n in range(6))


def test_display_cuts_lines_across_pieces(small_limits: None) -> None:
  # A line in several pieces; then a text without a last line end, which
  # the next line doesn't run into.
  pieces = [
    ('abcde', True), ('fghij', False), ('k\n', False),
    ('no end', True),
    ('# next\n', True),
  ]
  assert display_content(pieces) == {
    'content': 'abcdefgh\nno end\n# next\n',
    'lines_truncated': [1],
  }
