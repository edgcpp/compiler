=====================
EDG Testing Framework
=====================

The EDG front end testing framework is fundamentally based on process exit
codes and recorded "expected output."

Writing a Test File
===================

Test files are written using a series of directives at the start of the test
file.

The following is a sample single-file test (the common case) to help illustrate
some of the features described below:

.. code:: c++

  //remark:Simple friend functions in class template
  //options::;cn
  template <class T> class A {
    friend int f(A);
    int     i;
  };

  A<int>  a;

  int f(A<int> a) {
    return a.i;     // Should be OK
  }

  #if TEST_NUMBER==2
  int f(A<int> a, int i) {
    return a.i;     // No access -- should fail
  }
  #endif

Single-file Testing
-------------------

Individual test files end in either ``.sft.c`` or ``.sft.cpp`` depending on
the programming language being tested.  The testing directives that can be
used in a single-file test are enumerated below.  Additional directives
have meaning in multi-file tests and are described in :ref:`mtu-testing`.

  |  ``//remark:``\ *A string that gets printed on the output*
  |  ``//type:``\ *xx*
  |  ``//name:``\ *xxx*
  |  ``//options:-a:-b=xx:-c=``
  |  ``//options_all:-A``
  |  ``//options_sep:``\ *x*
  |  ``//match_regex:``\ *regular expression*
  |  ``//cases:``\ *n*
  |  ``//eccp_command:``\ *compile-command*
  |  ``//ulimit:``\ *options*
  |  ``//linker_options:``\ *options*
  |  ``//execution_args:``\ *command line arguments*
  |  ``//use_system_includes:``\ *true/false*
  |  ``//edg_header_pack:``\ *string*
  |  ``//require:``\ *string*
  |  ``//filter:``\ *pipe-command*

*xx* is a type identifier.  The accepted values are:

+------------+--------------------------------------------------------------+
| Identifier | Meaning                                                      |
+============+==============================================================+
| ``fn``     | front end only negative                                      |
+------------+--------------------------------------------------------------+
| ``fp``     | front end only positive                                      |
+------------+--------------------------------------------------------------+
| ``fc``     | front end - expect catastrophic error                        |
+------------+--------------------------------------------------------------+
| ``cn``     | compile negative                                             |
+------------+--------------------------------------------------------------+
| ``cp``     | compile positive (including compiling generated C) - DEFAULT |
+------------+--------------------------------------------------------------+
| ``cc``     | compile - expect catastrophic error                          |
+------------+--------------------------------------------------------------+
| ``ln``     | compile and link negative                                    |
+------------+--------------------------------------------------------------+
| ``lp``     | compile and link positive                                    |
+------------+--------------------------------------------------------------+
| ``rn``     | runtime negative                                             |
+------------+--------------------------------------------------------------+
| ``rp``     | runtime positive                                             |
+------------+--------------------------------------------------------------+
| ``ra``     | runtime - expect abort                                       |
+------------+--------------------------------------------------------------+
| ``cppbe``  | a special C++-generating back end only mode that dumps the   |
|            | generated code for review and validation                     |
+------------+--------------------------------------------------------------+
| ``s``      | skip the test                                                |
+------------+--------------------------------------------------------------+

Positive tests expect a return code of zero.  Negative tests expect a
non-zero return code.

The ``//name:`` directive specifies the name to be used when the file is
compiled.  This is useful for tests that need to test platform support for
Unicode file names and similar file name-dependent scenarios.

The ``//options:`` directive specifies a set of command line options to be
used, separated (by default) by colons.  The compiler will be invoked once
for each set of options. An option set can override the type specified by
appending the optional string ``;``\ *xx* on the end of an option
specification.

If the options need to include a colon, the ``//options_sep:`` directive
can be used to specify a different separator.  For example,
``//options_sep:!`` will change the separator to ``!``.  The separator can
be multiple characters.

The ``//options_all:`` directive specifies a set of command line options
that will be provided on every compilation.  This is useful for tests with
a single case and for options that don't vary from case to case.  This may
be used in conjunction with the ``//options:`` directive.  The options from
each option set are put in the command line after the ``//options_all:``
string so that they can override values in the ``//options_all:`` string.

The ``//match_regex:`` directive allows specification of a regular
expression (expressed in the Python regex dialect) that must be found in
the output for the test to pass.  Matches will additionally add sections to
the captured output highlighting the matching lines (thus serving as a
further sanity check against unintended changes).  This directive is
particularly useful if wanting to require fine grained details like
specific diagnostics, IL output fragments, or generated C++ code (when
using a ``cppbe`` test).

