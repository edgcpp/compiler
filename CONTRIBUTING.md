# Contribution Guide

> [!IMPORTANT]
>
> :construction: Pardon our dust :construction:
>
> This is a work in progress (and somewhat aspirational) document that will
> be updated as we work through establishing the project's open source
> community, build & testing infrastructure, and processes that work best
> for contributors.

## :chipmunk:     - Reporting Problems

> [!IMPORTANT]
>
> Please use GitHub Discussions to file your initial issue.  Once it's
> reproduced a contributor will open an issue for you (along with a pull
> request adding a reduced version of the failing test case -- if possible).

When it comes to compiler bug reports, we need at least the following four
things:

1. A description of what's gone wrong (including what should've happened).
2. What's the problematic source code?
3. What command-line options were used?
4. What front end configuration you're using (i.e., the output when running
   the front end with the `--dump_configuration` flag)?

We would *greatly* appreciate:

1. Effort on your part to reducing the test case to <100 lines of code.

> [!TIP]
>
> If you would like to reduce the issue but don't know how, checkout the
> [cvise](https://github.com/marxin/cvise/) project as this is VERY helpful
> for automatic issue reduction.  Additionally, in many cases AI agents can
> successfully create a reduced test case (that's similar enough to still
> reproduce the problem) in a fraction of the time.

2. You checking to see if the problem reproduces in one of the build presets
   in `CMakePresets.json`.

> [!TIP]
>
> You don't need to check all presets, but it's very helpful when issues are
> reported against configurations that are easily reproduced by the project's
> build system (as this reduces the amount of time it takes for contributors
> to reproduce the problem).
>
> If it doesn't reproduce in any existing configuration, consider contributing
> a new one to make problems in your desired configuration easier to reproduce
> in the future.

## :bulb:         - Requesting Features

If you've got an idea for a feature or improvement to the front end, please
start a GitHub Discussion about your idea.  If there's consensus amoung the
maintainers of the relevant area that the idea is something we would like to
move forward with, an issue will be opened following the forum discussion.

## :technologist: - Contributing Code

> [!CAUTION]
>
> We do accept AI-assisted contributions.  However, you must understand
> these contributions and be prepared to explain them to the extent that you
> can defend design decisions.  The compiler project is a composition of many
> complex components and applications; continuity of maintaince requires
> continuity of understanding.
>
> Thank you for your understanding and cooperation.

If you haven't reviewed the [development guide](HACKING.md) yet and are in the
early stages of getting acclimated to working on the project we recommend you
start there.

All pull request will be reviewed, with priority given to those filed against
an open issue.  Before starting new work it's best to review the status of
existing issues and discussion (it's possible someone is already working on
the item or that there has been significant discussion about the desired
implementation characeristics).

### What CI Enforces

Some of the items below are checked automatically on every pull request by
the `Policy Check` workflow, which runs the same rules as the `pre-push`
hook installed by `dev-init.py`.

These **fail** the check:

- Changed files must stay within 79 columns, must comment their `#endif`
  and `#else` directives, and must not contain repeated-word typos.

These are **reported but do not fail** the check:

- Commit subjects should carry a bracketed tag naming at least one GitHub
  issue or legacy PR, such as `[GH #213]` or `[EDGcpfe/29032]`.  Whether
  commits that belong to no issue must still carry an empty `[]` is an
  open question, so this is advisory for now.
- Spelling, because there is no project dictionary and a compiler code
  base trips `aspell` constantly.  Please still read the spelling
  annotations; they catch real mistakes.

You can run the same check locally before opening a pull request:

```sh
edg-check-policy --base-rev origin/main
```

### Checklist for Success

> [!NOTE]
>
> When possible maintainers will help with minor shortcomings.  However,
> we ask that you do your best to check your work against this list.

- [ ] There is an open issue your pull request addresses.
- [ ] Your commits are tagged with the issue number they're addressing
      (i.e., `[GH #12345]`).
- [ ] You understand your code:
  - [ ] You've explained the high level details of your design.
  - [ ] You've noted alternative designs you considered (particularly
        if diverging from an agreed upon design).
  - [ ] Your code doesn't result in a major (negative) change in compiler
        performance or memory consumption.
  - [ ] Your code changes are reasonably localized and do not affect
        unrelated components (major refactors -- AI or not -- will be accepted
        ONLY from established contributors or by working closely with the
        relevant maintainer(s)).
- [ ] You've put effort into maintaining code quality:
  - [ ] Your code is formatted in "EDG Style".
  - [ ] Your code has good comments.
  - [ ] Your code has a Changes entry (not needed for `dev_tools` or build
        system changes).
- [ ] You used `edg-docker-test`:
  - [ ] You have added tests (or there are existing tests) that FAIL before
        your change but PASS after your change.
  - [ ] You do not see regressions and have carefully reviewed changes in
        recorded outputs with `edg-test-run-diff` (or a similar tool).
  - [ ] You've updated the default recordings after reviewing differences
        with `edg-docker-test -W`.

### Early Design Feedback

Please use the open issue for discussion about the proposed design if
non-trivial.

We do accept "WIP" (work in progress) pull requests for early review and
implementation feedback; when opening a WIP pull request, please explicitly
state the pull request is work in progress and include details about the
feedback you're seeking.  In general, WIP pull request will be lightly
reviewed *only* for the specific feedback you're seeking (and whatever else
might be coincidentally encountered in the process).
