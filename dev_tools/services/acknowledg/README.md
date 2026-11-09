# AcknowlEDG

AcknowlEDG is a web service that allows for easy review of EDG Run Test output
and benchmark deltas when paired with the `edgy-review`, `edg-bench-review`,
and `edg-acknowledg-cli` tools (even when working with results on one or more
remote hosts).

![Screenshot of Web Client Reviewing Results](client.png)

The service is composed of a VueJS web client (which when built reduces down to
static HTML, CSS, and JavaScript files) and a Python WebSocket Server back end.
The WebSocket protocol is documented in [PROTOCOL.md](PROTOCOL.md).

Test and benchmark review sessions can share the same review UUID. Multiple
sections of each kind are supported; each CLI `--section-name` becomes a tab
under `/review/<UUID>/tests/<slug>` or `/review/<UUID>/benchmarks/<slug>`
(slug derived by lowercasing the display name, rewriting `c++` to `cpp`,
and replacing non-alphanumeric runs with dashes). The review list still only reports type presence
(Test / Benchmark / both), not every section name. List titles default to
`"Review opened by {user}"` and can be overridden with a fully custom
`--name` (plain text; providers such as acknowledg-forge may send rich names
with links, see PROTOCOL.md). Each review entry includes a client-supplied start time (captured
when the CLI starts, so reconnects keep the original time) shown in the
viewer's local timezone.