The ``//linker_options:`` directive specifies an additional set of options
that are placed at the end of the command used to compile and link the
program and is intended to be used for library names.

The string specified in the ``//remark:`` directive is printed in the test
results output to help identify what a test is doing.

As shown in the initial testing example, it is often useful to be able to have
code conditionally compiled for each of the different compilations specified by
the options string.  The test driver defines a preprocessor variable named
``TEST_NUMBER``.  This variable has the value "1" for the first set of options,
"2" for the second, and so on.

If you have a number of test cases that don't require different command
line options and that have the same result type you can use the
``//cases:``\ *n* directive to specify that the compiler should by invoked
*n* times, with ``TEST_NUMBER`` set to the current invocation number.

``//options:`` and ``//cases:`` can be used together.  For example, you may
have five test cases but only need to specify options for the first two.

.. code:: c++

  //options:-A;cn:-O;cn
  //cases:5

The ``//ulimit:`` directive specifies command-line options passed to a
``ulimit`` command prior to compiling, linking, and running the test.  This
can be used to prevent a test from exhausting system resources or running
in a non-terminating loop.

Special characters can be incorporated into the test case by using special
escape sequences.  The escapes are of the form ``???``\ *nnn* where *n* is
a decimal digit.

If ``//require:`` is specified, the string that follows is searched for in the
output of ``--dump_configuration``; if not found, the test is skipped.  If
multiple ``//require:`` cases are specified, all strings must be found or the
test will be skipped.

If ``//use_system_includes:`` is set to ``true``, the system headers
associated with the current EDG base will be added to the default include
paths.  Additionally, EDG compiler emulation modes will be enabled as
appropriate.  For instance, if the test is run using a GCC 15.1 base this will
add the include paths for system headers associated with GCC 15.1 and
``--gnu_version 150100`` to the command line.  Otherwise, if this directive is
set to ``false``, EDG headers in ``include_c99`` (for C programs) and
``include_c++`` (for C++ programs) will be added to the default include paths.

The ``//edg_header_pack:`` directive specifies an "EDG header pack" to include
as system headers for testing.  Header packs are include directories that are
themselves subdirectories of ``$TEST_ROOT/.includes/``.  For instance,
``//edg_header_pack: foo`` will result in a hidden
``--sys_include $TEST_ROOT/.includes/foo`` command line argument.

If ``//filter:`` is specified, the specified command will be used to filter
the ``run_test`` output before ``diff``-ing it.  By default, a test driver
uses in-process test normalization logic as a filter.

.. _mtu-testing:

Multi-file Testing
------------------

Multi-file tests work similarly to single-file tests; however, they end
with either ``.mft.c`` or ``.mft.cpp``.  Additionally, for correct
operation, these tests live in their own test directory, typically
following the convention of ``test-name/test.mft.cpp``. This allows the
test to live alongside its supporting files.

For instance, testing a simple C++ ``#include`` might using the following
structure:

.. code:: text

  simple-include/
    test.h
    test.mft.cpp

Similarly, for testing an import of a module, the test might use the
following structure:

.. code:: text

  modules-function/
    exporting-module.ixx
    test.mft.cpp

where ``test.mft.cpp`` contains the testing directives (described below) that
compile ``exporting-module.ixx``.

As previously noted, there are additional directives which are useful for
multi-file testing:

  | ``//header_unit_files:t.h t2.h``
  | ``//module_files:i-part.ixx internal:i-internal.cpp i.ixx obj:msvc-module.ixx``
  | ``//script:``\ *script_file*

The ``//header_unit_files:`` directive specifies any header files that should
be compiled via ``--create_header_unit`` and passed to the test invocation via
a ``--header_unit`` mapping.

The ``//module_files:`` directive specifies any module files to be compiled for
the test.

In most cases these files should be listed without a prefix; such files are
presumed to be module interface source files and will be compiled with the
``--module_interface`` flag to generate both an IFC and a ``.obj`` file for
the module source file.

If given the ``obj:`` prefix it's assumed the IFC file is already present and
only the ``.obj`` file should be compiled.

If given the ``internal:`` prefix this is a module implementation unit that an
IFC file should be force-generated for (this is unusual).  These module
source files are compiled under the ``--module_internal_partition`` flag to
generate both an IFC and a ``.obj`` file.

