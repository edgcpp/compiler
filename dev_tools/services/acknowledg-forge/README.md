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
its head finished, or `settle_timeout` passed), with every
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
  while the service was down) and then every `poll_interval`
  seconds, unless that is `0`.
- **Webhooks** are optional. A delivery only identifies what may have changed;
  the service re-queries the forge, so polling and webhooks have the same
  effect.
- **The database** (SQLite by default, PostgreSQL by configuration) records
  each pull request, its latest CI run, and what's served for it. The processor
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

The host then needs only Docker Engine (25 or later) with the Compose plugin
(2.20.2 or later), systemd, a reverse proxy for webhooks, network access to
the forge and the AcknowlEDG API, and, for deploying from CI, `python3` (3.8
or later; the deploy helper uses only the standard library). It needs no
unprivileged user namespaces, and no AppArmor changes.

## Configuration

Application settings come only from the TOML file named by
`ACKNOWLEDG_FORGE_SETTINGS_FILE`. Python's built-in `tomllib` reads native
strings, booleans, numbers, and arrays, without interpolation. Keys are flat,
lowercase names. `ForgeServiceConfig.from_file()` is the shared loader;
each service checks the settings required for its role. Unknown keys and
incorrect types are errors. Relative paths are resolved against the TOML
file's directory. Database connection components are typed fields in a
`[database]` table; the database module constructs a SQLAlchemy URL object
when connecting. Config itself has no SQLAlchemy dependency.

Compose mounts `SETTINGS_FILE` at `/etc/acknowledg-forge.toml`, read-only in
all four containers. Publisher workers read `max_rt_entries` and
`max_rt_member_bytes` locally. Section messages use `kind` to discriminate
a `variant` payload: empty for tests, or the baseline directory and tag for
benchmarks. Processor bookkeeping stays in the database. Restart the entire
deployment after changing settings. Secrets are loaded into a separate
`ForgeCredentials` object from named files in
`CREDENTIALS_DIRECTORY`, mounted only in the service's container. The TOML
file is readable in every container, including the builder and tools
containers that handle pull request content, so it accepts no token or API
key settings. The `[database]` table accepts only non-secret components;
the password comes from the `database-password` credential, which only the
service can read.

The deployment's wiring is not a setting: which address each server listens
on, and where the service and publisher find the other servers. That is
what keeps each server to its one network (see
[SANDBOXING.md](SANDBOXING.md)), so `compose.yaml` sets it on each
container's command line, and the TOML rejects it, so a settings change
can't widen a listener:

| Server | Arguments |
| --- | --- |
| Service (`acknowledg-forge-github`, `acknowledg-forge-gitlab`) | `--service-bind=HOST:PORT` (the web app: health checks, and `POST /webhook` with webhooks; unset serves nothing), `--builder-service-url`, `--publisher-service-url` (`ws://`; required unless unsandboxed) |
| Builder and tools runners (`edgacknowledg.forge.builder`, `.tools`) | `--host` (default `127.0.0.1`), `--port`, `--container` |
| Publisher (`edgacknowledg.forge.publisher`) | `--host` (default `127.0.0.1`), `--port`, `--tools-service-url` (`ws://`) |

Only `ACKNOWLEDG_FORGE_SETTINGS_FILE` and `CREDENTIALS_DIRECTORY` remain
application bootstrap environment variables. The host's Compose variables
(`FORGE`, `IMAGE`, `FORGE_UID`, `FORGE_GID`, `STATE_DIR`, `SETTINGS_FILE`,
`CREDENTIALS_DIR`, `WEBHOOK_PORT`) still select images, mounts, identities,
and published ports. Standard runtime variables (`PATH`, `HOME`, `PYTHON*`)
retain their usual purposes.

Migration is explicit: convert old `KEY=value` files and uppercase TOML
keys to lowercase TOML with native types; remove all former Forge setting
and secret environment variables, including `EDG_ACKNOWLEDG_*` endpoint/key
variables. Forge rejects them with migration guidance. Move secrets into
credentials, and AcknowlEDG endpoints into the administrator-owned TOML.
Remove every secret from the old `<service>.settings` file **before**
converting it or pointing `SETTINGS_FILE` at it: Compose mounts that file
into all four containers before Forge reads (and rejects) it, whereas the
old file was only ever the service container's.
Update the installed Compose file, deploy helper, workflow, configuration,
and image together. The deploy helper now takes only service and image;
existing endpoint arguments are rejected. No compatibility fallback or live
reload is provided.

