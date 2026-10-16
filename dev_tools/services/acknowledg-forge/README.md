# acknowledg-forge

acknowledg-forge connects a code forge (GitHub or GitLab today) to
[AcknowlEDG](../acknowledg/README.md). For every open pull request (merge
request) it keeps an AcknowlEDG review alive, using the same review id for the
pull request's whole life (including after it's reopened). Each review has
these sections, when they apply:

| Section | Source |
| --- | --- |
| Test - Changes | The pull request's expectation and recording diffs, recreated with `edg-rematerialize-test-run` on the merge-base |
| Test - Failures | Unexpected differences from the latest CI run for the pull request's head (the `test-results-all` artifact) |
| Bench - C, Bench - C++ | The CI cachegrind results (a `bench-results` artifact, once CI produces one), compared with the checked-in `benchmarks/baselines/project/release-{c,cpp}` |

Optionally, a service account comments on the pull request whenever what's
served changes in a meaningful way: once the pull request has *settled* (CI for
its head finished, or `ACKNOWLEDG_FORGE_SETTLE_TIMEOUT` passed), with every
section that changed in a single comment. Restarts, closing, merging, and
reopening with the same results post nothing.

## How it works

```
 webhooks (optional) ─┐
                      ├─► ingest ─► database ─► processor ─► AcknowlEDG
 polling (always) ────┘                            │  └────► comments (optional)
                                                   ▼
                                     clones, worktrees, run directories
```

- **Polling** always runs once at startup (catching up on anything missed
  while the service was down) and then every `ACKNOWLEDG_FORGE_POLL_INTERVAL`
  seconds, unless that is `0`.
- **Webhooks** are optional. A delivery only identifies what may have changed;
  the service re-queries the forge, so polling and webhooks have the same
  effect.
- **The database** (SQLite by default, PostgreSQL by URL) records each pull
  request, its latest CI run, and what's served for it. The processor
  reconciles each changed pull request: it builds missing or stale sections,
  publishes exactly the sections that apply, and removes everything when the
  pull request closes.
- **Disk use is kept small.** There is one blobless clone per repository. Each
  review gets worktrees whose sparse checkouts list individual tests. Run
  directories are deleted once superseded, and a clone is deleted when its
  last open pull request closes.

The shared, forge-independent code lives in
[`dev_tools/pylibs/edgacknowledg/forge`](../../pylibs/edgacknowledg/forge).
Each forge is a separate service here (`github/`, `gitlab/`) that implements
the `Forge`, `ForgeWebhooks`, and `ForgeCommenter` protocols.

## Untrusted content

Pull requests, and the CI artifacts their runs produce, are untrusted:

- The EDG tooling the service runs comes from a frozen, reviewed copy
  (`ACKNOWLEDG_FORGE_TOOLS_DIR`; the container image embeds one), never from a
  pull request. A pull request's diff is only input to `patch`, applied to its
  merge-base with a served branch (`ACKNOWLEDG_FORGE_BASE_BRANCHES`).