Note that when testing modules in a compiler emulation mode (e.g., with
``--gnu_version=150100`` added to ``//options_all:``), the ``--modules``
option must also be specified:  emulation modes disable the module keywords
by default to match the emulated compiler (no version of g++ enables them
without ``-fmodules-ts``).  For example:

.. code:: text

  //options_all:--c++20 --gnu_version=150100 --modules

See the ``modules/eifc/modules/import-function-gnu`` and
``modules/eifc/modules/export-float16-gnu`` tests for examples.

If ``//script:`` is specified, that script is executed instead of any other
commands.  In general, the test type does not matter for script files except
for checking the return code.  For example, a ``cp`` test will expect a zero
return code, while a ``cn`` will expect a non-zero.  The script will be passed
command-line arguments in the same way that the compile command would.

Advanced Concepts
-----------------

The directives are processed in the following sequence:

- those specified in ``$TEST_ROOT/.run_test_flags``
- (for multi-file tests) the ``flags.txt`` file in the directory of the test
- those specified at the start of the test file

In some cases later directives overwrite earlier ones (e.g., ``//type:``).
In other cases, a list of the specified values is accumulated (in the order in
which the various lists are specified above).  The directives for which the
values are accumulated are:

- ``options``
- ``options_all``
- ``match_regex``
- ``linker_options``
- ``execution_args``
- ``require``
- ``header_unit_files``
- ``module_files``

Compile tests are processed through ``eccp -c``.  Runtime tests are
compiled and linked using ``eccp``.  Compile and link tests are also
compiled and linked using ``eccp``, however a ``ln`` test is expected to
return a non-zero status from ``eccp`` where a ``rn`` test is expected to
return a zero status from ``eccp`` and a nonzero status when the program is
executed.  If the ``//eccp_command:`` directive is used, the command
specified is used instead of the ``eccp`` command.


Testing Tools
=============

There are two primary tools for testing, ``edgy`` and ``edg-run-test``.
Both of these tools rely on the same internal ``edgtest`` Python library,
they just provide a different interface to it.

In most cases ``edgy`` is the preferred testing tool as it's designed for
large-scale testing.  However, when authoring a new test ``edg-run-test``
can be a great way to get immediate feedback.

``edgy``
--------

Usage information:

.. code:: text

  usage: edgy [-h] [-c RUN_CONFIGS] [--runs-dir TEST_RUNS_DIRECTORY]
              [--run-timestamp TEST_RUN_TIMESTAMP] [-j JOB_COUNT] [-T TIMEOUT_SECONDS]
              [-A] [-D] [-E] [-M] [-R] [-n MAX_FAIL_COUNT] [--pick PICK_COUNT]
              [--disp-run-dir] [-W] [--crash-avoidance] [--no-flakey-tests]
              [--replay {all-failing,output-mismatch,regress,unexpected}]
              [--slow-tests NUM_SLOW_TESTS_REPORTED] [--fully-random-order]
              [--cmd] [--verbose] [filters]

  EDG test runner.

  positional arguments:
    filters               the test filter to apply; "all" for all suites

  options:
    -h, --help            show this help message and exit
    -c, --config RUN_CONFIGS
                          the run configurations to apply
    --runs-dir TEST_RUNS_DIRECTORY
                          override the directory where tests runs are stored
    --run-timestamp TEST_RUN_TIMESTAMP
                          override the timestamp when creating a new test run (does
                          not apply to replay runs)
    -j, --jobs JOB_COUNT  the number of concurrent jobs
    -T, --timeout TIMEOUT_SECONDS
                          the number of seconds to wait for tests before killing them
                          (0 to disable)
    -A                    include slow tests (marked by an "_" prefix)
    -D, --debug-failures  print debug commands for failures
    -E, --no-expectations
                          disable expectations
    -M, --meticulous      disable faster mode (on by default) and run all expensive
                          checking
    -R, --regress         silence non-regressive changes
    -n, --max-failure-count MAX_FAIL_COUNT
                          the maximum number of failures
    --pick PICK_COUNT     the maximum number of tests to run (this option implies
                          --fully-random-order)
    --disp-run-dir        print the directory where the run output is being stored
    -W, --record          record tests
    --crash-avoidance     avoid tests that are likely to crash the compiler (this option
                          also implies --no-flakey-tests)
    --no-flakey-tests     do not run tests that are known to be flakey
    --replay {all-failing,output-mismatch,regress,unexpected}
                          rerun tests from the last run
    --slow-tests NUM_SLOW_TESTS_REPORTED
                          the number of the slowest tests to report
    --fully-random-order  completely randomize test execution order (this can help lower
                          system pressure by mixing more "intensive" test suites with
                          less intensive test suites)
    --cmd                 print all run test commands
    --verbose             enable verbose mode

Setup Requirements
^^^^^^^^^^^^^^^^^^

``edgy`` was designed to be usable from any directory.  To facilitate this,
the ``EDG_TEST_HOME`` environment variable must be set to the directory
where the tests subdirectory and ``.edgy/config.json`` are.

In the typical configuration, ``CPFE`` should be set to the C-generating
back end binary being tested and ``CPFE_CP`` should be set to the
C++-generating back end binary being tested.

Filtering
^^^^^^^^^

```edgy`` works off of one or more (comma-separated) filters.  These
filters are typically test suite names, e.g.,

.. code:: bash

  edgy regressions,stdlib

Filter Reduction
~~~~~~~~~~~~~~~~

These can be reduced by adding addition path components.  For instance, if
you want to run the tests in the ``modules`` directory of the
``regressions`` test suite, you can do so via

.. code:: bash

  edgy regressions/modules

You can also elaborate on this to dive down to a single test, e.g.,
``regressions/foo.sft.cpp``:

.. code:: bash

  edgy regressions/modules/foo.sft.cpp

Special Filters
~~~~~~~~~~~~~~~

There are also a couple of special filters, the primary one is ``all``
(which simply means all the test suites ``edgy`` knows about), e.g.,

.. code:: bash

  edgy all

An additional very useful one is the ``mine`` filter.  This filter is
configured by setting the ``EDGY_MY_TESTS`` environment variable to a
comma-separated list of test suites.  For instance, if concerned primarily
with ``regressions`` and the ``stdlib`` impact in your work, this might be
a reasonable use:

.. code:: bash

  export EDGY_MY_TESTS=regressions,stdlib
  edgy mine

Note that special filters cannot be reduced (e.g., ``edgy mine/xyz``
does not work).

Negative Filters
~~~~~~~~~~~~~~~~

Negative filters can be applied as well. These start with a ``!`` and are
applied as a reduction of positive filters, e.g.,

.. code:: bash

  edgy regressions,!regressions/modules

Here we're running all tests in the ``regressions`` suite except for the
tests in the ``modules`` subdirectory.

File-Loaded Filters
~~~~~~~~~~~~~~~~~~~

Filters can also be loaded from a file:

.. code:: bash

  $> cat my_filters.txt
  regressions
  !regressions/modules
  $> edgy @my_filters.txt

The above is logically equivalent to the above negative filter example (the
filters are just spelled out in a file on a line-separated basis).

This feature is particularly useful for working with fine-grained lists of
tests (such as ``edgy``'s ``.elog`` files).

Live Feedback
^^^^^^^^^^^^^

Test Run Status
~~~~~~~~~~~~~~~

Progress reporting is provided by a (fairly rapidly) updating status line:

.. code:: text

  588/83,651 (0%) in 00:00:04 ~127/sec (00:10:54 remaining)

This status line shows:

.. code:: text

  <number of completed tests>/<number of tests to run total>
  (<percentage of tests completed>%) in <elapsed time>
  ~<tests per second>/sec
  (<estimated time remaining at current test per second> remaining)

Problem Reporting
~~~~~~~~~~~~~~~~~

Any encountered problems are reported above the status line as they're
discovered.

A completed run might look something like::

  - regressions/foo.sft.cpp cp::ABORT:
  + regressions/foo.sft.cpp cp::PASS--OUTPUT MISMATCH:

"+" is used to indicate an unexpected problem, while "-" is used to indicate an
unsatisfied expectation (the above is an example of what might happen if there
was a regression resulting in a crash that has been resolved).

When multiple configurations are in use, the configuration is specified in
parens prior to the test "name", e.g.::

  + (edg_x86_64) regressions/foo.sft.cpp fp::PASS--OUTPUT MISMATCH:
  + (edg_x86_64_cp) regressions/foo.sft.cpp fp::PASS--OUTPUT MISMATCH:

``edgy`` also will print "commentary" in this output without using a marking
character.  These are used to report extra information that might be
good to know (e.g., that a particular test ran very slowly), e.g.,

.. code:: text

  + regressions/foo.sft.cpp fp::PASS--OUTPUT MISMATCH:
    Consider marking regressions/foo.sft.cpp slow (Took: 00:01:04)

Test Termination
~~~~~~~~~~~~~~~~

``edgy`` supports early termination when it receives ``SIGINT`` (Ctrl + C
in most shells) or ``SIGTERM``.  Early termination will make edgy quickly
terminate all spawned process groups (and thus their processes).  Any tests
terminated in this fashion will be reported to the user with an "X" prefix,
e.g.,

.. code:: text

  X regressions/foo.sft.cpp
  X stdlib/bar/test.mft.cpp
  X regressions/baz/test.mft.cpp

Note: This is particularly helpful both for killing (and finding) tests that
are stuck in an infinite loop if your tests aren't terminating.

Failure Debug Mode
^^^^^^^^^^^^^^^^^^

``edgy`` has a failure debug mode (enabled via ``-D`` or
``--debug-failures``) that enables additional "commentary" for failed
tests, aimed at debugging issues with said tests.

For exposition purposes the below section uses ``<TEST_ROOT>`` for the path
represented by ``EDG_TEST_HOME`` and ``<BUILD_BIN>`` for the location of
the directory containing the front end ``cpfe`` binary.  Additionally ``\``
lines have been added to wrap lines.

As an example, commentary from this option typically looks something like:

.. code:: text

  RUN_TEST_DEFAULT_CONFIG="edg_x86_64" LC_ALL="C" CDISP="<BUILD_BIN>" \
    edg-run-test -o \
    <TEST_ROOT>/runs/2026.06.25-17.14.55/edg_x86_64/regressions
    --driver-debug --faster -t \
    <TEST_ROOT>/tests/regressions \
    <TEST_ROOT>/tests/regressions/foo.sft.cpp

This can be copied verbatim and when executed will print something like
the following:

.. code:: text

  Source file clone:
  cd $(clone-test-to-tmp <TEST_ROOT>/tests/regressions/foo.sft.cpp)
  <BUILD_BIN>/cpfe -DTEST_NUMBER=1 --c++20 Test_name.c

This combined information can then be used to set up a test directory, and a
debug command, following the general setup:

.. code:: text

  <make test directory>
  <set environment variables> <debugger command> <front end invocation>

Using the above sample output, that would look something like:

.. code:: text

  cd $(clone-test-to-tmp <TEST_ROOT>/tests/regressions/foo.sft.cpp)
  RUN_TEST_DEFAULT_CONFIG="edg_x86_64" LC_ALL="C" CDISP="<BUILD_BIN>" \
    gdb --args <BUILD_BIN>/cpfe -DTEST_NUMBER=1 --c++20 Test_name.c

This will quickly give you a debugger invocation (modulo any bugs/missing
features for test directory setup driver debug) that should exactly reproduce
the circumstances that resulted in ``edgy`` seeing what it saw.

Note that many tests may not have a ``Source file clone`` line reported by
the test executor. In these cases it's perfectly safe to omit this (i.e.,
``<make test directory>`` may not always be a part of the pattern).

.. _regress-mode:

Regress Mode
^^^^^^^^^^^^

``edgy`` has a test regression reporting mode (enabled via ``-R`` or
``--regress``).  This is aimed at reporting only those tests whose status
changed to something worse and is particularly useful for noisy tests where
there may be lots of valid ``--OUTPUT-MISMATCH``\ s.  The severity ranking
used to determine what's "worse" is represented in the following table:

+-------------------+------------------+
| Test Status       | Severity Ranking |
+===================+==================+
| ``PASS``          | 0                |
+-------------------+------------------+
| ``FAIL``          | 1                |
+-------------------+------------------+
| ``BADC``          | 1                |
+-------------------+------------------+
| ``CATASTROPHE``   | 2                |
+-------------------+------------------+
| ``ABORT``         | 3                |
+-------------------+------------------+
| ``RUNTIME ABORT`` | 3                |
+-------------------+------------------+

As an example, a ``FAIL`` to an ``ABORT`` will be reported, but an
``ABORT`` to a ``FAIL`` will not be reported.  Similarly, ``FAIL`` to
``BADC`` (or vice versa) will not be reported.

Failure Limiting
^^^^^^^^^^^^^^^^

``edgy`` has a failure limit mode (enabled via ``-n`` *MAX_FAIL_COUNT* or
``--max-failure-count`` *MAX_FAIL_COUNT*, where *MAX_FAIL_COUNT* is an
integer) that stops test execution after *MAX_FAIL_COUNT* tests have
been reported as problematic.  This is compatible with regression mode, and
will only count reported regressions.

Re-record Mode
^^^^^^^^^^^^^^

``edgy`` has a mode for updating test recordings and patching expectations
in the process (enabled via ``-W`` or ``--record``).

As this mode is intended for usage only to update output, ``edgy`` does not
check expectations or otherwise report test status during re-recording.
The process for using re-record mode is something akin to the following:

1. Run the tests
2. Review the changes
3. Determine if any tests' expectations or output need to be updated
4. Rerecord the tests that need to be updated (e.g,
   ``edgy -W regressions/foo.sft.cpp``)

When re-recording multiple configurations, the default configuration's
expectations are updated first.  The expectations of the other configurations
are then recorded as their differences from the default configuration's (see
`Expectation Inheritance`_).