Write-capable `/source`, `/edit`, and `/batch` routes can be protected with a
shared API key via the `EDG_ACKNOWLEDG_API_KEY` environment variable or, for
deployed servers, a systemd credential (see [Server](#server)).
`edg-acknowledg-cli` multiplexes many sections over `/batch` from a JSON
specification; `edgy-review` and `edg-bench-review` remain available as
task-focused CLIs that use the direct `/source` and `/edit` endpoints.

## Internal Deployment

The standard deployment of the software is as an internal service within an
organization. With API-key protection enabled on `/source` and `/edit`, the
server can also be hosted more publicly while still restricting who can publish
new review data.

The deployment descriptions below describe the basics of the setup and do not
enable HTTPS; modification of these configuration files for environments where
HTTPS is desired or required does not require special steps beyond adding the
HTTPS portions of the respective virtual host configuration.

### Client

Standard deployment of the client using a web server like Apache looks
something like the following (with `<HOSTNAME>` replaced by the desired
UI hostname):

```
<VirtualHost *:80>
        ServerName <HOSTNAME>
        DirectoryIndex index.html
        DocumentRoot /srv/http/<HOSTNAME>/

        <Location />
                RewriteEngine On
                # set the base URL prefix
                RewriteBase /
                # for requests for index.html, just respond with the file
                RewriteRule ^index.html$ - [L]
                # if requested path is not a valid filename, continue rewrite
                RewriteCond %{REQUEST_FILENAME} !-f
                # if requested path is not a valid directory, continue rewrite
                RewriteCond %{REQUEST_FILENAME} !-d
                # if you have continue to here, respond with index.html
                RewriteRule . /index.html [L]
        </Location>
</VirtualHost>

# Used to expose the web client files:
<Directory /srv/http/<HOSTNAME>/>
        # Enable the web server to act on this directory and its subdirectories.
        Require all granted
</Directory>
```

### Server

Standard deployment of the server is to run the Quart app (via Hypercorn) behind
a reverse proxy.  For Apache, this looks something like the following (with
`<HOSTNAME>` replaced by the desired API hostname):

```
<VirtualHost *:80>
        ServerName <HOSTNAME>

        ProxyPass / ws://localhost:2727/
</VirtualHost>
```

Hypercorn itself is best run as a systemd service. A unit along these lines
(saved as e.g. `/etc/systemd/system/acknowledg-api.service`, with `<USER>` and
`<SERVER PATH>` replaced, where `<SERVER PATH>/venv` has the server wheel
installed) keeps it running and restarts it on failure:

```
[Unit]
Description=AcknowlEDG API Server
After=network-online.target
Wants=network-online.target

[Service]
Type=exec
User=<USER>
WorkingDirectory=<SERVER PATH>/wdir
# Optional API key protecting the write routes (see below).
LoadCredentialEncrypted=api-key:/etc/credstore.encrypted/acknowledg-api.api-key
ExecStart=<SERVER PATH>/venv/bin/hypercorn --bind 127.0.0.1:2727 acknowledg_server:app
Restart=on-failure
RestartSec=5

# Review state is held in memory, so the server needs no writable paths.
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=read-only

[Install]
WantedBy=multi-user.target
```

The API key is optional: without one, the write routes accept any client, which
is only appropriate when the API host is reachable solely by trusted users.
When a key is wanted, supply it as a [systemd credential][credentials] rather
than an environment variable: systemd decrypts it at service start into a
private directory readable only by the service, and the server reads it from
`$CREDENTIALS_DIRECTORY/api-key` (falling back to `EDG_ACKNOWLEDG_API_KEY` when
no credential is present). On disk the key is encrypted with the host's key
(and TPM2, when available), so the file is useless if copied elsewhere, and
neither the deploy user nor the service user can read it. Create it once as
root (requires systemd 250 or later):

```
mkdir -p /etc/credstore.encrypted
systemd-ask-password -n 'AcknowlEDG API key:' \
  | systemd-creds encrypt --name=api-key - \
      /etc/credstore.encrypted/acknowledg-api.api-key
```

As written, `LoadCredentialEncrypted=` makes systemd refuse to start the
service if the credential file is missing, so a host meant to require a key
fails loudly rather than quietly running without authentication. To run without
a key, omit the `LoadCredentialEncrypted=` line. To rotate the key, re-run the
command above and restart the service. On hosts with systemd 247–249, use
`LoadCredential=` with a plain file instead (`root:root`, mode `0600`), which
keeps the same isolation from the service and deploy users but stores the key
unencrypted.

[credentials]: https://systemd.io/CREDENTIALS/

Enable the service with `systemctl daemon-reload` and `systemctl enable --now
acknowledg-api`. Review sessions live only in the server's memory, so
restarting the service drops any open reviews.

Locally, `server/run-server.sh` starts Hypercorn on `127.0.0.1:2727`.

### Automated Deployment

`.github/workflows/acknowledg-deploy.yml` builds and deploys both halves over
SSH on pushes to `main` that touch AcknowlEDG. The client is rsynced into the
web root (no restart needed). The server wheel is uploaded and handed to
`server/deploy-remote.sh`, which installs it and restarts the systemd unit
**only** when the wheel's contents differ from the last successful deploy
(tracked in `<server path>/deployed.fingerprint`). The last few deployed
wheels are kept under `<server path>/wheels/` for manual rollback.

The workflow reads these secrets from the `acknowledg` GitHub environment:

| Secret | Purpose |
| --- | --- |
| `ACKNOWLEDG_SSH_HOST` | Host name or IP address |
| `ACKNOWLEDG_SSH_USER` | Deploy user |
| `ACKNOWLEDG_SSH_PORT` | SSH port (optional; defaults to 22) |
| `ACKNOWLEDG_SSH_PRIVATE_KEY` | Private key for the deploy user |
| `ACKNOWLEDG_SSH_KNOWN_HOSTS` | `ssh-keyscan` output for the host |
| `ACKNOWLEDG_CLIENT_PATH` | Web root for the client files |
| `ACKNOWLEDG_SERVER_PATH` | Server directory containing `venv/` |
| `ACKNOWLEDG_SERVICE_NAME` | systemd unit running the server (e.g. `acknowledg-api`) |

It also reads the public API URL from the `ACKNOWLEDG_API_HOST` **repository
variable** (not a secret: it's public, and it's shared with the
[acknowledg-forge](../acknowledg-forge/README.md) deploy). The URL is baked
into the client as `VITE_EDG_ACKNOWLEDG_API_HOST` and must include the scheme,
e.g. `wss://acknowledg-api.example.com`.

The host must already be set up; the workflow does not manage the unit file:

- The deploy user's `authorized_keys` holds the public key, `rsync` is
  installed, and the user owns the client and server paths.
- `<server path>/venv` is a Python 3.11+ venv, and the unit's `ExecStart`
  runs Hypercorn from it (see the example unit under [Server](#server)).
- The deploy user may restart the unit without a password, e.g. in
  `/etc/sudoers.d/acknowledg`. sudo matches arguments exactly, so the unit
  name here must be spelled as in `ACKNOWLEDG_SERVICE_NAME`:

  ```
  <user> ALL=(root) NOPASSWD: /usr/bin/systemctl restart acknowledg-api
  ```
