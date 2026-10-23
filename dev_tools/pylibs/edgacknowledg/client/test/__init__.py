# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''AcknowlEDG test-review provider package.'''

import asyncio
import os
import re
import shutil
import sys
import tempfile
import traceback

import edgtest
import edgutil

from pathlib import Path, PurePosixPath
from typing import (
  IO,
  Any,
  Dict,
  List,
  Optional,
  Tuple,
)

from edgacknowledg import (
  MAX_DISPLAY_CONTENT_SIZE,
  ProtocolMessage,
  RequestTags,
  WebSocketsMessageType,
  message,
  decode_response,
)
from edgacknowledg.client.shared import (
  BackgroundWorkerPool,
  Channel,
  ConfinedPath,
  IllegalFileAccessError,
  SandboxContext,
  trunc_display_content,
)
from edgutil import eprint

MULTI_CONFIG_TEST_FORMAT = re.compile(r'^\(([a-zA-Z0-9-_]+)\) (.+)$')

_StatusDict = Dict[str, Dict[str, Any]]


# Re-exported: it used to be defined here.
__all__ = ['IllegalFileAccessError']


def _rt_path_for(config_dir: ConfinedPath, test_id: str) -> ConfinedPath:
  return config_dir / str(PurePosixPath(test_id).with_suffix(
    edgtest.runtest.RUN_TEST_OUTPUT_SUFFIX
  ))


def _create_status_dict(elog_file_paths: List[ConfinedPath], *,
                        is_multi_config_run: bool) -> _StatusDict:
  # test_id -> config_name -> combined_diff_hash.
  pending: Dict[str, Dict[str, str]] = {}
  lines_by_test_config: Dict[str, Dict[str, List[Dict[str, str]]]] = {}
  remarks: Dict[str, str] = {}

  for elog_file_path in elog_file_paths:
    config_name = elog_file_path.parent.name
    with elog_file_path.open() as file_handle:
      for line in file_handle:
        key, parsed_diff = (
          edgtest.runtest.parse_test_status_diff(line)
        )

        if key not in pending:
          pending[key] = {}
          lines_by_test_config[key] = {}
          if parsed_diff.remark:
            remarks[key] = parsed_diff.remark

        if config_name not in pending[key]:
          rt_file_path = _rt_path_for(elog_file_path.parent, key)
          pending[key][config_name] = (
            edgtest.runtest.grouping_combined_diff_hash(rt_file_path.path)
          )
          lines_by_test_config[key][config_name] = []

        lines_by_test_config[key][config_name].append({
          "qualifier": parsed_diff.qualifier,
          "variant": parsed_diff.variant,
          "status": parsed_diff.status.full_status
        })

  keyed_statuses: _StatusDict = {}
  for test_id, groups in edgtest.runtest.group_tests_by_diff_hash(pending):
    keyed_statuses[test_id] = {
      'groups': [
        {
          'diff_hash': group.diff_hash,
          'configs': group.configs,
          'lines': lines_by_test_config[test_id][group.configs[0]]
        }
        for group in groups
      ]
    }
    if test_id in remarks:
      keyed_statuses[test_id]['remark'] = remarks[test_id]

  return keyed_statuses


def _matching_change_files(changes_files: Dict[str, ConfinedPath],
                           new_name: str) -> List[ConfinedPath]:
  regress_files = [f.with_name(new_name) for f in changes_files.values()]
  regress_files[:] = [f for f in regress_files if f.exists()]
  return regress_files


def _test_if_any_output_exists(recordings_dir: ConfinedPath,
                               suite_env: Dict[str, str],
                               case_number: int,
                               command_number: int) -> Optional[ConfinedPath]:
  '''Return a recording path if one exists for the case/command, else None.

  Recordings are read and shown to reviewers, so they're confined like any
  other requested file.
  '''
  names_to_test = edgtest.runtest.get_recording_possible_names(
    suite_env,
    case_number,
    command_number
  )
  for possible_file_name in names_to_test:
    test_file_path = recordings_dir / possible_file_name
    if test_file_path.exists():
      return test_file_path

  return None


def _copy_recording(recording: Optional[ConfinedPath],
                    destination: Path) -> Optional[Path]:
  '''Copy ``recording`` (if any) to the private ``destination``, reading it
  through its checks, for tools that take a plain path. The copy streams,
  so a large recording isn't held in memory.
  '''
  if recording is None:
    return None
  with recording.open('rb') as source, open(destination, 'wb') as target:
    shutil.copyfileobj(source, target)
  return destination