| TOML key | Default | Meaning |
| --- | --- | --- |
| `home` | (required) | Storage root for clones, worktrees, and the default database |
| `tools_dir` | (required) | A `dev_tools`-style directory (`bin/`, `pylibs/`) with the EDG tooling to run: `edg-rematerialize-test-run`, `ifc-dump` |
| `repositories` | (required) | Array of repositories: `owner/name` on GitHub, project paths on GitLab |
| `database` | SQLite at `<home>/state.db` | Typed connection table; see below |
| `base_branches` | `["main"]` | Array of target branches whose pull requests are served |
| `api_url` | github.com / gitlab.com | Forge API URL, e.g. `https://ghe.example/api/v3` or `https://gitlab.example/api/v4`. The forge token is sent here, so it must be `https://` (plain `http://` only to `localhost`), with no credentials, query, or fragment |
| `poll_interval` | `300` | Seconds between polls; `0` polls only at startup |
| `webhooks` | `false` | `true` receives webhooks on `--service-bind` (needs the webhook secret) |
| `ci_workflow` | (omitted) | CI to track: the GitHub workflow file (e.g. `build-and-test-ci.yml`), or the GitLab pipeline name (`*` for any). Omit this key to disable CI tracking (and the Failures and Bench sections) |
| `test_artifact` | `test-results-all` | Test results artifact (GitLab: job) name |
| `bench_artifact` | `bench-results` | Benchmark results artifact (GitLab: job) name |
| `comments` | `false` | `true` enables comments (needs the forge token) |
| `settle_timeout` | `7200` | Seconds after a push to stop waiting for CI before commenting |
| `max_artifact_bytes` | 256 MiB | Largest artifact download |
| `max_extracted_bytes` | 2 GiB | Largest extracted artifact (including stored, compressed inner tarballs, but not their unextracted contents) |
| `max_artifact_entries` | `500000` | Most ZIP entries in an artifact. Inner tarballs are checked when read |
| `max_artifact_directory_bytes` | 64 MiB | Largest artifact zip central directory, which is read into memory whole (about 106 bytes per entry in CI's test results) |
| `max_rt_entries` | `10000` | Most entries in a run tarball opened for a reviewer |
| `max_rt_member_bytes` | 64 MiB | Largest member of a run tarball opened for a reviewer |
| `max_diff_bytes` | 512 MiB | Largest diff of a pull request's `tests/`. It is streamed to disk, not held in memory, and kept with the review's rematerialized run; the builder applies all of it within `rematerialize_timeout` |
| `max_sparse_patterns` | `20000` | Most sparse-checkout patterns in a review's worktree (up to two per test). Checking out takes time proportional to patterns times paths |
| `max_concurrency` | `2` | Pull requests processed at once |
| `rematerialize_timeout` | `900` | Seconds `edg-rematerialize-test-run` may run |
| `unsandboxed` | `false` | `true` runs without containers: the service serves the runner servers and the publisher server itself, on `127.0.0.1`, where any local process can reach them. **Local development on a single-user machine only.** |
| `acknowledg_api_host`, `acknowledg_api_port` | `localhost`, `2727` | AcknowlEDG API: a host name alone (no scheme, port, or path). Port 443 selects TLS (`wss://`), any other plain `ws://`, so a host other than `localhost` needs its port stated, and the `acknowledg-api-key` credential is only sent to one on 443 |
| `acknowledg_ui_host`, `acknowledg_ui_port` | `localhost`, `5173` | AcknowlEDG UI, used for links in comments; as for the API, 443 selects `https://`, and a host other than `localhost` needs its port stated |
| `publisher_processes` | `4` | Publisher worker count |
| `publisher_request_timeout` | `150` | Seconds per worker request |

| Credential | Meaning |
| --- | --- |
| `forge-token` | Forge API token (also used for git) |
| `webhook-secret` | Webhook secret; required for webhooks |
| `database-password` | PostgreSQL password; SQLite accepts no password |
| `acknowledg-api-key` | AcknowlEDG API key, if required |

### Database connection table

Omit `[database]` to use SQLite at `<home>/state.db`. To select another
SQLite file, append this table after the root settings:

```toml
[database]
backend = "sqlite"
path = "state.db"
```

An explicit relative path is relative to the TOML file's directory;
`:memory:` selects an in-memory database. SQLite accepts only `backend`
and `path`. PostgreSQL has two backends; both require `name` and
`username`, and default `port` to 5432:

- `backend = "postgresql-unix"` requires `socket_directory`, an absolute
  path such as `/run/postgresql`. Mount that directory into the service
  container (see step 6 below). Unix sockets do not use network ports, but
  PostgreSQL includes the configured `port` in the socket filename:
  `/run/postgresql/.s.PGSQL.5432` with the defaults. This selects among
  PostgreSQL instances sharing a socket directory. Peer authentication
  needs no `database-password` credential. This backend accepts neither
  `host` nor `require_tls`.
- `backend = "postgresql-tcp"` requires `host`, a host name or IP address.
  `require_tls` defaults to `true`, requiring TLS with certificate and
  hostname verification using the system trust store. Set it to `false`
  to disable TLS explicitly. This backend accepts no `socket_directory`.

The drivers are fixed to `aiosqlite` and `asyncpg` respectively.

Passwords belong only in the `database-password` credential. Arbitrary
connection URLs, query mappings, and driver argument mappings are not
configuration options; supported options must be explicit schema fields.

### GitHub

- **Token:** a fine-grained token (or a bot account's token) with read access
  to *Contents*, *Pull requests*, and *Actions* on the configured
  repositories. To comment, it also needs write access to *Pull requests*.
- **Webhook:** use content type `application/json`, the webhook secret, and
  only the *Pull requests* and *Workflow runs* events (choose *Let me select
  individual events* and clear *Pushes*, which the service ignores; a push
  to a pull request arrives as a *Pull requests* event). Point it at
  `https://<host>/webhook`, typically behind a reverse proxy to
  `--service-bind` (published on the host's `127.0.0.1`;
  proxy only `/webhook`).
- **CI:** set `ci_workflow = "build-and-test-ci.yml"`. Its
  `publish-test-results` job uploads `test-results-all`, including the
  `ci-meta.json` that records the tested (merge) commit. Benchmark results
  need a CI job uploading `bench-results` (`c/` and `cpp/` runs plus
  `ci-meta.json`), which is a separate change.

### GitLab

- **Token:** a project or group access token with the `api` scope
  (`read_api` is enough without comments).
- **Webhook:** use the secret token, with *Merge request events* and
  *Pipeline events*.
- **CI:** artifacts belong to jobs, so `test_artifact` and
  `bench_artifact` name the jobs whose artifacts archives hold
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
   # Webhooks (see step 4); remove this line when webhooks = false.
   LoadCredentialEncrypted=webhook-secret:/etc/credstore.encrypted/acknowledg-forge.webhook-secret
   #LoadCredentialEncrypted=acknowledg-api-key:/etc/credstore.encrypted/acknowledg-forge.acknowledg-api-key
   # Optional: the password for a remote PostgreSQL database (see step 6).
   #LoadCredentialEncrypted=database-password:/etc/credstore.encrypted/acknowledg-forge.database-password
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
   to only poll, skip this, remove the unit's `webhook-secret` line, and set
   `webhooks = false` in the TOML settings (step 5). Enabling webhooks
   without the secret prevents the service from starting:

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
   SETTINGS_FILE=/etc/acknowledg-forge/acknowledg-forge-github.toml
   CREDENTIALS_DIR=/run/acknowledg-forge-github/credentials
   # The webhook port on the host's 127.0.0.1, for the reverse proxy.
   WEBHOOK_PORT=2728
   ```

   And the shared TOML settings (see [Configuration](#configuration)) as
   `/etc/acknowledg-forge/acknowledg-forge-github.toml`, replacing the
   repository:

   ```toml
   home = "/var/lib/acknowledg-forge"
   tools_dir = "/edg/tools"
   repositories = ["<OWNER>/<REPOSITORY>"]
   # Your public AcknowlEDG endpoints; port 443 selects TLS.
   acknowledg_api_host = "acknowledg-api.example"
   acknowledg_api_port = 443
   acknowledg_ui_host = "acknowledg.example"
   acknowledg_ui_port = 443
   ci_workflow = "build-and-test-ci.yml"
   comments = true
   # Webhooks (with the webhook secret, step 4); set to false to only poll.
   webhooks = true
   # Optional run tarball limits (the defaults).
   max_rt_entries = 10000
   max_rt_member_bytes = 67108864
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
     ```

     Add this table after the root settings in the shared TOML:

     ```toml
     [database]
     backend = "postgresql-unix"
     socket_directory = "/run/postgresql"
     port = 5432
     name = "acknowledg_forge"
     username = "acknowledg-forge"
     ```

     No database password credential is needed for peer authentication.

   - If using a remote PostgreSQL database:

     Create a secure credential using systemd for the database password:

     ```
     systemd-ask-password -n 'Database password:' \
       | systemd-creds encrypt --name=database-password - \
           /etc/credstore.encrypted/acknowledg-forge.database-password
     ```

     Uncomment the unit's `database-password` credential line, and add the
     non-secret connection table after the root settings:

     ```toml
     [database]
     backend = "postgresql-tcp"
     host = "db.internal"
     port = 5432
     name = "acknowledg_forge"
     username = "acknowledg_forge"
     require_tls = true
     ```

     No PostgreSQL socket mount is needed for a remote database.

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
deployment's `IMAGE`. Application settings, including AcknowlEDG endpoints,
are owned by the administrator in TOML. It pulls the image and drives the
service's unit.

- **Strict arguments:** it accepts only images from
  `ghcr.io/edgcpp/acknowledg-forge` pinned by digest and a service named
  `acknowledg-forge-*`. A leaked deploy key can only deploy another
  published image; it cannot change application settings.
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

The Forge deploy no longer reads the `ACKNOWLEDG_API_HOST` or
`ACKNOWLEDG_UI_HOST` repository variables. Those may still be used by the
separate AcknowlEDG application deploy.

The host needs Docker Engine 25 or later and Compose 2.20.2 or later (for
several `--env-file`s, and the healthchecks' `start_interval`), and systemd
250 or later (for encrypted credentials). The first published
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
`unsandboxed = true`.

## Development

```
github/setup-venv.sh            # or gitlab/
tests/run-tests.sh              # pytest: both services and the shared code
typecheck.sh                    # mypy, as strict as typecheck-python.sh
```

Database tests default to in-memory SQLite. To exercise another database,
set `ACKNOWLEDG_FORGE_TEST_SETTINGS` to a Forge TOML settings file; the tests
use its `[database]` table through the application settings loader. For
example (adjust `tools_dir` to your checkout):

```toml
home = "/tmp/forge-test"
tools_dir = "/path/to/compiler/dev_tools"
repositories = ["edgcpp/compiler"]

[database]
backend = "postgresql-tcp"
host = "localhost"
name = "forge_test"
username = "forge_test"
require_tls = true # Set to false for a local plaintext test server.
```

Use a dedicated test database: its schema is dropped before and after each
test using the store fixture. If it needs a password, put it in a
`database-password` credential file and set
`ACKNOWLEDG_FORGE_TEST_CREDENTIALS_DIRECTORY` to that directory. Both schema
reset and store connections use the same database settings and credentials,
including TLS verification. The `postgresql-unix` backend works with the
same settings as the application and requires no password for peer
authentication. These test variables replace
`ACKNOWLEDG_FORGE_TEST_DATABASE_URL`.

To try the service locally against `edgcpp/compiler`:

1. Create a [fine-grained personal access
   token](https://github.com/settings/personal-access-tokens/new) with
   *Repository access* set to *Public repositories*. That is read-only
   access to public repositories, and all the script needs: it reads pull
   requests, clones, and downloads CI artifacts, but never comments. Add no
   permissions.
2. Start the AcknowlEDG server and client
   ([../acknowledg](../acknowledg/README.md)).
3. Run `./run-dev-oss-repo.sh` and enter the fine-grained token at the
   silent prompt. Alternatively, explicitly choose `./run-dev-oss-repo.sh
   --use-gh-token` to use `gh auth token`. The CLI token can have broader
   permissions than this demo needs.

The Linux Python launcher holds credentials in sealed anonymous `memfd`
files. Its private temporary credentials directory contains only symlinks
to `/proc/<launcher-pid>/fd/<fd>`, never credential bytes. The launcher stays
alive while Forge runs and forwards termination signals. After the first
signal it allows ten seconds for shutdown, then kills the child process
group and reaps the child if necessary before removing its temporary state.
Tokens are absent from child environments, arguments, logs, and generated
TOML. There is no disk-backed fallback if `memfd` or `/proc` is unavailable.
Memory can swap, as environment memory can; this does not isolate other
processes running as the same user.

The default generated TOML serves `edgcpp/compiler`, unsandboxed, without
commenting. Its state lives in a temporary directory removed on exit. To
change settings or preserve state, set `ACKNOWLEDG_FORGE_SETTINGS_FILE` to
your own TOML with a persistent `home`. To supply existing credentials, set
`CREDENTIALS_DIRECTORY` to an existing directory containing a nonempty
`forge-token`; the launcher leaves that directory untouched.

For other development configurations, run `github/run-service.sh` or
`gitlab/run-service.sh` with `ACKNOWLEDG_FORGE_SETTINGS_FILE` and
`CREDENTIALS_DIRECTORY`. Set `unsandboxed = true` in TOML when running
outside containers. Use this mode only on a single-user machine.

Database schema changes go through Alembic. Add a revision under
`edgacknowledg/forge/db/migrations/versions/` (see `migrations/env.py`).

### Adding a forge

Implement `Forge`, and optionally `ForgeWebhooks` and `ForgeCommenter`
(`edgacknowledg/forge/__init__.py`). `edgacknowledg.forge.http` handles the
REST plumbing: rate limits, pagination, and credential-free downloads. Then
add a service directory like `github/`, whose `__main__` passes a
`build(config, credentials)` to `edgacknowledg.forge.service.main`.
