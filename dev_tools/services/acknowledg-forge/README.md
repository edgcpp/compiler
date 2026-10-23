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

Pull requests, and the CI artifacts their runs produce, are untrusted.
[SANDBOXING.md](SANDBOXING.md) describes in detail how the service handles
them; in short:

- The EDG tooling it runs is a frozen, reviewed copy, never a pull
  request's.
- A deployment is four containers ([`compose.yaml`](compose.yaml)). Only
  the service's holds its credentials, database, and clones. Everything
  that reads pull request content runs in the others: the builder, which
  builds sections; the publisher, which answers reviewers; and tools,
  which runs commands for the publisher. Each sees only the review trees,
  read-only (the builder also writes a staging directory), and reaches only
  the containers it must.
- Checkouts, artifacts, and what the builder writes are checked for
  symlinks and other links before anything reads them, and reviewers' reads
  never follow one.
- Downloads, files read for reviewers, commands, and each container's
  memory are all limited.
- Sections are read-only: reviewers can't accept diffs or update baselines
  through the service.

## Host requirements

The service runs in the `acknowledg-forge` container image
([`dev_tools/docker/acknowledg-forge`](../../docker/acknowledg-forge)), as
the containers in [`compose.yaml`](compose.yaml). The image pins every
dependency:

- Fedora's system packages (git, patch, diffutils, valgrind);
- a frozen `dev_tools/bin` and `dev_tools/pylibs`;
- each service with its `uv.lock`-pinned Python dependencies.

