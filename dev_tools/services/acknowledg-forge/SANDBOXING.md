# Sandboxing in acknowledg-forge

acknowledg-forge builds review sections out of pull requests and the CI
artifacts their runs produce, and serves them to reviewers. All of that
content is untrusted: anyone who can open a pull request controls it. This
document describes how the service keeps that content away from what it
protects, and what a host must provide for it to work.

## What is protected

The service holds things a pull request must never reach:

- the forge token (which can comment on, and read, every served repository)
  and the webhook secret;
- the database;
- the clones, and every other pull request's review trees;
- the host itself, and the service's memory and disk.

The AcknowlEDG API key is the one secret the process serving reviewers
holds; it only lets that process publish review sections.

## Layers

No single mechanism is relied on. In order, from outermost:

1. **The containers.** A deployment is four Docker containers
   ([`compose.yaml`](compose.yaml)), one per role, each seeing only the
   files and networks its role needs. Everything that reads pull request
   content runs outside the service's container, away from its secrets.
2. **What enters the service.** The EDG tooling comes from a frozen,
   reviewed copy (`ACKNOWLEDG_FORGE_TOOLS_DIR`; the image embeds one), never
   from a pull request. A pull request's diff is only ever input to `patch`.
   Checkouts and artifacts are checked before anything reads them (see
   [Symlinks and links](#symlinks-and-links)).
3. **Narrow interfaces.** The containers reach each other only over
   WebSockets, each pair on a network of its own that no other container
   shares. The runners run only the commands they allow, on paths under
   their mounts, with a cleared environment.
4. **Confined reads.** The code answering reviewers opens files without
   following symlinks, only under the section's tree, and only up to a size
   limit.
5. **Resource limits.** Timeouts, size caps, bounded `/tmp`s, and each
   container's memory and process limits.

## Processes

```mermaid
flowchart TB
  forge[(Forge API)]
  acknowledg[(AcknowlEDG API)]
  proxy[Reverse proxy<br/>on the host]

  subgraph service [Container: service]
    poller[Poller:<br/>every poll interval]
    webhooks[Webhook receiver:<br/>optional]
    ingestor[Ingestor]
    processor[Processor:<br/>review management loop]
    announcer[Announcer:<br/>optional comments]
    publisherclient[publisher client:<br/>reconnect loop]
    builderclient[builder's runner client]
    secrets[/forge token,<br/>webhook secret/]
    db[(database)]
    clones[(clones)]
  end

  subgraph builder [Container: builder]
    buildrunner[runner server:<br/>builder role] --> rematerialize[edg-rematerialize-test-run,<br/>patch]
  end

  subgraph publisher [Container: publisher]
    publisherserver[publisher server:<br/>one publisher, for every connection] -->|each request, to the<br/>next free process| poolprocess[pool processes:<br/>the stock handlers]
    poolprocess --> toolsclient[tools runner client]
  end

  subgraph tools [Container: tools]
    toolsrunner[runner server:<br/>tools role] --> commands[ifc-dump, cg_annotate]
  end

  reviews[(reviews/:<br/>worktrees and<br/>run directories)]
  staging[(build-staging/)]

  proxy -->|webhook deliveries,<br/>127.0.0.1 only| webhooks
  poller -->|pull requests, CI runs| ingestor
  webhooks -->|what changed| ingestor
  ingestor -->|asks for the actual state| forge
  ingestor -->|records, marks dirty| db
  ingestor -->|wakes| processor
  processor -->|fetches into| clones
  processor -->|fetches pull requests,<br/>downloads artifacts| forge
  processor -.->|checks out worktrees, symlinks as plain files;<br/>extracts CI artifacts| reviews
  processor -->|builds| builderclient
  builderclient -->|network: build<br/>one connection, a request per section| buildrunner
  rematerialize -.->|reads, read-only| reviews
  rematerialize -.->|writes the rebuilt run| staging
  processor -.->|checks the run,<br/>renames it into place| staging
  processor -->|review changed| publisherclient
  processor -->|after each sweep| announcer
  announcer -->|comments with section links| forge
  publisherclient <-->|network: publish| publisherserver
  publisherserver <-->|network: egress<br/>WebSocket, API key| acknowledg
  poolprocess -.->|reads sections for reviewers,<br/>read-only| reviews
  toolsclient -->|network: tools<br/>a connection per pool process,<br/>a request per reviewer command| toolsrunner
  commands -.->|reads one section's tree,<br/>read-only| reviews
```

The service container is the only one holding the forge token, the webhook
secret, the database, and the clones. The service never reads pull request
content beyond checking its shape; reading it is left to the other
containers. The processor's loop is described under
[Managing reviews](#managing-reviews), and the publisher's under
[The publisher](#the-publisher).

### Names

Every connection between containers is a server, in one container, and its
client, in another, and the code is named that way: each container's name
in `compose.yaml` is the package it runs (`python -m
edgacknowledg.forge.<container>`), what listens in it is a `…Server`
(`server.py`), and what reaches it is a `…Client` (`client.py`).

| Container | Runs | Its server | Its client, in |
| --- | --- | --- | --- |
| `service` | `acknowledg-forge-github` (`edgacknowledg.forge.service`, `processor`, …) | (webhooks only) | — |
| `builder` | `edgacknowledg.forge.builder` | the runner server (`create_runner` in `runner.server`) with the builder role (`runner.roles`) | `RunnerClient` (`runner.client`), in the service, through `Sandbox.run` |
| `publisher` | `edgacknowledg.forge.publisher` | the publisher server (`create_publisher` in `publisher.server`), publishing what every connection queues (`publish` in `publisher.acknowledg`), and the `ProcessPool` (`publisher.pool`) of pool processes (`publisher.pool_process`) answering its reviewers | `PublisherClient` (`publisher.client`), in the service |
| `tools` | `edgacknowledg.forge.tools` | the runner server with the tools role | `RunnerClient`, in each pool process, through `Sandbox.context_for` |

The builder and tools containers run the same server, the runner, with a
role each. `publisher.protocol` is the publisher's messages; `publisher`
itself is the `Publisher` interface the processor publishes through. Without
containers, `local` serves all three servers from the service's process.

## What each container sees

All four containers run from the same image and get:

- the host account that owns the deployment's state as their user (never
  root), no capabilities, and `no-new-privileges`;
- a read-only root file system, with the frozen tools under `/edg/tools`;
- a 512 MiB tmpfs `/tmp`;
- an init process that reaps whatever their commands leave behind, and
  memory and process-count limits;
- Docker's default seccomp profile (and AppArmor profile, on hosts using
  AppArmor), which, among other things, stops them from creating user
  namespaces.

Each mounts only its part of the deployment's state directory, at the same
path in every container (`/var/lib/acknowledg-forge`), so paths in messages
need no translation.

| Container | Runs | Sees | Writes | Networks | Holds |
| --- | --- | --- | --- | --- | --- |
| `service` | the service (`acknowledg-forge-github` or `-gitlab`) | the whole state directory | the whole state directory | `frontend` (the forge; webhooks from the host), `build`, `publish` | the credentials |
| `builder` | a runner server (builder role): `edg-rematerialize-test-run` only | `reviews/` | `build-staging/` only | `build` (internal) | nothing |
| `publisher` | the publisher server: `publish`, and pool processes running the stock handlers | `reviews/` | nothing | `publish` (internal), `tools` (internal), `egress` (AcknowlEDG) | the API key, in the server's memory |
| `tools` | a runner server (tools role): `ifc-dump` and `cg_annotate` only | `reviews/` | nothing | `tools` (internal) | nothing |

- **Networks.** Each pair of containers that talk shares a network of its
  own, and no other: the builder can't reach the publisher or the tools
  runner, nor they it. Internal networks have no route out, so the builder
  and tools runner can reach nothing but their one client. A listener on a
  container with several networks binds only on its pair's (the publisher
  server on `publish`; the service's web app (health checks and webhooks),
  with `ACKNOWLEDG_FORGE_SERVICE_BIND=service-frontend:2728`, on
  `frontend`).
  That is what keeps each server to its one client: the connections carry
  no credentials of their own, so `tests/test_containers.py` checks every
  pairing, and a change to the networks in `compose.yaml` that breaks one
  fails it.
- **Published ports.** Only the webhook port, and only on the host's
  `127.0.0.1`, for a reverse proxy there to expose. Docker's published
  ports bypass the host's firewall, so the address stays the loopback one.

### Runners

The builder and tools containers each run a runner server
(`runner.server`), with their role's allowed commands, roots, and variables
(`runner.roles`), which are defined in the code, so they are the same with
or without containers. Each client keeps one connection open, and sends
each command as a request on it, with an id that its output and answer
carry, so commands run concurrently over it. For each, the runner:

- runs it only if it starts with an allowed prefix (e.g. `python3
  /edg/tools/bin/edg-rematerialize-test-run`), with the program resolved on
  the runner's own `PATH`;
- refuses any absolute path, in the arguments, the working directory, or a
  variable, outside its roots (its mounts), and any variable it doesn't
  allow (only `EDG_TEST_HOME`, for the builder);
- runs it with a cleared environment (`PATH`, `HOME=/tmp`, `LC_ALL`,
  `PYTHONPATH` for the frozen tools), in a session of its own, and kills
  its process group when it ends, times out (the builder: the service's
  `ACKNOWLEDG_FORGE_REMATERIALIZE_TIMEOUT`; tools: 2 min), its client
  cancels it, or the connection goes (which kills every command on it);
- once no command is running, kills every other process left in the
  container and empties `/tmp`.

The service has one runner client, for its builds. Each pool process has
one too, for the commands its handlers run: they run every command through
their `SandboxContext` (`run_command`), whose command runner is that
client. A client keeps one connection to its runner, which all its
commands share (each request carries an id its answer and output come back
with), and reconnects whenever it goes, backing off while the runner can't
be reached; a command waits for the connection, within its time limit, and
the connection going fails the commands in flight on it.

### Trade-offs

Two deliberate choices, and what limits them:

- **The containers are long-lived.** The builder and tools runners serve
  every pull request, not one each. A pull request that compromises a
  command could leave something behind for later ones, or interfere with
  one running at the same time. Against that: between commands the runner
  kills every stray process and empties `/tmp`; the root file system is
  read-only; the builder can write only `build-staging/`, which the service
  checks before using anything in it; and neither runner has a network to
  send anything out on.
- **Docker runs as root.** The containers don't (see above), but an escape
  through the kernel or the container runtime would reach the host as root,
  where a rootless setup would reach only an unprivileged account. That is
  what the containers' limits (no capabilities, `no-new-privileges`,
  seccomp, AppArmor, read-only root file systems) guard against.

