# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''Tests for edgutil.

Run with: python3 -m unittest discover -s dev_tools/tests
(with dev_tools/pylibs on PYTHONPATH).
'''

import asyncio
import os
import signal
import subprocess
import sys
import textwrap
import time
import unittest

import edgutil

class TerminateWithinTests(unittest.IsolatedAsyncioTestCase):
  async def test_terminates(self) -> None:
    process = await asyncio.create_subprocess_exec('sleep', '30')
    started = time.monotonic()
    status = await edgutil.async_terminate_within(process, 5)
    self.assertEqual(status, -signal.SIGTERM)
    self.assertLess(time.monotonic() - started, 2)

  async def test_kills_what_outlasts_its_allowance(self) -> None:
    # It ignores SIGTERM (as exec keeps ignored signals ignored).
    process = await asyncio.create_subprocess_exec(
      'sh', '-c', 'trap "" TERM; echo ready; exec sleep 30',
      stdout = asyncio.subprocess.PIPE
    )
    assert process.stdout is not None
    self.assertEqual(await process.stdout.readline(), b'ready\n')
    started = time.monotonic()
    status = await edgutil.async_terminate_within(process, 0.5)
    self.assertEqual(status, -signal.SIGKILL)
    self.assertGreaterEqual(time.monotonic() - started, 0.5)

  async def test_kills_when_cancelled_while_waiting(self) -> None:
    process = await asyncio.create_subprocess_exec(
      'sh', '-c', 'trap "" TERM; echo ready; exec sleep 30',
      stdout = asyncio.subprocess.PIPE
    )
    assert process.stdout is not None
    self.assertEqual(await process.stdout.readline(), b'ready\n')
    terminating = asyncio.create_task(
      edgutil.async_terminate_within(process, 30)
    )
    await asyncio.sleep(0.2)
    terminating.cancel()
    with self.assertRaises(asyncio.CancelledError):
      await terminating
    self.assertEqual(await asyncio.wait_for(process.wait(), 5),
                     -signal.SIGKILL)

  async def test_leaves_what_has_exited(self) -> None:
    process = await asyncio.create_subprocess_exec('true')
    await process.wait()
    self.assertEqual(await edgutil.async_terminate_within(process, 5), 0)

# Each runs in a process of its own: run_asyncio_loop installs its loop (and
# signal handlers) for the process.
_PROGRAM = textwrap.dedent('''
  import asyncio, os, signal, sys
  import edgutil

  scenario = sys.argv[1]

  async def clean_up_slowly(name):
    try:
      await asyncio.sleep(100)
    finally:
      await asyncio.sleep(0.1)
      print(name, 'cleaned up', flush = True)

  async def consumer(loop):
    loop.create_task(clean_up_slowly('worker'))
    await asyncio.sleep(0.05)
    if scenario == 'fatal':
      raise edgutil.AsyncLoopFatalError()
    os.kill(os.getpid(), signal.SIGINT)
    try:
      await asyncio.sleep(100)
    except asyncio.CancelledError:
      # Starting a task while stopping: it's cancelled, and awaited, too.
      loop.create_task(clean_up_slowly('latecomer'))
      if scenario == 'sigint':
        raise
    return 'returned'

  try:
    print(edgutil.run_asyncio_loop(consumer), flush = True)
  except BaseException as ex:
    print('raised', type(ex).__name__, flush = True)
  loop = edgutil.lazy_init_event_loop()
  print('left running:', len(asyncio.all_tasks(loop)), flush = True)
''')

def _run(scenario: str) -> list[str]:
  pylibs = os.path.join(os.path.dirname(__file__), '..', 'pylibs')
  completed = subprocess.run(
    [sys.executable, '-c', _PROGRAM, scenario],
    env = {**os.environ, 'PYTHONPATH': os.path.abspath(pylibs)},
    capture_output = True,
    text = True,
    timeout = 30,
    check = True
  )
  return completed.stdout.splitlines()

class RunAsyncioLoopTests(unittest.TestCase):
  '''Stopping: the tasks it cancels finish their cleanup.'''

  def test_fatal_errors_wait_for_cancelled_tasks(self) -> None:
    self.assertEqual(_run('fatal'), [
      'worker cleaned up',
      'raised AsyncLoopFatalError',
      'left running: 0',
    ])

  def test_signals_wait_for_cancelled_tasks(self) -> None:
    lines = _run('sigint')
    self.assertEqual(sorted(lines[:2]),
                     ['latecomer cleaned up', 'worker cleaned up'])
    self.assertEqual(lines[2:], ['raised CancelledError', 'left running: 0'])

  def test_signals_caught_wait_for_cancelled_tasks(self) -> None:
    lines = _run('sigint-caught')
    self.assertEqual(sorted(lines[:2]),
                     ['latecomer cleaned up', 'worker cleaned up'])
    self.assertEqual(lines[2:], ['returned', 'left running: 0'])

if __name__ == '__main__':
  unittest.main()