The host then needs only Docker Engine with the Compose plugin (2.17 or
later), systemd, a reverse proxy for webhooks, network access to the forge
and the AcknowlEDG API, and, for deploying from CI, `python3` (3.8 or
later; the deploy helper uses only the standard library). It needs no
unprivileged user namespaces, and no AppArmor changes.

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
| `ACKNOWLEDG_FORGE_WEBHOOK_BIND` | (unset) | `host:port` to receive webhooks on (in `compose.yaml`: `service-frontend:2728`); unset runs no web server at all |
| `ACKNOWLEDG_FORGE_CI_WORKFLOW` | (unset) | CI to track: the GitHub workflow file (e.g. `build-and-test-ci.yml`), or the GitLab pipeline name (`*` for any). Unset disables CI tracking (and the Failures and Bench sections) |
| `ACKNOWLEDG_FORGE_TEST_ARTIFACT` | `test-results-all` | Test results artifact (GitLab: job) name |
| `ACKNOWLEDG_FORGE_BENCH_ARTIFACT` | `bench-results` | Benchmark results artifact (GitLab: job) name |
| `ACKNOWLEDG_FORGE_COMMENTS` | `0` | `1` enables comments (needs the forge token) |
| `ACKNOWLEDG_FORGE_SETTLE_TIMEOUT` | `7200` | Seconds after a push to stop waiting for CI before commenting |
| `ACKNOWLEDG_FORGE_MAX_ARTIFACT_BYTES` | 256 MiB | Largest artifact download |
| `ACKNOWLEDG_FORGE_MAX_EXTRACTED_BYTES` | 2 GiB | Largest extracted artifact (including inner tarballs) |
| `ACKNOWLEDG_FORGE_MAX_CONCURRENCY` | `2` | Pull requests processed at once |
| `ACKNOWLEDG_FORGE_REMATERIALIZE_TIMEOUT` | `900` | Seconds `edg-rematerialize-test-run` may run |
| `ACKNOWLEDG_FORGE_UNSANDBOXED` | `0` | `1` runs without containers: the service serves the runner servers and the publisher server itself, on `127.0.0.1`, where any local process can reach them. **Local development on a single-user machine only.** |
| `ACKNOWLEDG_FORGE_{BUILDER,PUBLISHER}_SERVICE_URL` | (set by `compose.yaml`) | The builder's runner server and the publisher server; required unless unsandboxed |
| `ACKNOWLEDG_FORGE_TOOLS_SERVICE_URL` | (set by `compose.yaml`) | The tools runner server, for the publisher server |
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
  only the *Pull requests* and *Workflow runs* events (choose *Let me select
  individual events* and clear *Pushes*, which the service ignores; a push
  to a pull request arrives as a *Pull requests* event). Point it at
  `https://<host>/webhook`, typically behind a reverse proxy to
  `ACKNOWLEDG_FORGE_WEBHOOK_BIND` (published on the host's `127.0.0.1`).
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
job has no access to the deployment's secrets. It has no default command:
[`compose.yaml`](compose.yaml) picks each container's, and `FORGE` picks the
forge's service (`acknowledg-forge-github` or `acknowledg-forge-gitlab`).

To build locally, run `build-edg-env acknowledg-forge`; the result is tagged
`ghcr.io/edgcpp/acknowledg-forge:latest`.

### Host setup (once)

A deployment is a systemd unit that runs [`compose.yaml`](compose.yaml) with
Docker, as the daemon normally runs (as root). The containers never run as
root: they run as the host's `acknowledg-forge` account, which owns the
deployment's state, has no login, and isn't in the `docker` group (which
would make it root's equal).

- **Credentials:** they stay in `/etc/credstore.encrypted`, encrypted and
  root-only. When the unit starts, systemd decrypts them, and
  [`prepare-runtime.sh`](prepare-runtime.sh) hands them to the service's
  container only, in memory (`/run`).
- **File ownership:** everything the service writes (its database, clones,
  and worktrees) is owned by `acknowledg-forge` on the host.
- **Ports:** the only one published is the webhook port, on `127.0.0.1`
  only. Docker's published ports bypass the host's firewall, so nothing is
  reachable from outside the host unless a reverse proxy there exposes it.

Everything about the service except its image and AcknowlEDG API and UI
URLs is configured here, by the host's administrator. As root:

1. Install Docker Engine and its Compose plugin (`docker-ce`,
   `docker-compose-plugin`; see Docker's instructions for the
   distribution). Create the account, and note its UID and GID (`<UID>`,
   `<GID>` below):

   ```
   useradd --system --no-create-home --shell /usr/sbin/nologin \
     acknowledg-forge
   id -u acknowledg-forge
   id -g acknowledg-forge
   ```

2. Create the state directory (or mount the storage for it there). Any
   other path works too; give it as `STATE_DIR` in step 5, which is the only
   other place it is named:

   ```
   install -d -m 0700 -o acknowledg-forge -g acknowledg-forge \
     /var/lib/acknowledg-forge/github
   ```

3. Save the unit below as
   `/etc/systemd/system/acknowledg-forge-github.service`. The next steps
   create the files it uses, and some uncomment its optional lines; it is
   started in step 7.

   ```
   [Unit]
   Description=acknowledg-forge (GitHub)
   Requires=docker.service
   After=docker.service network-online.target
   Wants=network-online.target

   [Service]
   LoadCredentialEncrypted=forge-token:/etc/credstore.encrypted/acknowledg-forge.forge-token
   # Webhooks (see step 4); remove to only poll.
   LoadCredentialEncrypted=webhook-secret:/etc/credstore.encrypted/acknowledg-forge.webhook-secret
   #LoadCredentialEncrypted=acknowledg-api-key:/etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   # Optional: a remote PostgreSQL, whose URL (with its password) is a credential
   # (see step 6).
   #LoadCredentialEncrypted=database-url:/etc/credstore.encrypted/acknowledg-forge.database-url
   # The service container's credentials, in memory; removed on stop.
   RuntimeDirectory=acknowledg-forge-github
   # For STATE_DIR (step 5).
   EnvironmentFile=/etc/acknowledg-forge/acknowledg-forge-github.env
   ExecStartPre=/usr/local/libexec/acknowledg-forge-prepare \
     /run/acknowledg-forge-github ${STATE_DIR} acknowledg-forge
   ExecStart=/usr/bin/docker compose --project-name acknowledg-forge-github \
     --file /etc/acknowledg-forge/compose.yaml \
     --env-file /etc/acknowledg-forge/acknowledg-forge-github.env \
     --env-file /etc/acknowledg-forge/acknowledg-forge-github.deploy.env \
     up --abort-on-container-exit
   ExecStop=/usr/bin/docker compose --project-name acknowledg-forge-github \
     --file /etc/acknowledg-forge/compose.yaml \
     --env-file /etc/acknowledg-forge/acknowledg-forge-github.env \
     --env-file /etc/acknowledg-forge/acknowledg-forge-github.deploy.env \
     down --timeout 30
   # Starting may include pulling the image.
   TimeoutStartSec=600
   Restart=on-failure
   RestartSec=10

   [Install]
   WantedBy=multi-user.target
   ```

4. Create the credentials. Each command prompts for the secret without
   echoing it.

   The forge token:

   ```
   mkdir -p /etc/credstore.encrypted
   systemd-ask-password -n 'Forge token:' \
     | systemd-creds encrypt --name=forge-token - \
         /etc/credstore.encrypted/acknowledg-forge.forge-token
   ```

   The webhook secret (the same value as in the forge's webhook settings; for
   example, generate one with `openssl rand -hex 32`). Webhooks are optional:
   to only poll, skip this, and remove the unit's `webhook-secret` line and
   the settings' `ACKNOWLEDG_FORGE_WEBHOOK_BIND` (step 5), or the service
   refuses to start:

   ```
   systemd-ask-password -n 'Webhook secret:' \
     | systemd-creds encrypt --name=webhook-secret - \
         /etc/credstore.encrypted/acknowledg-forge.webhook-secret
   ```

   The AcknowlEDG API key, only if the AcknowlEDG server requires one (then
   also uncomment its `LoadCredentialEncrypted=` line in the unit):

   ```
   systemd-ask-password -n 'AcknowlEDG API key:' \
     | systemd-creds encrypt --name=acknowledg-api-key - \
         /etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   ```

   Once the service is running, to rotate a secret, rerun its command and
   restart the service:

   ```
   systemctl restart acknowledg-forge-github
   ```

5. Install the deployment's files, from the repository's `main` branch:
   the compose file, the runtime helper, and an empty
   `acknowledg-forge-github.deploy.env`, which only the deploy helper writes
   (until the first deploy, the deployment runs
   `ghcr.io/edgcpp/acknowledg-forge:latest`):

   ```
   base=https://raw.githubusercontent.com/edgcpp/compiler/main/dev_tools/services/acknowledg-forge
   install -d -m 0755 /etc/acknowledg-forge
   curl -fsSL -o /etc/acknowledg-forge/compose.yaml "${base}/compose.yaml"
   chmod 0644 /etc/acknowledg-forge/compose.yaml
   curl -fsSL -o /usr/local/libexec/acknowledg-forge-prepare \
     "${base}/prepare-runtime.sh"
   chmod 0755 /usr/local/libexec/acknowledg-forge-prepare
   install -m 0644 /dev/null \
     /etc/acknowledg-forge/acknowledg-forge-github.deploy.env
   ```

   The runtime helper runs as root; read it (and the compose file) before
   installing it. Fetch them again the same way when they change.

   Then save the deployment's variables as
   `/etc/acknowledg-forge/acknowledg-forge-github.env`. This is a different
   file from `acknowledg-forge-github.deploy.env` above, which the deploy
   helper overwrites on every deploy:

   ```
   FORGE=github
   FORGE_UID=<UID>
   FORGE_GID=<GID>
   # The state directory (step 2).
   STATE_DIR=/var/lib/acknowledg-forge/github
   SETTINGS_FILE=/etc/acknowledg-forge/acknowledg-forge-github.settings
   CREDENTIALS_DIR=/run/acknowledg-forge-github/credentials
   # The webhook port on the host's 127.0.0.1, for the reverse proxy.
   WEBHOOK_PORT=2728
   ```

   And the service's settings (see [Configuration](#configuration)) as
   `/etc/acknowledg-forge/acknowledg-forge-github.settings`, replacing the
   repository:

   ```
   ACKNOWLEDG_FORGE_REPOSITORIES=<OWNER>/<REPOSITORY>
   ACKNOWLEDG_FORGE_CI_WORKFLOW=build-and-test-ci.yml
   ACKNOWLEDG_FORGE_COMMENTS=1
   # Webhooks (with the webhook secret, step 4); remove to only poll.
   ACKNOWLEDG_FORGE_WEBHOOK_BIND=service-frontend:2728
   ```

6. Optionally, use PostgreSQL instead of the default SQLite database (which
   lives in the state directory).

   - If using a PostgreSQL database on the same host:

     Connect over PostgreSQL's Unix socket with peer authentication.  The
     service's container runs as the host's `acknowledg-forge` account, so no
     password is needed.  Create its role and database:

     ```
     apt install postgresql
     runuser -u postgres -- createuser acknowledg-forge
     runuser -u postgres -- createdb --owner=acknowledg-forge acknowledg_forge
     ```

     Then give the service the socket, in
     `/etc/acknowledg-forge/acknowledg-forge-github.override.yaml`, and add
     `--file /etc/acknowledg-forge/acknowledg-forge-github.override.yaml`
     after both of the unit's `--file /etc/acknowledg-forge/compose.yaml`
     lines (in `ExecStart=` and `ExecStop=`):

     ```
     services:
       service:
         volumes:
           - /run/postgresql:/run/postgresql
         environment:
           ACKNOWLEDG_FORGE_DATABASE_URL: postgresql+asyncpg://acknowledg-forge@/acknowledg_forge?host=/run/postgresql
     ```

   - If using a a remote PostgreSQL database:

     Create a secure credential using systemd for the database password:

     ```
     systemd-ask-password -n 'Database URL:' \
       | systemd-creds encrypt --name=database-url - \
           /etc/credstore.encrypted/acknowledg-forge.database-url
     ```

     Then uncomment the unit's `database-url` line in the systemd unit,
     configure `/etc/acknowledg-forge/acknowledg-forge-github.override.yaml`
     from the template below, and add `--file
     /etc/acknowledg-forge/acknowledg-forge-github.override.yaml` after both of
     the unit's `--file /etc/acknowledg-forge/compose.yaml` lines (in
     `ExecStart=` and `ExecStop=`):

     ```
     services:
       service:
         volumes:
           - /run/postgresql:/run/postgresql
         environment:
           ACKNOWLEDG_FORGE_DATABASE_URL: postgresql+asyncpg://<ROLE>:<PASSWORD>@<HOST>:<PORT>/<DATABASE>
     ```

7. Start it:

   ```
   systemctl daemon-reload
   systemctl enable --now acknowledg-forge-github
   ```

If any container exits, the whole deployment stops, and systemd restarts it.

For GitLab, use `FORGE=gitlab` with its own unit name, files, and state
directory.

### Deploying from CI

The workflow's `deploy` job connects to the host over SSH as a deploy user,
which needs no other access to the host. Install the deploy helper, from the
repository's `main` branch, and let that user run it, and only it, as root:

```
base=https://raw.githubusercontent.com/edgcpp/compiler/main/dev_tools/services/acknowledg-forge
curl -fsSL -o /usr/local/libexec/acknowledg-forge-deploy \
  "${base}/deploy-remote.py"
chown root:root /usr/local/libexec/acknowledg-forge-deploy
chmod 0755 /usr/local/libexec/acknowledg-forge-deploy
echo '<DEPLOY USER> ALL=(root) NOPASSWD: /usr/local/libexec/acknowledg-forge-deploy' \
  > /etc/sudoers.d/acknowledg-forge
```

It runs as root on the deploy user's behalf; read it before installing it.

The helper ([`deploy-remote.py`](deploy-remote.py), which needs only the host's
`python3`) manages a single root-owned file,
`/etc/acknowledg-forge/acknowledg-forge-github.deploy.env`, which sets the
deployment's `IMAGE` and the `EDG_ACKNOWLEDG_{API,UI}_{HOST,PORT}` the
AcknowlEDG URLs translate to. It pulls the image and drives the service's
unit.

- **Strict arguments:** it accepts only images from
  `ghcr.io/edgcpp/acknowledg-forge` pinned by digest, `ws(s)://` and
  `http(s)://` URLs with no path, and a service named `acknowledg-forge-*`.
  The service picks TLS by port alone, so the helper also rejects URLs it
  couldn't honor exactly: TLS on a port other than 443, or plain
  `ws://`/`http://` on 443. A leaked deploy key can therefore only
  deploy another published image.
- **What it does:** it pulls the image and restarts the service only if the
  file changed (otherwise it just makes sure the service is running), then
  checks that the service stays up, and removes the repository's other
  images that nothing uses (and no other image).
- **What it leaves alone:** the unit, the compose file, the settings, and
  the credentials.
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

The host needs Docker Engine with Compose 2.17 or later (for several
`--env-file`s)
and systemd 250 or later (for encrypted credentials). The first published
package on GHCR is private; make it public in the package's settings so the
host can pull it without logging in.

Notes:

- **Isolation:** each container sees only what its role needs (see
  [SANDBOXING.md](SANDBOXING.md)).
- **Shutdown:** stopping the service stops the containers cleanly. Because
  state lives in the database and workspaces in the state directory, a
  restart, or a newer image, restores what was served without rebuilding it
  or commenting.
- **One instance:** run only one instance per AcknowlEDG server. AcknowlEDG
  allows a single provider per review section.

### Without the containers

Running the service directly on a host has no sandbox at all: it serves
its runner servers and the publisher server itself, the same way, but
commands that read pull request content run beside its credentials. That is
only for local development (see below), with
`ACKNOWLEDG_FORGE_UNSANDBOXED=1`.

## Development

```
github/setup-venv.sh            # or gitlab/
tests/run-tests.sh              # pytest: both services and the shared code
typecheck.sh                    # mypy, as strict as typecheck-python.sh
```

To try the service locally against `edgcpp/compiler`:

1. Create a [fine-grained personal access
   token](https://github.com/settings/personal-access-tokens/new) with
   *Repository access* set to *Public repositories*. That is read-only
   access to public repositories, and all the script needs: it reads pull
   requests, clones, and downloads CI artifacts, but never comments. Add no
   permissions.
2. Start the AcknowlEDG server and client
   ([../acknowledg](../acknowledg/README.md)).
3. Set `ACKNOWLEDG_FORGE_TOKEN` to the token and run `run-dev-oss-repo.sh`:

   ```
   read -rs ACKNOWLEDG_FORGE_TOKEN && export ACKNOWLEDG_FORGE_TOKEN
   ./run-dev-oss-repo.sh
   ```

   (In fish, `read -sx ACKNOWLEDG_FORGE_TOKEN`.) Reading it this way keeps
   the token out of your shell history.

   If `ACKNOWLEDG_FORGE_TOKEN` isn't set, the script falls back to the GitHub
   CLI's token (`gh auth token`), if you're logged in with `gh auth login`.
   That's convenient, but the CLI's token usually has far broader scopes
   (e.g. `repo`, `workflow`) than the script needs, so prefer the
   fine-grained token.

It serves the pull requests to the local AcknowlEDG, unsandboxed and without
commenting. Its state is kept in a temporary directory (printed when it
starts) and removed when it exits.

For anything else, run `github/run-service.sh` with these set:

- `ACKNOWLEDG_FORGE_HOME`;
- `ACKNOWLEDG_FORGE_REPOSITORIES`;
- `ACKNOWLEDG_FORGE_TOOLS_DIR` (e.g. this checkout's `dev_tools`);
- a token.

Outside the containers, also set `ACKNOWLEDG_FORGE_UNSANDBOXED=1`.

Database schema changes go through Alembic. Add a revision under
`edgacknowledg/forge/db/migrations/versions/` (see `migrations/env.py`).

### Adding a forge

Implement `Forge`, and optionally `ForgeWebhooks` and `ForgeCommenter`
(`edgacknowledg/forge/__init__.py`). `edgacknowledg.forge.http` handles the
REST plumbing: rate limits, pagination, and credential-free downloads. Then
add a service directory like `github/`, whose `__main__` passes a
`build(config)` to `edgacknowledg.forge.service.main`.