## Managing reviews

The processor (`Processor.run`, in the service) decides what each review
serves: it builds sections from pull requests through the builder, and
hands them to the publisher to serve.

### Review management loop

```mermaid
flowchart TB
  start([start]) --> connected1[wait until the publisher client<br/>has served the stored sections]
  connected1 --> restore[restore: drop builds<br/>the restart interrupted]
  restore --> connected2[wait until connected]
  connected2 --> due[start processing every dirty pull request,<br/>a few at a time]
  due --> maintain{6 h since the<br/>last maintenance?}
  maintain -->|yes| gc[git housekeeping] --> sleep
  maintain -->|no| sleep[wait to be woken,<br/>or 60 s]
  sleep --> connected2

  due -.-> process
  subgraph process [Processing one pull request]
    open{open, against a<br/>served branch?}
    open -->|no| teardown[stop serving its review,<br/>delete its worktrees]
    open -->|yes| fetchbuild[fetch it, build its sections]
    fetchbuild --> sync[sync the sections]
    sync -->|ok| record[record the sections,<br/>delete superseded runs]
    sync -->|unavailable| defer[stays dirty:<br/>retried once connected]
    fetchbuild -->|error| retry[retried with backoff:<br/>1 min up to 1 h,<br/>at most 8 times]
    sync -->|error| retry
  end
```