def _test_if_any_matches_exists(recordings_dir: ConfinedPath,
                                suite_env: Dict[str, str],
                                case_number: int) -> Optional[ConfinedPath]:
  '''Return a matches recording path if one exists for the case, else None.'''
  names_to_test = edgtest.runtest.get_matches_possible_names(
    suite_env,
    case_number
  )
  for possible_file_name in names_to_test:
    test_file_path = recordings_dir / possible_file_name
    if test_file_path.exists():
      return test_file_path

  return None


def _assoc_files_path(test_home: ConfinedPath,
                      test_path: ConfinedPath) -> Optional[ConfinedPath]:
  assoc_files_path = edgtest.runtest.get_assoc_files_path(test_path.path)
  if assoc_files_path is None:
    return None
  return test_home.subpath(assoc_files_path)


async def run_test_source_handler(channel: Channel,
                                  context: SandboxContext,
                                  test_home: ConfinedPath,
                                  run_dir: ConfinedPath,
                                  changes_files: Dict[str, ConfinedPath]
                                  ) -> None:
  '''Handle one test source request on ``channel``. Every file is reached
  from ``test_home`` or ``run_dir``, so it can't leave them, and commands
  run on them (ifc-dump) in ``context``.
  '''
  request_tag, request_value, _section = decode_response(await channel.recv())
  is_multi_config_run = len(changes_files) > 1

  def decompose_test_id(test_id: str) -> Tuple[ConfinedPath, str]:
    test_id_match = MULTI_CONFIG_TEST_FORMAT.match(test_id)
    assert test_id_match is not None
    return changes_files[test_id_match.group(1)], test_id_match.group(2)

  def test_id_to_path(test_id: str) -> ConfinedPath:
    test_id_parts = test_id.split('/')

    # See if there's a patched version of this test that should be displayed
    # instead.
    patched_test_version = run_dir.joinpath('patched-tests', *test_id_parts)
    if patched_test_version.exists():
      return patched_test_version

    return test_home.joinpath('tests', *test_id_parts)

  if request_value is not None and 'reviewer_id' in request_value:
    reviewer_id = request_value['reviewer_id']

    try:
      if request_tag == RequestTags.TEST_LIST_STATUSES:
        lists_of_status_lists: List[Any] = []
        lists_of_status_lists.append(
          {
            'name': 'Changes List',
            'statuses': _create_status_dict(
              _matching_change_files(changes_files, 'changes.elog'),
              is_multi_config_run = is_multi_config_run
            )
          }
        )

        regress_files = _matching_change_files(
          changes_files,
          'regressions.elog'
        )
        if len(regress_files) != 0:
          lists_of_status_lists.append(
            {
              'name': 'Regressions List',
              'statuses': _create_status_dict(
                regress_files,
                is_multi_config_run = is_multi_config_run
              )
            }
          )

        improve_files = _matching_change_files(
          changes_files,
          'improvements.elog'
        )
        if len(improve_files) != 0:
          lists_of_status_lists.append(
            {
              'name': 'Improvements List',
              'statuses': _create_status_dict(
                improve_files,
                is_multi_config_run = is_multi_config_run
              )
            }
          )

        await channel.send(message(
          RequestTags.TEST_LIST_STATUSES,
          value = {
            'reviewer_id': reviewer_id,
            'client_response': {
              'configs': sorted(changes_files.keys()),
              'statuses': lists_of_status_lists
            }
          }
        ))
      elif (request_tag == RequestTags.TEST_SHOW_DIFF or
            request_tag == RequestTags.TEST_SHOW_CURR_DIFF or
            request_tag == RequestTags.TEST_SHOW_RAW_OUTPUT):
        configured_test_id = request_value['id']
        changes_file_path, test_id = decompose_test_id(configured_test_id)
        rt_file_path = _rt_path_for(changes_file_path.parent, test_id)

        if not rt_file_path.exists():
          await channel.send(message(
            request_tag,
            value = {
              'reviewer_id': reviewer_id,
              'client_response': {
                'id': configured_test_id,
                'content': None
              }
            }
          ))
          return

        async def form_curr_diff_text(config_name: str,
                                      case_rt_path: ConfinedPath) -> str:
          '''Live-diff observed vs recordings per command.

          Per-command body content is diffed (recording command line stripped),
          then equal bodies are collapsed with # command comments — same
          presentation as static.
          '''
          suite_name = test_id.split('/')[0]
          suite_path = test_home / 'tests' / suite_name
          # Load the suite ENV and then populate EDG_CONFIG with the name of
          # the config that execute so as to restore the correct active config,
          # ensuring that the diff can be run against not only the default
          # recordings but the prior recordings of that same active config.
          suite_env = edgtest.load_test_suite_env(suite_path.path)
          suite_env['EDG_CONFIG'] = config_name
          # Use the test path to gather the recording directory to find the
          # status-quo recording files to be examined.
          test_path = test_id_to_path(test_id)
          recordings_dir = test_home.subpath(
            edgtest.runtest.get_assoc_recordings_path(test_path.path)
          )

          with tempfile.TemporaryDirectory() as tmp_dir_name:
            tmp_dir = Path(tmp_dir_name)
            entries: List[Tuple[Dict[str, Any], List[str]]] = []

            with edgtest.runtest.RunTestOutput(case_rt_path.path) as rt_output:
              for case_number, case in enumerate(
                rt_output.metadata.get('cases', []),
                start = 1
              ):
                for command_number, command in enumerate(
                  case.get('commands', []),
                  start = 1
                ):
                  cmd_dir = tmp_dir / f"{case_number}.{command_number}"
                  cmd_dir.mkdir()
                  observed_path = cmd_dir / 'observed'
                  change_diff_path = cmd_dir / 'change.diff'

                  try:
                    rt_output.extract_command_output(
                      case_number,
                      command_number,
                      observed_path
                    )
                  except FileNotFoundError:
                    observed_path.touch()

                  expected_path = _copy_recording(
                    _test_if_any_output_exists(
                      recordings_dir,
                      suite_env,
                      case_number,
                      command_number
                    ),
                    cmd_dir / 'expected'
                  )

                  diff_hash = await edgtest.runtest.write_content_diff(
                    expected_path,
                    observed_path,
                    change_diff_path,
                    skip_expected_command_line = True
                  )
                  if diff_hash is None:
                    continue

                  with open(change_diff_path, 'r',
                            errors = 'replace') as file_handle:
                    diff_lines = file_handle.readlines()

                  command_entry = dict(command)
                  command_entry['diff_hash'] = diff_hash
                  entries.append((command_entry, diff_lines))

                if case.get('matches') is None:
                  continue

                matches_dir = tmp_dir / f"{case_number}.matches"
                matches_dir.mkdir()
                observed_matches = matches_dir / 'observed'
                matches_change_diff = matches_dir / 'change.diff'
                try:
                  rt_output.extract_case_matches(
                    case_number,
                    observed_matches
                  )
                except FileNotFoundError:
                  continue

                expected_matches = _copy_recording(
                  _test_if_any_matches_exists(
                    recordings_dir,
                    suite_env,
                    case_number
                  ),
                  matches_dir / 'expected'
                )
                matches_diff_hash = await edgtest.runtest.write_content_diff(
                  expected_matches,
                  observed_matches,
                  matches_change_diff,
                  skip_expected_command_line = False
                )
                if matches_diff_hash is None:
                  continue

                with open(matches_change_diff, 'r',
                          errors = 'replace') as file_handle:
                  matches_diff_lines = file_handle.readlines()

                matches_entry = {
                  'command': edgtest.runtest.case_matches_annotation(
                    case_number
                  ),
                  'diff_hash': matches_diff_hash
                }
                entries.append((matches_entry, matches_diff_lines))

            return ''.join(
              edgtest.runtest.format_collapsed_case_diffs(entries)
            )

        if request_tag == RequestTags.TEST_SHOW_DIFF:
          with edgtest.runtest.RunTestOutput(rt_file_path.path) as rt_output:
            static_text = ''.join(rt_output.combined_static_diff())
          client_response = {
            'id': configured_test_id,
            **trunc_display_content(static_text)
          }
        elif request_tag == RequestTags.TEST_SHOW_RAW_OUTPUT:
          with edgtest.runtest.RunTestOutput(rt_file_path.path) as rt_output:
            raw_output = ''.join(rt_output.combined_raw_output())
          client_response = {
            'id': configured_test_id,
            **trunc_display_content(raw_output)
          }
        elif request_tag == RequestTags.TEST_SHOW_CURR_DIFF:
          rep_config = changes_file_path.parent.name
          group_configs = request_value.get('configs') or [rep_config]
          rep_curr_text = await form_curr_diff_text(rep_config, rt_file_path)

          divergent_configs: List[str] = []
          for config_name in group_configs:
            if config_name == rep_config:
              continue
            sibling_changes = changes_files[config_name]
            sibling_rt = _rt_path_for(sibling_changes.parent, test_id)
            if not sibling_rt.exists():
              divergent_configs.append(config_name)
              continue
            sibling_text = await form_curr_diff_text(config_name, sibling_rt)
            if sibling_text != rep_curr_text:
              divergent_configs.append(config_name)

          client_response = {
            'id': configured_test_id,
            **trunc_display_content(rep_curr_text)
          }

          if len(divergent_configs) != 0:
            client_response['divergent_configs'] = sorted(divergent_configs)
        else:
          # Unhandled request_tag; the above are explicitly branched as this is
          # a large function and the verbosity increases clarity as to which
          # request is being handled.
          raise AssertionError(f'unhandled request_tag: {request_tag}')

        await channel.send(message(
          request_tag,
          value = {
            'reviewer_id': reviewer_id,
            'client_response': client_response
          }
        ))
      elif request_tag == RequestTags.TEST_LIST_ASSOC_FILES:
        test_id = request_value['id']
        test_path = test_id_to_path(test_id)
        assoc_files_path = _assoc_files_path(test_home, test_path)

        file_list = []

        if assoc_files_path is not None and assoc_files_path.exists():
          for root, dirs, filenames in assoc_files_path.walk():
            dirs[:] = [
              d for d in dirs
              if edgtest.runtest.is_valid_assoc_path(str(root.path), d)
            ]
            for filename in filenames:
              if not edgtest.runtest.is_valid_assoc_path(
                str(root.path), filename
              ):
                continue

              file_path = root / filename

              # The relative path with / separators (to canonicalize the path
              # regardless of operating system).
              relative_path = file_path.relative_to(assoc_files_path)
              file_list.append(relative_path.as_posix())

          file_list.sort()

        await channel.send(message(
          RequestTags.TEST_LIST_ASSOC_FILES,
          value = {
            'reviewer_id': reviewer_id,
            'client_response': {
              'id': test_id,
              'files': file_list
            }
          }
        ))
      elif request_tag == RequestTags.TEST_SHOW_SOURCE:
        test_id = request_value['id']
        test_path = test_id_to_path(test_id)

        path_to_show: Optional[ConfinedPath] = None
        if ('assoc_file' in request_value and
            request_value['assoc_file'] is not None):
          assoc_file_path_parts = request_value['assoc_file'].split('/')
          assoc_files_path = _assoc_files_path(test_home, test_path)
          if assoc_files_path is not None:
            path_to_show = assoc_files_path.joinpath(*assoc_file_path_parts)
        else:
          path_to_show = test_path

        if path_to_show is None or not path_to_show.exists():
          await channel.send(message(
            RequestTags.TEST_SHOW_SOURCE,
            value = {
              'reviewer_id': reviewer_id,
              'client_response': {
                'id': test_id,
                'assoc_file': request_value.get('assoc_file'),
                'content': None
              }
            }
          ))
        elif path_to_show.suffix == '.ifc':
          result = await context.run_command('ifc-dump', str(path_to_show))
          if result.timed_out:
            ifc_dump_output = (
              f"ifc-dump timed out after {context.command_timeout:g} "
              f"seconds\n"
            )
          else:
            ifc_dump_output = result.output.decode(errors = 'replace')

          await channel.send(message(
            RequestTags.TEST_SHOW_SOURCE,
            value = {
              'reviewer_id': reviewer_id,
              'client_response': {
                'id': test_id,
                'assoc_file': request_value.get('assoc_file'),
                **trunc_display_content(ifc_dump_output)
              }
            }
          ))
        else:
          # Only as much as can be shown is read (one character more marks
          # the content as truncated).
          with path_to_show.open(errors = 'replace') as file_handle:
            shown = file_handle.read(MAX_DISPLAY_CONTENT_SIZE + 1)
          await channel.send(message(
            RequestTags.TEST_SHOW_SOURCE,
            value = {
              'reviewer_id': reviewer_id,
              'client_response': {
                'id': test_id,
                'assoc_file': request_value.get('assoc_file'),
                **trunc_display_content(shown)
              }
            }
          ))
      else:
        await channel.send(message(
          RequestTags.INVALID_REQUEST,
          value = {
            'reviewer_id': reviewer_id
          }
        ))
    except Exception as ex:
      if isinstance(ex, IllegalFileAccessError):
        ex.reviewer_id = reviewer_id
      if sys.version_info >= (3, 11):
        ex.add_note(f"request triggered by reviewer: \"{reviewer_id}\"")
      raise ex
  else:
    await channel.send(message(RequestTags.INVALID_REQUEST))


