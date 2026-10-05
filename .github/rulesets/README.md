# Branch rulesets

GitHub does not read rulesets out of a repository, so the JSON here is not
applied automatically. It is the reviewed, version-controlled statement of
what the branch protection *should* be; applying it is a deliberate act by
someone with admin rights, either from the command line below or through
**Settings -> Rules -> Rulesets** in the web UI.

Keeping the definition in the repository means a change to the project's
enforcement is proposed, reviewed, and recorded the same way a code change
is, instead of appearing in the settings UI with no history and no
discussion.

## What enforces what

It is worth being precise about where each kind of rule actually lives,
because the three categories behave very differently.

- `.git-hooks/pre-push.py` lives in the repository and runs on the
  contributor's own machine, so it is advisory: it can be skipped with
  `git push --no-verify` and is absent unless `dev-init.py` was run.
- `.github/workflows/*.yml` lives in the repository and runs on GitHub, so
  it cannot be skipped, but a failing run blocks nothing on its own.
- Rulesets live in GitHub's settings, not in the repository, and are
  enforced by GitHub on push and on merge. Only the actors listed in
  `bypass_actors` can get around them.
- `.github/CODEOWNERS` lives in the repository but has no effect until a
  ruleset turns on code owner review.

The middle two are the overlap worth understanding. A workflow file is
version controlled and runs on every pull request, but a failing run blocks
nothing until its job name is listed as a required status check in a
ruleset. `CODEOWNERS` is the mirror image: the file is version controlled,
but it only gates anything when `require_code_owner_review` is turned on
here.

None of this needs GitHub Enterprise. Rulesets, required status checks,
required reviews, and `CODEOWNERS` are all available on free public
repositories. True server-side `pre-receive` hooks are Enterprise Server
only, but a required status check covers the same ground: it runs code the
project controls, on GitHub's infrastructure, and a contributor cannot skip
it.

The one capability genuinely out of reach is **push rulesets** (restricting
file paths, file extensions, or file sizes at push time), which require an
organization on GitHub Team or Enterprise Cloud.

## Applying the ruleset

With the `gh` CLI, authenticated as a repository admin:

```sh
# First time: create it.
gh api --method POST repos/edgcpp/compiler/rulesets \
  --input .github/rulesets/default-branch-protection.json

# Afterwards: update the existing ruleset in place.
gh api --method PUT repos/edgcpp/compiler/rulesets/RULESET_ID \
  --input .github/rulesets/default-branch-protection.json
```

Find `RULESET_ID` with:

```sh
gh api repos/edgcpp/compiler/rulesets --jq '.[] | "\(.id)\t\(.name)"'
```

To confirm what is live, and to check it still matches this file:

```sh
gh api repos/edgcpp/compiler/rulesets/RULESET_ID \
  --jq '{name, enforcement, rules: [.rules[].type]}'
```

## What this ruleset does

Beyond the `deletion` and `non_fast_forward` rules that were already in
place, it adds:

- **A pull request is required to change the default branch.** Direct
  pushes to `main` stop being possible, which is the gap that lets work
  reach `main` today without any check running at all.
- **One approving review**, with stale approvals dismissed when new commits
  are pushed, and the last push requiring approval from someone other than
  the person who pushed it.
- **No merge commits.** `allowed_merge_methods` lists only `squash` and
  `rebase`, so pull requests cannot be merged with a merge commit and the
  history on `main` stays linear. Note this governs the merge button only;
  to stop merge commits arriving any other way, also untick
  **Settings -> General -> Pull Requests -> Allow merge commits**, or add a
  `required_linear_history` rule.
- **Required status checks**: the policy check and both Python typecheck
  legs must pass, and a branch must be up to date with `main` before it can
  merge.

`bypass_actors` is deliberately empty, so the rules apply to administrators
as well. If the project decides maintainers need an escape hatch, add them
here rather than disabling the rule, so the exception is visible.

## Checks deliberately not required yet

`Build & Test CI` and `Strict Build Check` run on every pull request but are
not listed as required. The test legs have a 360 minute timeout and the
Windows build depends on an external SoftFloat checkout, so making them
blocking is a scheduling and flakiness decision for the maintainers rather
than a policy one. They can be added to `required_status_checks` later
using the job names exactly as they appear in the checks list, for example
`Test edg_x86_64` or `Linux GCC debug`.

## Settings that are not expressible here

Two things still have to be set by hand in the web UI, because they are
repository options rather than rules:

- **Settings -> General -> Pull Requests -> Allow auto-merge**, if the
  project wants pull requests to merge themselves once checks pass.
- **Settings -> Actions -> General -> Fork pull request workflows**, which
  governs whether workflows on pull requests from forks run without
  approval. Leaving this at the default ("Require approval for first-time
  contributors") is recommended; note that it means the policy check will
  not report on a new contributor's pull request until a maintainer
  approves the run.