### Building a section

The "fetch it, build its sections" and "sync the sections" steps of that
loop, for one section:

```mermaid
sequenceDiagram
  autonumber
  participant F as Forge
  participant S as Service
  participant B as Builder
  participant P as Publisher

  S->>F: fetch the pull request
  Note over S: fsck what was fetched
  Note over S: check out its merge-base,<br/>symlinks as plain files
  Note over S: check_plain_tree(worktree)
  S->>F: download the CI artifact (size-capped)
  Note over S: extract it: no links, no escaping paths,<br/>inner tarballs size-capped
  S->>B: edg-rematerialize-test-run diff
  Note over B: worktree read-only,<br/>build-staging writable,<br/>time-limited
  B-->>S: output, exit status
  Note over S: check_plain_tree(staging),<br/>no hard links either
  Note over S: rename staging into the worktree
  Note over S: record the sections,<br/>delete superseded run directories
  S->>P: sync(review, sections)
  P-->>S: ok
```

A failed check fails the section rather than serving it. Once the new
sections are recorded, nothing needs the superseded run directories, so
they are deleted before the publisher is told; until it catches up, a
reviewer's request for one gets the handler's failure reply. Staging
directories a restart interrupted are deleted at startup.

## Serving reviewers

Reviewers' requests reach the publisher through AcknowlEDG. Most are
answered by the publisher itself; those needing a tool go to the tools
runner.