class TestSourceHandler:
  def __init__(self,
               section_name: str,
               slug: str,
               context: SandboxContext,
               test_home: ConfinedPath,
               root: ConfinedPath) -> None:
    '''Requests reach files only through ``test_home`` and the run
    directory ``root``, and run commands in ``context``.
    '''
    self._section_name = section_name
    self._slug = slug
    self._context = context
    self._test_home = test_home
    self._root = root

  def _changes_files(self) -> Dict[str, ConfinedPath]:
    '''Each configuration's changes file in the run, found now.'''
    return {
      config: self._root.subpath(path)
      for config, path in edgtest.find_changes_files(self._root.path).items()
    }

  @property
  def subscription_type(self) -> str:
    return 'test.src'

  @property
  def section_name(self) -> str:
    return self._section_name

  @property
  def slug(self) -> str:
    return self._slug


  async def handle_request(self, channel: Channel) -> None:
    await run_test_source_handler(
      channel,
      self._context,
      self._test_home,
      self._root,
      await edgutil.async_to_thread(self._changes_files)
    )

async def handle_test_edit_request(
    request: WebSocketsMessageType) -> ProtocolMessage:
  '''Handle one test edit request; return a protocol reply message.'''
  request_tag, request_value, _section = decode_response(request)

  if request_tag == RequestTags.TEST_UPDATE_DIFF:
    test_id = request_value['id']
    configs = request_value['configs']
    assert isinstance(configs, list) and len(configs) > 0
    config_arg = ','.join(configs)
    reviewer_id = request_value['reviewer_id']

    try:
      with tempfile.TemporaryDirectory() as tmp_dir:
        cmd_args = [
          'edgy',
          f"--runs-dir={tmp_dir}",
          f"--config={config_arg}",
          '-A',
          '-W',
          test_id
        ]

        # Note the execution on the command line.
        cmd_arg_str = ' '.join(cmd_args)
        print(f"Issuing command: {cmd_arg_str}")

        # Execute the edgy command.
        proc = await asyncio.create_subprocess_exec(
          *cmd_args,
          stdout = asyncio.subprocess.PIPE,
          stderr = asyncio.subprocess.STDOUT
        )
        stdout, _ = await proc.communicate()
        output_text = stdout.decode()
        cmd_output = ''
        if len(output_text) != 0:
          # If output exists, add preceding indent to it, then drop the garbage
          # last line to prevent starting the next line with unwanted
          # whitespace.
          output_lines = [f"  {x}" for x in output_text.split('\n')]
          dropped_line = output_lines.pop()

          # Make sure this doesn't accidentally drop something important.
          assert dropped_line == '  '

          # Recombine the lines and add a newline for printing.
          cmd_output = '\n'.join(output_lines)
          cmd_output += '\n'

        print(
          f"Command complete: {cmd_arg_str}\n{cmd_output}",
          end = ''
        )

        return message(
          RequestTags.TEST_UPDATE_DIFF,
          value = {
            'id': test_id,
            'configs': configs,
            'reviewer_id': reviewer_id,
            'success': proc.returncode == 0
          }
        )
    except Exception as ex:
      if sys.version_info >= (3, 10):
        traceback.print_exception(ex)
      else:
        eprint(str(ex))
      # In case something goes wrong, guarantee a response.
      return message(
        RequestTags.TEST_UPDATE_DIFF,
        value = {
          'id': test_id,
          'configs': configs,
          'reviewer_id': reviewer_id,
          'success': False
        }
      )

  return message(RequestTags.INVALID_REQUEST)


class TestEditHandler:
  def __init__(self,
               section_name: str,
               slug: str,
               worker_pool: BackgroundWorkerPool) -> None:
    self._section_name = section_name
    self._slug = slug
    self._worker_pool = worker_pool

  @property
  def subscription_type(self) -> str:
    return 'test.edit'

  @property
  def section_name(self) -> str:
    return self._section_name

  @property
  def slug(self) -> str:
    return self._slug


  async def handle_request(self, channel: Channel) -> None:
    request = await channel.recv()

    async def edit_job() -> None:
      reply = await handle_test_edit_request(request)
      await channel.send(reply)

    await self._worker_pool.submit(edit_job)