No Expectations Mode
^^^^^^^^^^^^^^^^^^^^

``edgy`` has a no expectations mode (enabled via ``-E`` or
``--no-expectations``) which as the name implies disables expectations.
This is useful for seeing true test statuses (i.e., seeing all failures,
including failures that were previously marked as expected).

No Flakey Tests Mode
^^^^^^^^^^^^^^^^^^^^

``edgy`` has a "no flakey tests" mode (enabled via ``--no-flakey-tests``)
which, as the name implies, skips known flakey tests.  This is useful for
avoiding problematic tests that are known to regularly misdiagnose problems
in your configuration.

For this to work, the configuration in use must have the flakey tests
listed in the ``$EDG_TEST_HOME/flakey-tests`` directory.  For example, to
mark ``regressions/baz/foo.sft.cpp`` flakey in the ``foo-bar``
configuration, one would add a file
``$EDG_TEST_HOME/flakey-tests/foo-bar/regressions.txt`` with the contents:

.. code:: text

  baz/foo.sft.cpp

Note: It's recommended (but not required) to organize this file alphabetically
for easier maintenance.

Replay mode
^^^^^^^^^^^

``edgy`` has a mode for replaying a portion of the previous test run
(enabled via ``--replay``).  It's important to note that the "runs" that
are replayed are the original "run" not a "replay", i.e., replay 2 is not
run against replay 1 but is run against the last run that wasn't a replay
run.