### Test source (answered by the publisher)

```mermaid
sequenceDiagram
  participant R as Reviewer
  participant A as AcknowlEDG
  participant W as Publisher

  R->>A: show a test's source
  A->>W: request
  Note over W: ConfinedPath.open: no symlinks,<br/>under the section, at most 64 MiB
  Note over W: read only as much<br/>as can be displayed
  W-->>A: the contents
  A-->>R: response
```

### IFC dump (delegated to the tools runner)

```mermaid
sequenceDiagram
  participant R as Reviewer
  participant A as AcknowlEDG
  participant W as Publisher
  participant T as Tools runner

  R->>A: show a test's IFC dump
  A->>W: request
  Note over W: ConfinedPath: the file<br/>is under the section
  W->>T: ifc-dump path
  Note over T: allowed command, path under reviews/,<br/>read-only, 2 min
  T-->>W: output
  W-->>A: the output
  A-->>R: response
```

Bench sections' annotated profiles (`cg_annotate`) take the same path.

Sections are read-only: reviewers can't accept diffs or update baselines
through the service.

## The publisher

The publisher container runs the publisher server (`create_publisher`),
which serves reviewers with `publish`, acknowledg-forge's AcknowlEDG
client. It holds one batch session with AcknowlEDG (the client library's
`BatchClient`, which keeps it connected) and manages each review's
sections; the stock providers from `edgacknowledg.client`
(`TestSourceHandler`, `BenchReviewSession`), the same handlers that
`edgy-review` and `edg-bench-review` use, answer reviewers' requests in its
process pool. The service drives it through the publisher client
(`PublisherClient`), which implements the same `Publisher` interface, so
the rest of the service doesn't know the publisher is elsewhere.

Connections only queue what their client asks to publish (`Wanted`, in
the order asked), and answer; one publishing task (`publish`,
with the settings a client sends first) works through the queue, whether
or not AcknowlEDG is reachable just then. What it serves is what the
service's database stores: the processor stores a review's sections, then
tells the publisher client the review changed, and the client sends what is
stored; on every connection (the publisher may have restarted), it sends
every stored review.

### The process pool

Answering a reviewer reads, parses, diffs, and decompresses pull request
content, which can take long, or forever. So the server only manages
sections, and answers every request in its process pool (`ProcessPool`),
which its event loop never waits on for more than a request's deadline:

- Each section is registered with a `UnixSocketHandler`, which stands in
  for the section's handler: it passes each request, with the section it
  is for, to the pool, and relays the replies.
- The pool keeps a fixed number of pool processes (`--processes`, 4), each
  with one end of a socket pair as its channel. Requests queue, in order;
  each process has a dispatcher that gives it the next one only once it
  has answered the last, so a process answers one request at a time,
  building the section's handler for it and dropping it afterwards, and a
  slow request holds up only its own process. Each request carries an id
  its answer carries back; a request abandoned before its turn (as when
  its section is removed) is skipped, and one abandoned while answered
  still keeps its process busy until then, its answer dropped.