- Every command that reads pull request content (`edg-rematerialize-test-run`
  and `patch`, plus `ifc-dump` and `cg_annotate` run for reviewers) runs under
  [bubblewrap](https://github.com/containers/bubblewrap). It gets no network,
  no capabilities, and a cleared environment, sees only the files it needs,
  and can write only its output directory. It can't create user namespaces
  either, so the service requires bubblewrap 0.8 or later and refuses to
  start with an older one. Reviewer-triggered commands are also time-limited.
- Answering reviewers means reading pull request content, so it happens in a
  separate process (`publisher_worker`), also under bubblewrap. That process
  has the network, to reach AcknowlEDG. It sees the review trees, read-only,
  and nothing else of the service: no credentials, database, or clones. Its
  only secret is the AcknowlEDG API key, which it gets over a pipe. If it
  dies, the service restarts it and serves every review again. At startup,
  the service checks that bubblewrap can run nested this way.
- **Symlinks.** A pull request can commit symlinks. Several layers keep the
  service from reading through them:
  - Clones check symlinks out as plain files holding the link's target
    (`core.symlinks=false`), so no worktree contains one. Symlinked tests
    therefore show their target path rather than its contents.
  - Clones also fsck everything fetched.
  - After every checkout, a worktree may hold only directories and regular
    files, and so may what `edg-rematerialize-test-run` writes (which also
    may not hold hard links). A violation fails the section rather than
    serving it.
  - The section's directories, including the bench baselines a pull request
    supplies, must be real directories.
  - The handlers serving reviewers open files without following any symlink,
    and only under the section's tree.
  - The service refuses to write through symlinks.
- Artifacts are size-capped, including the decompressed size of the
  `.rt.tar.gz` files inside them, and may not contain links or escaping paths.
- Sections are read-only. Reviewers can't accept diffs or update baselines
  through the service.
- Secrets belong in credentials. A secret passed in an environment variable is
  readable from the process's own `/proc/self/environ`, so the service warns
  at startup when sandboxing is on and a secret came from the environment.
- Comments contain only fixed text, commit hashes, and links the service
  generates.

## Host requirements

The service normally runs in the `acknowledg-forge` container image
([`dev_tools/docker/acknowledg-forge`](../../docker/acknowledg-forge)). The
image pins every dependency:

- Fedora's system packages (git, bubblewrap, patch, diffutils, valgrind);
- a frozen `dev_tools/bin` and `dev_tools/pylibs`;
- each service with its `uv.lock`-pinned Python dependencies.

The host then needs only rootless Podman (with Quadlet), systemd, and network
access to the forge and the AcknowlEDG API.

Running directly on a host instead needs Python 3.11 or later, git 2.35 or
later, bubblewrap 0.8 or later with unprivileged user namespaces (Ubuntu
24.04 and later have it), `python3`, `patch`, `diff` and `cg_annotate` under
`/usr`, and a `dev_tools` copy for `ACKNOWLEDG_FORGE_TOOLS_DIR`.

## Configuration

Settings come from the environment. Secrets come from systemd credentials, or
from the environment variable shown as a fallback.

| Variable | Default | Meaning |
| --- | --- | --- |
| `ACKNOWLEDG_FORGE_HOME` | (required; image: `/var/lib/acknowledg-forge`) | Storage root for clones, worktrees, and the default database |
| `ACKNOWLEDG_FORGE_TOOLS_DIR` | (required; image: `/edg/tools`) | A `dev_tools`-style directory (`bin/`, `pylibs/`) with the EDG tooling to run: `edg-rematerialize-test-run`, `ifc-dump` |
| `ACKNOWLEDG_FORGE_REPOSITORIES` | (required) | Comma-separated repositories: `owner/name` on GitHub, project paths on GitLab |
| `ACKNOWLEDG_FORGE_DATABASE_URL` | `sqlite+aiosqlite:///$HOME/state.db` | SQLAlchemy async URL, e.g. `postgresql+asyncpg://…` (the image includes the `postgresql` extra). Use the `database-url` credential instead when the URL holds a password |
| `ACKNOWLEDG_FORGE_BASE_BRANCHES` | `main` | Comma-separated target branches whose pull requests are served |
| `ACKNOWLEDG_FORGE_API_URL` | github.com / gitlab.com | Forge API URL, e.g. `https://ghe.example/api/v3` or `https://gitlab.example/api/v4` |
| `ACKNOWLEDG_FORGE_POLL_INTERVAL` | `300` | Seconds between polls; `0` polls only at startup |
| `ACKNOWLEDG_FORGE_WEBHOOK_BIND` | (unset) | `host:port` to receive webhooks on; unset runs no web server at all |
| `ACKNOWLEDG_FORGE_CI_WORKFLOW` | (unset) | CI to track: the GitHub workflow file (e.g. `build-and-test-ci.yml`), or the GitLab pipeline name (`*` for any). Unset disables CI tracking (and the Failures and Bench sections) |
| `ACKNOWLEDG_FORGE_TEST_ARTIFACT` | `test-results-all` | Test results artifact (GitLab: job) name |
| `ACKNOWLEDG_FORGE_BENCH_ARTIFACT` | `bench-results` | Benchmark results artifact (GitLab: job) name |
| `ACKNOWLEDG_FORGE_COMMENTS` | `0` | `1` enables comments (needs the forge token) |
| `ACKNOWLEDG_FORGE_SETTLE_TIMEOUT` | `7200` | Seconds after a push to stop waiting for CI before commenting |
| `ACKNOWLEDG_FORGE_MAX_ARTIFACT_BYTES` | 256 MiB | Largest artifact download |
| `ACKNOWLEDG_FORGE_MAX_EXTRACTED_BYTES` | 2 GiB | Largest extracted artifact (including inner tarballs) |
| `ACKNOWLEDG_FORGE_MAX_CONCURRENCY` | `2` | Pull requests processed at once |
| `ACKNOWLEDG_FORGE_REMATERIALIZE_TIMEOUT` | `900` | Seconds `edg-rematerialize-test-run` may run |
| `ACKNOWLEDG_FORGE_UNSANDBOXED` | `0` | `1` runs commands without bubblewrap. **Local development only.** |
| `EDG_ACKNOWLEDG_API_HOST`, `EDG_ACKNOWLEDG_API_PORT` | `localhost`, `2727` | AcknowlEDG API |
| `EDG_ACKNOWLEDG_UI_HOST`, `EDG_ACKNOWLEDG_UI_PORT` | `localhost`, `5173` | AcknowlEDG UI, used for links in comments |

| Credential | Fallback variable | Meaning |
| --- | --- | --- |
| `forge-token` | `ACKNOWLEDG_FORGE_TOKEN` | Forge API token (also used for git) |
| `webhook-secret` | `ACKNOWLEDG_FORGE_WEBHOOK_SECRET` | Webhook secret; required for webhooks |
| `database-url` | `ACKNOWLEDG_FORGE_DATABASE_URL` | Database URL, for one holding a password (optional) |
| `acknowledg-api-key` | `EDG_ACKNOWLEDG_API_KEY` | AcknowlEDG API key, if the server requires one |

### GitHub

- **Token:** a fine-grained token (or a bot account's token) with read access
  to *Contents*, *Pull requests*, and *Actions* on the configured
  repositories. To comment, it also needs write access to *Pull requests*.
- **Webhook:** use content type `application/json`, the webhook secret, and
  the *Pull requests* and *Workflow runs* events. Point it at
  `https://<host>/webhook`, typically behind a reverse proxy to
  `ACKNOWLEDG_FORGE_WEBHOOK_BIND`.
- **CI:** set `ACKNOWLEDG_FORGE_CI_WORKFLOW=build-and-test-ci.yml`. Its
  `publish-test-results` job uploads `test-results-all`, including the
  `ci-meta.json` that records the tested (merge) commit. Benchmark results
  need a CI job uploading `bench-results` (`c/` and `cpp/` runs plus
  `ci-meta.json`), which is a separate change.

### GitLab

- **Token:** a project or group access token with the `api` scope
  (`read_api` is enough without comments).
- **Webhook:** use the secret token, with *Merge request events* and
  *Pipeline events*.
- **CI:** artifacts belong to jobs, so `ACKNOWLEDG_FORGE_TEST_ARTIFACT` and
  `ACKNOWLEDG_FORGE_BENCH_ARTIFACT` name the jobs whose artifacts archives hold
  the results, laid out like the GitHub artifacts (configurations or
  `c/`/`cpp/` at the top, plus `ci-meta.json`).

## Deployment

### Image

The `acknowledg-forge Deploy` workflow
(`.github/workflows/acknowledg-forge-deploy.yml`) runs on pushes to `main`
that change the service or the tooling it freezes. It runs the tests, builds
the image with `build-edg-env acknowledg-forge`, and pushes it to
`ghcr.io/edgcpp/acknowledg-forge:latest`. Deployments reference the pushed
image by its digest.

`prebuild.py` embeds that commit's `dev_tools/bin` and `dev_tools/pylibs`, and
builds both services' wheels with their locked requirements. The image is
public. It contains no credentials or deployment configuration, and the build
job has no access to the deployment's secrets.

To build locally, run `build-edg-env acknowledg-forge`; the result is tagged
`ghcr.io/edgcpp/acknowledg-forge:latest`.

### Host setup (once)

The service runs as rootless Podman under its own host account,
`acknowledg-forge`, as a Quadlet *user* unit that the administrator owns in
`/etc/containers/systemd/users/<UID>/`. Podman never runs as root.

- **Credentials:** they stay in `/etc/credstore.encrypted`, root-owned. They
  are scoped to the account, which may read them but not change them.
- **File ownership:** the container's service user is mapped to the account,
  so everything the service writes (its database, clones, and worktrees) is
  owned by `acknowledg-forge` on the host.
- **Everything else:** the container's other IDs, including its root, come
  from the account's subordinate range, which no host user owns.

Everything about the service except its image and AcknowlEDG API and UI
URLs is configured here, by the host's administrator. As root:

1. Create the account, with a subordinate ID range for its containers, and
   enable lingering so its user services run without a login. Note its UID
   (`<UID>` below):

   ```
   useradd --system --create-home --home-dir /var/lib/acknowledg-forge \
     --shell /usr/sbin/nologin acknowledg-forge
   usermod --add-subuids 300000-365535 --add-subgids 300000-365535 \
     acknowledg-forge
   loginctl enable-linger acknowledg-forge
   id -u acknowledg-forge
   ```

   The subordinate range must not overlap other `/etc/subuid` or
   `/etc/subgid` entries. Podman keeps the account's images under its home.

2. Create the state directory (or mount the storage for it there):

   ```
   install -d -m 0700 -o acknowledg-forge -g acknowledg-forge \
     /var/lib/acknowledg-forge/github
   ```

3. Create the credentials. Scope each to the account (`--user --uid`), which
   is the only one able to decrypt it, and let it read but not change them.
   Each command prompts for the secret without echoing it.

   The forge token:

   ```
   mkdir -p /etc/credstore.encrypted
   systemd-ask-password -n 'Forge token:' \
     | systemd-creds encrypt --user --uid=acknowledg-forge \
         --name=forge-token - \
         /etc/credstore.encrypted/acknowledg-forge.forge-token
   chgrp acknowledg-forge /etc/credstore.encrypted/acknowledg-forge.forge-token
   chmod 0640 /etc/credstore.encrypted/acknowledg-forge.forge-token
   ```

   The webhook secret (the same value as in the forge's webhook settings; for
   example, generate one with `openssl rand -hex 32`):

   ```
   systemd-ask-password -n 'Webhook secret:' \
     | systemd-creds encrypt --user --uid=acknowledg-forge \
         --name=webhook-secret - \
         /etc/credstore.encrypted/acknowledg-forge.webhook-secret
   chgrp acknowledg-forge \
     /etc/credstore.encrypted/acknowledg-forge.webhook-secret
   chmod 0640 /etc/credstore.encrypted/acknowledg-forge.webhook-secret
   ```

   The AcknowlEDG API key, only if the AcknowlEDG server requires one (then
   also uncomment its `LoadCredentialEncrypted=` line in the unit below):

   ```
   systemd-ask-password -n 'AcknowlEDG API key:' \
     | systemd-creds encrypt --user --uid=acknowledg-forge \
         --name=acknowledg-api-key - \
         /etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   chgrp acknowledg-forge \
     /etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   chmod 0640 /etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   ```

   To rotate a secret, rerun its commands and restart the service:

   ```
   systemctl --user --machine=acknowledg-forge@ restart acknowledg-forge-github
   ```

4. Optionally, use PostgreSQL instead of the default SQLite database (which
   lives in the state directory). The service creates its schema on startup.
   Switching gives the service a fresh database: it rebuilds what it serves
   and adopts existing pull requests' results without commenting.

   On this host, connect over PostgreSQL's Unix socket with peer
   authentication. The container's user is the host's `acknowledg-forge`
   account, so no password is needed. Create its role and database, then
   uncomment the unit's two "PostgreSQL on this host" lines:

   ```
   apt install postgresql
   runuser -u postgres -- createuser acknowledg-forge
   runuser -u postgres -- createdb --owner=acknowledg-forge acknowledg_forge
   ```

   For a remote server, the URL holds a password, so store it as a credential
   instead and uncomment the unit's `database-url` line:

   ```
   systemd-ask-password -n 'Database URL:' \
     | systemd-creds encrypt --user --uid=acknowledg-forge \
         --name=database-url - \
         /etc/credstore.encrypted/acknowledg-forge.database-url
   chgrp acknowledg-forge \
     /etc/credstore.encrypted/acknowledg-forge.database-url
   chmod 0640 /etc/credstore.encrypted/acknowledg-forge.database-url
   ```

   Enter it as
   `postgresql+asyncpg://<ROLE>:<PASSWORD>@<HOST>:<PORT>/<DATABASE>`.

5. Save the unit below as
   `/etc/containers/systemd/users/<UID>/acknowledg-forge-github.container`,
   replacing the repository. Then load it:

   ```
   systemctl --user --machine=acknowledg-forge@ daemon-reload
   ```

```
[Unit]
Description=acknowledg-forge (GitHub)
After=network-online.target
Wants=network-online.target

[Container]
# Deployments override the image (pinned by digest) in a drop-in; see below.
Image=ghcr.io/edgcpp/acknowledg-forge:latest
ContainerName=acknowledg-forge-github
Exec=acknowledg-forge-github
# The container's 'acknowledg' user (1000), which runs the service, is the
# host's acknowledg-forge account.
UserNS=keep-id:uid=1000,gid=1000
# bubblewrap inside the container mounts its own /proc, which Podman's
# masking of /proc paths prevents.
PodmanArgs=--security-opt unmask=/proc/*
Volume=/var/lib/acknowledg-forge/github:/var/lib/acknowledg-forge:Z
# The credentials systemd decrypted for this unit.
Volume=%d:/run/credentials:ro
Environment=CREDENTIALS_DIRECTORY=/run/credentials
Environment=ACKNOWLEDG_FORGE_REPOSITORIES=<OWNER>/<REPOSITORY>
Environment=ACKNOWLEDG_FORGE_CI_WORKFLOW=build-and-test-ci.yml
Environment=ACKNOWLEDG_FORGE_COMMENTS=1
Environment=ACKNOWLEDG_FORGE_WEBHOOK_BIND=0.0.0.0:2728
PublishPort=127.0.0.1:2728:2728
# Optional: PostgreSQL on this host, over its Unix socket with peer
# authentication (see step 4).
#Volume=/run/postgresql:/run/postgresql
#Environment=ACKNOWLEDG_FORGE_DATABASE_URL=postgresql+asyncpg://acknowledg-forge@/acknowledg_forge?host=/run/postgresql

[Service]
LoadCredentialEncrypted=forge-token:/etc/credstore.encrypted/acknowledg-forge.forge-token
LoadCredentialEncrypted=webhook-secret:/etc/credstore.encrypted/acknowledg-forge.webhook-secret
#LoadCredentialEncrypted=acknowledg-api-key:/etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
# Optional: a remote PostgreSQL, whose URL (with its password) is a credential
# (see step 4).
#LoadCredentialEncrypted=database-url:/etc/credstore.encrypted/acknowledg-forge.database-url
# Starting may include pulling the image.
TimeoutStartSec=600
Restart=on-failure
RestartSec=10

[Install]
WantedBy=default.target
```

For GitLab, use `Exec=acknowledg-forge-gitlab` with its own unit name and
state directory.

### Deploying from CI

The workflow's `deploy` job connects to the host over SSH as a deploy user,
which needs no other access to the host. Install the deploy helper, and let
that user run it, and only it, as root:

```
install -m 0755 -o root -g root \
  dev_tools/services/acknowledg-forge/deploy-remote.sh \
  /usr/local/libexec/acknowledg-forge-deploy
echo '<DEPLOY USER> ALL=(root) NOPASSWD: /usr/local/libexec/acknowledg-forge-deploy' \
  > /etc/sudoers.d/acknowledg-forge
```

The helper ([`deploy-remote.sh`](deploy-remote.sh)) manages a single root-owned
drop-in next to the unit,
`acknowledg-forge-github.container.d/50-ci-deploy.conf`, which sets the unit's
`Image=` and the `EDG_ACKNOWLEDG_{API,UI}_{HOST,PORT}` the AcknowlEDG URLs
translate to. It pulls the image into the `acknowledg-forge` account's storage
and drives that account's user services
(`systemctl --user --machine=acknowledg-forge@`).

- **Strict arguments:** it accepts only images from
  `ghcr.io/edgcpp/acknowledg-forge` pinned by digest, `ws(s)://` and
  `http(s)://` URLs with no path, and a service named `acknowledg-forge-*`.
  The service picks TLS by port alone, so the helper also rejects URLs it
  couldn't honor exactly: TLS on a port other than 443, or plain
  `ws://`/`http://` on 443. A leaked deploy key can therefore only
  deploy another published image.
- **What it does:** it pulls the image and restarts the service only if the
  drop-in changed (otherwise it just makes sure the service is running), then
  checks that the service stays up.
- **What it leaves alone:** the base unit and the credentials.
- **Updates:** the installed copy changes only when the administrator
  reinstalls it.

The deploy job reads these secrets from the `acknowledg-forge` GitHub
environment:

| Secret | Purpose |
| --- | --- |
| `ACKNOWLEDG_FORGE_SSH_HOST` | Host name or IP address |
| `ACKNOWLEDG_FORGE_SSH_USER` | The deploy user |
| `ACKNOWLEDG_FORGE_SSH_PORT` | SSH port (optional; defaults to 22) |
| `ACKNOWLEDG_FORGE_SSH_PRIVATE_KEY` | Private key for that user |
| `ACKNOWLEDG_FORGE_SSH_KNOWN_HOSTS` | `ssh-keyscan` output for the host |
| `ACKNOWLEDG_FORGE_SERVICE` | The unit's name (optional; defaults to `acknowledg-forge-github`) |

And these **repository variables** (public, so not secrets; the API URL is
shared with the AcknowlEDG deploy, which builds the web client with it):

| Variable | Purpose |
| --- | --- |
| `ACKNOWLEDG_API_HOST` | Public AcknowlEDG API URL, e.g. `wss://acknowledg-api.example` |
| `ACKNOWLEDG_UI_HOST` | Public AcknowlEDG UI URL, e.g. `https://acknowledg.example`, for links in comments |

The host needs Podman with Quadlet drop-in support (tested with Podman 5.4)
and systemd 256 or later (for user-scoped encrypted credentials).
The first
published package on GHCR is private; make it public in the package's
settings so the host can pull it without logging in.

Notes:

- **Isolation:** the container keeps the service and its dependencies away
  from the host. Inside the container, bubblewrap still isolates whatever
  reads pull request content from the service: from its credentials, its
  database, and its clones. That covers the commands, and the worker process
  that answers reviewers. The commands are also isolated from other reviews. `unmask=/proc/*` only affects what the
  container's `/proc` shows. In a rootless container those files stay
  unreadable to it.
- **Shutdown:** stopping the service (SIGTERM) shuts it down cleanly. Because
  state lives in the database and workspaces on the volume, a restart, or a
  newer image, restores what was served without rebuilding it or commenting.
- **One instance:** run only one instance per AcknowlEDG server. AcknowlEDG
  allows a single provider per review section.

### Without a container

Install a service wheel (`build.sh`) into a virtual environment and run
`acknowledg-forge-github` (or `-gitlab`) with the same settings, plus
`ACKNOWLEDG_FORGE_HOME` and `ACKNOWLEDG_FORGE_TOOLS_DIR`. Under systemd, pass
secrets with `LoadCredentialEncrypted=`, as for the
[AcknowlEDG server](../acknowledg/README.md#server). Don't add
`RestrictNamespaces=` or `PrivateUsers=`, which would stop bubblewrap.

## Development

```
github/setup-venv.sh            # or gitlab/
tests/run-tests.sh              # pytest: both services and the shared code
typecheck.sh                    # mypy, as strict as typecheck-python.sh
```

To try the service locally, start the AcknowlEDG server and client
([../acknowledg](../acknowledg/README.md)). Then run `github/run-service.sh`
with these set:

- `ACKNOWLEDG_FORGE_HOME`;
- `ACKNOWLEDG_FORGE_REPOSITORIES`;
- `ACKNOWLEDG_FORGE_TOOLS_DIR` (e.g. this checkout's `dev_tools`);
- a token.

Without bubblewrap, also set `ACKNOWLEDG_FORGE_UNSANDBOXED=1`.

Database schema changes go through Alembic. Add a revision under
`edgacknowledg/forge/db/migrations/versions/` (see `migrations/env.py`).

### Adding a forge

Implement `Forge`, and optionally `ForgeWebhooks` and `ForgeCommenter`
(`edgacknowledg/forge/__init__.py`). `edgacknowledg.forge.http` handles the
REST plumbing: rate limits, pagination, and credential-free downloads. Then
add a service directory like `github/`, whose `__main__` passes a
`build(config)` to `edgacknowledg.forge.service.main`.

## Known limitations

- `edg-rematerialize-test-run` currently writes `changes.elog` but no
  `changes.elist`, which the review tooling needs. Until that's fixed, the
  service logs a warning and serves no **Test - Changes** section.
- On GitHub, pull request CI tests the merge commit. Failures and benchmarks
  are served against that commit when it can still be fetched, and against the
  pull request's head otherwise.
- Until CI uploads a `bench-results` artifact (a separate change), no bench
  sections are served.
- The tooling is frozen into the image, so a change to the test-home or
  expectations layout that `edg-rematerialize-test-run` must follow needs a
  new image.