Replay mode is compatible with the normal ``edgy`` options (e.g., filters).
Replay mode itself supports the following sub-options:

+---------------------+-------------------------------------------------------+
| Option              | Description                                           |
+=====================+=======================================================+
| ``all-failing``     | All tests without ``PASS`` status/all tests that      |
|                     | appear in the previous run's log files.               |
+---------------------+-------------------------------------------------------+
| ``output-mismatch`` | All tests with ``OUTPUT MISMATCH`` status (including  |
|                     | tests that are otherwise passing).                    |
+---------------------+-------------------------------------------------------+
| ``regress``         | All tests that had a status regression vs the current |
|                     | configuration's expectations (see the                 |
|                     | :ref:`regress-mode` section for more information      |
|                     | about status regressions).                            |
+---------------------+-------------------------------------------------------+
| ``unexpected``      | All tests that are reporting a status differing from  |
|                     | the current configuration's expectations.             |
+---------------------+-------------------------------------------------------+

Replay results are stored in the last test execution's run output directory
(typically ``tests/runs/``\ *some-timestamp*) in their own ``replays``
subdirectory.  Every time a replay is performed a new subdirectory will be
created within the respective replays subdirectory for the replay output.
This gives replay runs a few special characteristics:

- Test selections are stable across all replay runs (in other words, if you
  have run "T" you're replaying, replay "{N, N + 1, ...}" will not affect each
  other's output/test selections).
- Replays can be run many times without resulting in ``edgy``'s test run
  cleanup deleting anything.

As a visual aid, this is the expected directory structure for a run with two
replays in the ``runs`` directory:

.. code:: text

  .
  ├── 2026.06.07-16.19.15
  │   ├── replays
  │   │   ├── 1
  │   │   │   └── edg_x86_64
  │   │   │       ├── regressions
  │   │   │       │   ├── test-a.rt
  │   │   │       │   ├── test-b.rt
  │   │   │       │   └── test-c.rt
  │   │   │       └── regressions.log
  │   │   └── 2
  │   │       └── edg_x86_64
  │   │           ├── regressions
  │   │           │   ├── test-a.rt
  │   │           │   ├── test-b.rt
  │   │           │   └── test-c.rt
  │   │           └── regressions.log
  │   └── edg_x86_64
  │       ├── regressions
  │       │   ├── test-a.rt
  │       │   ├── test-b.rt
  │       │   └── test-c.rt
  │       └── regressions.log
  ...

Replaying Failed Test
~~~~~~~~~~~~~~~~~~~~~

As an example, consider you've run a test suite:

.. code:: text

  edgy regressions

This then revealed 5 of the 500 tests are failing.  You then make a change, and
then want to run those 5 tests again to see if you've fixed the problem:

.. code:: text

  edgy --replay all-failing

This runs only the 5 failing tests.

Replaying Subset of Failed Test
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As an example, consider you've run a test suite:

.. code:: text

  edgy regressions

This then revealed 50 of the 500 tests are failing, but you're only
interested in the 10 tests in ``regressions/foo``.  You then make a change,
and then want to run those 10 tests again to see if you've fixed the
problem:

.. code:: text

  edgy --replay all-failing regressions/foo

This runs only the 10 failing tests.

Replaying Mismatched
~~~~~~~~~~~~~~~~~~~~

As an example, consider you've run a test suite:

.. code:: text

  edgy regressions

This then revealed 5 of the 500 tests have an output mismatch due to changes
you made.  You review the changes, and decide you want to rerecord these
tests:

.. code:: text

  edgy --replay output-mismatch -W

This runs only the 5 tests with the output mismatch, rerecording them and
updating expectations in the process.

Replaying Regressions
~~~~~~~~~~~~~~~~~~~~~

As an example, consider you're working with volatile code that's failing many
tests, and you're using regression mode to focus down the reported tests:

.. code:: text

  edgy -R regressions

This then revealed 5 of the 500 tests are regressing while 50 are unexpected
status changes, and 200 are failing.  The other replay modes will not help you
realistically narrow down the set of test output being reviewed.  This is where
regress replays shine:

.. code:: text

  edgy --replay regress

This runs only the 5 tests that actually have regressive status changes,
allowing you to evaluate much more effectively whether your iterative
changes are improvements.

Replaying Unexpected
~~~~~~~~~~~~~~~~~~~~

As an example, consider you've run a test suite:

.. code:: text

  edgy regressions

This then revealed 5 of the 500 tests differ from their expected status or
otherwise have an output mismatch.  You review the changes, and decide you want
to rerecord these tests:

.. code:: text

  edgy --replay unexpected -W

This runs only the 5 tests, rerecording them and updating expectations in the
process.

Slow Test Reporting
^^^^^^^^^^^^^^^^^^^

``edgy`` has a slow test reporting mode (enabled via ``--slow-tests``
*NUM_SLOW_TESTS_REPORTED*, where *NUM_SLOW_TESTS_REPORTED* is an integer)
that reports the *NUM_SLOW_TESTS_REPORTED* slowest test executions.  This
is compatible with all modes (with the exception of re-record mode which does
not record the necessary timings).

Verbose Mode
^^^^^^^^^^^^

``edgy`` has a verbose mode (enabled via ``--verbose``) that enables
printing of additional details.  Verbose mode provides additional
information about environment variables, what cleanup actions are being
taken, and remarks for failure debugging are turned on for every tests
(regardless of its pass/fail status).  This can be useful for initial
setup, or when attempting to get a debug invocation for one or more test(s)
that aren't currently failing.

Copying Tested Binaries
^^^^^^^^^^^^^^^^^^^^^^^

By default ``edgy`` copies the executables being tested for each test run.
This allows for (some degree of) working on the front end while testing
occurs as updated binaries will not impact testing.

If the environment variable ``EDG_TEST_NO_COPY_``\ *X* is set or
``TEST_``\ *X* (where *X* is the environment variable corresponding
to the tested binary -- typically ``CPFE`` or ``CPFE_CP``) is set, this
functionality is disabled.

Test Run Cleanup
^^^^^^^^^^^^^^^^

If the environment variable ``EDG_TEST_RUNS_KEPT`` is set to a valid
integer *N* (e.g., 10), ``edgy`` will only keep the last *N* entries in the
run directory.

Note: This does not take into account directory names, it simply deletes
directories in the ``runs`` directory based on their modification time.
Make sure you don't have any subdirectories in the ``runs`` directory that
should be kept around before setting this environment variable.

Multi-Configuration Support
^^^^^^^^^^^^^^^^^^^^^^^^^^^

``edgy``, as previously noted, has support for simultaneous testing of
different configurations.  The configurations that can be tested, as well as
the default set of configurations to test, is configured in the
``.edgy/config.json`` file.

To manually specify which configurations to run, the ``-c`` flag can be
used with a comma-separated list of configurations, e.g.:

.. code:: text

  edgy -c edg_x86_64,edg_x86_64_cp regressions

This will run the regression test suite, in both the ``edg_x86_64`` and
``edg_x86_64_cp`` configurations (which is also the default as edgy is
typically configured).

Expectation Inheritance
~~~~~~~~~~~~~~~~~~~~~~~

Test output recordings and test expectations are both inherited from the
default configuration (``edg_x86_64`` unless ``RUN_TEST_DEFAULT_CONFIG`` says
otherwise).  For expectations, each configuration other than the default
configuration starts from the default configuration's expectations
(``tests/expectations/<default>/<suite>.log``), and its own expectations file
(``tests/expectations/<config>/<suite>.log``) only needs to hold the
differences.  For each test (and test case):

* a line in the configuration's own file is the expectation, otherwise
* a line in the default configuration's file is the expectation, otherwise
* the test is expected to pass.

As the absence of a line means the default configuration's expectation applies,
a configuration that passes a test that the default configuration fails
records an explicit ``PASS`` line (e.g.,
``regressions/foo.sft.cpp:fp:-DTEST_NUMBER=1:PASS:``) to say so.  Lines in a
configuration's own file that are the same as the default configuration's
are redundant, and are removed the next time the configuration is re-recorded.
A configuration with an empty (or missing) expectations file expects exactly
what the default configuration does.

edg-run-test
------------

Usage information:

.. code:: text

  usage: edg-run-test [-h] [-d] [-r] [--driver-debug] [--faster] [-q] [-t TOP_DIR]
                      [-o OUTPUT_DIR] test_files [test_files ...]

  EDG Python based test executor.

  positional arguments:
    test_files            the paths to the test files to run

  options:
    -h, --help            show this help message and exit
    -d, --debug           keep the output files
    -r, --record, --silent-record
                          update the expected test output to be the output from this run.
    --driver-debug        add --driver_debug to the test's command line arguments. The
                          resulting additional output will be printed to stdout with the
                          original test file replacing the generic name. This makes the
                          underlying command more easily accessible so that it may be
                          more easily fed to a debugger (e.g., gdb) or checked for
                          correctness.
    --faster              enable command line flags that might speed up the compilation 
                          of the test files
    -q                    only display log output for errors
    -t, --top-dir TOP_DIR
                          the path to the contextually "top" directory
    -o OUTPUT_DIR         the path where output will be stored

``edg-run-test`` is a considerably simpler tool than ``edgy``.  Consider
the following edgy command:

.. code:: bash

  edgy regressions

Assuming ``EDG_TEST_HOME`` is ``/edg/tests``, the corresponding
``edg-run-test`` invocation would be approximately the following:

.. code:: bash

  edg-run-test -t /edg/tests/tests/regressions \
    /edg/tests/tests/regressions/**/*.sft.c   \
    /edg/tests/tests/regressions/**/*.sft.cpp \
    /edg/tests/tests/regressions/**/*.mft.c   \
    /edg/tests/tests/regressions/**/*.mft.cpp

Here ``TEST_ROOT`` is specified by ``-t`` so that any ``.run_test_flags`` file
can be correctly processed.

Of note, ``edg-run-test`` does not support expectations or have a notion of
configuration, so this will report the status of all tests regardless of
expected output.

Additionally, by default all statuses are reported; ``-q`` can be used to
reduce the output to something closer to what's given by ``edgy``.

The tested binary is the binary found at the ``CPFE`` environment variable by
default.  However, alternative environment variables can be tested by
specifying the ``PRIMARY_CPFE`` environment variable to a different environment
variable (e.g., ``CPFE_CUSTOM``).

The occasional advantage of ``edg-run-test`` over ``edgy`` is simplicity.
It's possible to author a new test outside of the test tree and quickly
execute and iterate on the test before moving the file(s) to include the
test in a test suite.