- A request has a deadline (`--request-timeout`, 150 s, longer than the
  tools runner's 2 min), counted from when its process is given it. A
  process that misses it is killed, with whatever it started, and
  replaced; so is one that exits. Either way that request fails, and the
  client library answers the reviewer with its failure reply; a request
  for a file outside the section still gets the reviewer dropped.
- A process that exits before it is ready has failed to start: the next
  try waits, doubling from 1 s up to 60 s; meanwhile the other processes
  take the requests, and while none can, requests fail at once. The server doesn't start unless every process does.

The client library answers one request at a time per section, so one
section's requests occupy at most one process at a time.

### Reconnect loop

The publisher client keeps one connection up, and on each one has the
publisher serve exactly what the service's database stores as served:

```mermaid
flowchart TB
  spawn[connect to the publisher server]
  spawn --> init[send the settings,<br/>with the API key]
  init --> serve[sync every open pull request's<br/>stored sections; stop serving<br/>those it can't, to be rebuilt]
  serve --> prune[prune: stop serving<br/>every other review]
  prune --> ready[accept requests]
  ready -->|request unanswered<br/>for 15 min| kill[drop the connection;<br/>send it nothing more]
  kill --> exited
  ready -->|connection closed| exited[fail the request in flight<br/>as unavailable]
  exited --> backoff[wait 1 s, doubling<br/>up to 60 s; back to 1 s<br/>after a long run]
  backoff --> spawn
```

At startup, the service checks that it can reach the builder
(`Sandbox.check`), and refuses to start otherwise. Nothing checks the tools
runner up front: only requests that run a command (`ifc-dump`,
`cg_annotate`) need it, so while it can't be reached, those fail, each with
the client library's failure reply, and everything else is still served.

Without containers (`ACKNOWLEDG_FORGE_UNSANDBOXED=1`, for local
development only), all of this is the same: the service runs the publisher
server and both runner servers itself (see [Development](#development))
and reaches them the same way.

### Messages

The client and the server exchange JSON objects, one per WebSocket text
message (`publisher.protocol`).

#### Requests

After the settings, the service serves every review its database stores as
served, and has the publisher stop serving every other one; then it sends
a request each time a review's stored sections change, and waits for its
answer:

```mermaid
sequenceDiagram
  participant S as Service<br/>(publisher client)
  participant P as Publisher server

  S->>P: settings, including the AcknowlEDG API key
  loop every review it serves
    S->>P: {"op": "sync", "review": ..., "sections": [...]}
    P-->>S: {"ok": true}
  end
  S->>P: {"op": "prune", "keep": [review ids]}
  P-->>S: {"ok": true}
  loop each time a review's sections change, or it is torn down
    alt its stored sections changed
      S->>P: {"op": "sync", "review": ..., "sections": [...]}
    else torn down (its files about to go)
      S->>P: {"op": "delist", "review_id": ...}
    end
    alt queued
      P-->>S: {"ok": true}
    else failed
      P-->>S: {"error": "failed", "message": ...}
    end
  end
```

- The API key goes over the connection, not a command line or
  environment, where `/proc` would show it, nor a file in the publisher
  container.
- Requests go one at a time, so each answer belongs to the request in
  flight. `failed` fails the pull request; if the publisher server can't
  be reached, the pull request is deferred until it can.
- The server queues a request whether or not it is connected to
  AcknowlEDG: the client library serves what it publishes once connected
  (again). Sections it no longer serves, it withdraws as soon as the
  publishing task reaches the update.
- The service's database leads: the processor stores a review's sections,
  deletes the trees of the sections they replace (nothing needs them
  then), and only then tells the publisher; should the publisher not take
  it, the pull request is processed again, and the next connection serves
  what is stored anyway. Until the publisher catches up, a reviewer's
  request for a deleted tree is answered with the handler's failure
  reply. Tearing a review down goes the other way: the publisher is told
  to stop serving it (`delist`) while it is still stored, then its files
  go, and only then what is stored.
- What the publisher serves outlasts the connection, so on connecting,
  after serving every review it stores, the service has it stop serving
  every other one (`prune`): nothing an earlier service served, and this
  one doesn't, stays published.

## Symlinks and links

A pull request can commit symlinks, and the builder's command could create
them. Several layers keep the service from reading through one:

- Clones check symlinks out as plain files holding the link's target
  (`core.symlinks=false`), so no worktree contains one. Symlinked tests
  therefore show their target path rather than its contents.
- Clones fsck everything they fetch (`transfer.fsckObjects`), refusing
  malformed trees.
- After every checkout, a worktree may hold only directories and regular
  files (`check_plain_tree`). So may what `edg-rematerialize-test-run`
  writes, which also may not hold hard links.
- The section's directories, including the bench baselines a pull request
  supplies, must be real directories.
- CI artifacts may not contain links or escaping paths, nor may the
  `.rt.tar.gz` files inside them. Reading those tarballs later refuses link
  members as well.
- The handlers open files through `ConfinedPath`, which opens each path
  component with `O_NOFOLLOW` relative to the last, so a symlink anywhere
  on the way is refused, even one created after a check. A refused path
  ends the reviewer's connection.
- The service refuses to write through symlinks.

## Resource limits

| Resource | Limit |
| --- | --- |
| Artifact download | `ACKNOWLEDG_FORGE_MAX_ARTIFACT_BYTES` (256 MiB), also counting the decompressed size of inner tarballs |
| A file read for a reviewer | 64 MiB; a larger one fails the request |
| A source shown to a reviewer | only as much as can be displayed is read |
| Each container's `/tmp` | 512 MiB tmpfs; copies made for diffing are streamed there |
| Build commands | `ACKNOWLEDG_FORGE_REMATERIALIZE_TIMEOUT` (15 min) |
| Reviewer commands | 2 min |
| A reviewer's request | 150 s in a pool process, which is then replaced |
| Each container | `mem_limit` (the service, builder, and publisher 2 GiB, tools 1 GiB) and 512 processes ([`compose.yaml`](compose.yaml)) |

## Secrets

Secrets belong in systemd credentials, which the host's unit decrypts and
[`prepare-runtime.sh`](prepare-runtime.sh) hands to the service container
only, in memory (`/run`), never on disk unencrypted. A secret passed in an
environment variable is readable from the process's own
`/proc/self/environ`, so the service warns at startup when sandboxing is on
and a secret came from the environment.

Comments the service posts contain only fixed text, commit hashes, and
links it generates.

## Host requirements

Docker Engine with Compose 2.17 or later, run as usual (the daemon as root;
see the trade-off above), and systemd, which starts the deployment and
holds its credentials (see [README.md](README.md#host-setup-once)). Nothing
else: unlike a sandbox nested in a container, the containers need no
unprivileged user namespaces, so hosts that restrict them (Ubuntu 24.04 and
later, with AppArmor) need no changes, and no AppArmor profile.

## Development

`ACKNOWLEDG_FORGE_UNSANDBOXED=1` runs without containers, for local
development only. The control flow stays the same: the service serves the
builder and tools runner servers and the publisher server itself
(`edgacknowledg.forge.local`), on `127.0.0.1`, and uses them as it uses the
containers, so they allow the same commands and paths. Only the
containers' confinement is missing: the publisher server runs in the
service's process, and its pool processes and the commands, though separate
processes with cleared environments, see and can reach everything the
service can. And with no
networks to keep the servers to their one client, any process on the
machine can reach them, so use it on a single-user machine only. A tool
that isn't installed (e.g. `cg_annotate`) is refused when asked for, rather
than stopping the service.

To run the containers locally, build the image (`build-edg-env
acknowledg-forge`); `tests/test_containers.py` brings a deployment up with
it and checks the table above from inside each container:

```
ACKNOWLEDG_FORGE_TEST_IMAGE=ghcr.io/edgcpp/acknowledg-forge:latest \
  tests/run-tests.sh test_containers.py
```
