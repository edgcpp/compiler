# AcknowlEDG WebSocket Protocol

This document describes the WebSocket protocol used by the AcknowlEDG server,
the Vue client, and the `edgy-review` / `edg-bench-review` /
`edg-acknowledg-cli` providers.
The current protocol revision is **19**.

All frames are JSON text. Implementations live in `edgacknowledg`
(`dev_tools/pylibs/edgacknowledg`) and must stay in sync with the JavaScript
`PROTOCOL_REVISION` in `client/src/sockets.js`.

## Envelope

Every message is an object:

| Field | Required | Meaning |
| --- | --- | --- |
| `t` | yes | Tag string (see [Tags](#tags)) |
| `r` | yes | Protocol revision (integer) |
| `v` | no | Tag-specific value (`null` if omitted by some senders) |
| `s` | no | Section slug |

Python `edgacknowledg.message()` always emits `v`. The JavaScript helper omits
`v` and `s` when they are not provided. Receivers treat a missing `v` as
`null`.

If `r` does not match the peer's revision, the receiver raises an incompatible
protocol error. The server replies with `outdated-proto` (still carrying the
server's current `r`).

## Endpoints

| Path | Role | Auth |
| --- | --- | --- |
| `/review` | Review list | none |
| `/review/<uuid>` | Browser reviewer for one review | none |
| `/source/<uuid>/test/<slug>` | Test source provider | API key |
| `/source/<uuid>/bench/<slug>` | Benchmark source provider | API key |
| `/edit/<uuid>/test/<slug>` | Test editor provider | API key |
| `/edit/<uuid>/bench/<slug>` | Benchmark editor provider | API key |
| `/batch` | Multiplexed providers/reviewers | API key |

`<uuid>` is the shared review id. `<slug>` selects one section of that kind.

When the server has an API key configured (a systemd `api-key` credential, or
else `EDG_ACKNOWLEDG_API_KEY`), `/source`, `/edit`, and `/batch` require
header `X-AcknowlEDG-API-Key`. The server accepts the WebSocket, then closes
with code **3000** if the key is missing or wrong. When no key is configured,
authentication is skipped (local development).

## Roles and occupancy

A review is identified by UUID. Within a review, each `(kind, slug)` pair has
at most one source and one editor:

- kind is `test` or `bench`
- slug is derived from the CLI `--section-name` by lowercasing and replacing
  each run of characters outside `[a-z0-9]` with a single dash
- a valid slug matches `^[a-z0-9]+(?:-[a-z0-9]+)*$`

Occupancy is claimed when the provider subscribe function runs (synchronously,
before the subscription generator is iterated) so a second connector for the
same UUID and slug is rejected immediately with `role-occupied` (`v` empty).
CLI tools do not reconnect that provider socket; any other still-open
source/edit socket for the process keeps running.

The review is removed from the list when no reviewers, sources, or editors
remain.

## Optional provider identity messages

Source and edit subscriptions start the same way: occupancy is claimed and
work can flow immediately. Providers may optionally send identity updates at
any time (on dedicated sockets or nested in `batch.event`):

```json
{ "t": "meta.section.name", "r": 19, "v": "<tab label>" }
```

```json
{ "t": "meta.review.name", "r": 19, "v": [ <rich name segments> ] }
```

```json
{ "t": "meta.review.timestamp", "r": 19, "v": "<ISO-8601 datetime>" }
```

- `meta.section.name` sets the display name for the section tab (shown in
  `review.connection-state` for sources)
- `meta.review.name` sets the review's name, a [rich name](#rich-names), if
  the review does not already have one
- `meta.review.timestamp` sets `created_at` if the review does not already have
  one; it must be a parseable ISO-8601 datetime (a trailing `Z` is accepted)

The first accepted `meta.review.name` / `meta.review.timestamp` wins; later
messages are ignored once those fields are set.

### Rich names

A review's name is rich text: an array of segments, shown in order.

```json
[
  { "type": "link", "url": "https://github.com/edgcpp/compiler/pull/329",
    "text": "edgcpp/compiler#329" },
  { "type": "text", "text": " CTAD through a template template parameter" }
]
```

| Segment | Fields |
| --- | --- |
| `text` | `text` |
| `link` | `url`, and optionally `text` (shown instead of the URL) |

Every field is a non-empty string, and no other fields are allowed. A link's
`url` must be `http` or `https`; reviewers' browsers open it in a new tab. A
name has at least one and at most 32 segments, and at most 4096 characters of
text and URLs in all. The server answers any other `meta.review.name` with
`invalid` and leaves the name unset; the browser client also renders a link
with any other URL as text.

## Section field `s`

The browser keeps **one** WebSocket to `/review/<uuid>`. Each reviewer request
that targets a source or editor sets `s` to the focused section slug.

The server ignores any `s` on provider replies and instead stamps `s` from the
provider socket URL (or the batch subscription's bound slug), so a CLI cannot
spoof which section a reply belongs to.

## Batch endpoint (`/batch`)

One authenticated WebSocket multiplexes many logical subscriptions. Each
subscription behaves like a dedicated `/source`, `/edit`, or `/review/<uuid>`
session, sharing the same occupancy rules.

### `batch.subscribe` (client → server, echoed server → client)

Client request `v`:

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | Review UUID |
| `type` | yes | `test.src`, `test.edit`, `bench.src`, `bench.edit`, or `review` |
| `slug` | providers | Section slug (validated like dedicated routes) |

The server replies with the same tag and a client-oriented echo of `v` that
includes `subscription_id` (opaque UUID for this logical session). There is no
separate `*-ok` tag. Optional `meta.section.name` / `meta.review.name` /
`meta.review.timestamp` messages are sent as ordinary nested `batch.event`
frames after subscribe.

### `batch.unsubscribe` (client → server, echoed server → client)

`v` is `{ "subscription_id": "<uuid>" }`. The server cancels that subscription
and echoes the same tag/value.

### `batch.event` (both directions)

Wraps a normal protocol message for one subscription:

```json
{
  "t": "batch.event",
  "r": 17,
  "v": {
    "subscription_id": "<uuid>",
    "message": { "t": "<inner tag>", "v": ..., "s": "..." }
  }
}
```

Protocol revision `r` is only on the outer envelope. Receivers inject the
current revision into the nested `message` before decoding it as a normal
frame.

Per-subscription outcomes that need routing context — including
`role-occupied` — are delivered as nested messages inside `batch.event`, not
as top-level frames on the batch socket.

Closing `/batch` cancels every subscription for that connection.

## Control tags

| Tag | `v` | When |
| --- | --- | --- |
| `invalid` | unused / `null` | Malformed request, bad slug, failed handshake, or unexpected tag |
| `outdated-proto` | `null` | Peer `r` does not match |
| `role-occupied` | `null` | Another provider already holds this UUID + slug |
| `illegal-access` | see [Illegal access](#illegal-access) | A reviewer requested a file outside the section |
| `test.missing-src` | `null` | Reviewer request needs a test source that is not connected |
| `test.missing-edit` | `null` | Reviewer request needs a test editor that is not connected |
| `bench.missing-src` | `null` | Reviewer request needs a bench source that is not connected |
| `bench.missing-edit` | `null` | Reviewer request needs a bench editor that is not connected |

## Review list (`/review`)

On connect the server sends a full snapshot, then incremental patches.

### `list-reviews`

`v` is an array of review entries:

```json
{
  "id": "<uuid>",
  "name": [ <rich name segments> ],
  "created_at": "<ISO-8601 or null>",
  "sockets": {
    "reviewer": 0,
    "test_editor": 0,
    "bench_editor": 0,
    "test_source": 0,
    "bench_source": 0
  }
}
```

`name` is the review's [rich name](#rich-names), or its id as a single text
segment until a `meta.review.name` sets one. `sockets` counts are `0` or `1`
for providers (occupied or not) and the current reviewer connection count for
`reviewer`. The list reports type presence, not every section name.

### `list-reviews-patch`

`v` is an object with optional keys:

- `upsert`: array of review entries (same shape as above)
- `remove`: array of review UUIDs

## Reviewer (`/review/<uuid>`)

On connect the server assigns an internal `reviewer_id` (not sent to the
browser) and sends `review.connection-state`. Further messages are either
connection-state updates, forwarded provider replies, or control tags.

### `review.connection-state`

```json
{
  "name": [ <rich name segments> ],
  "sources": [
    { "kind": "test" | "bench", "slug": "<slug>", "name": "<section_name>" }
  ],
  "editors": [
    { "kind": "test" | "bench", "slug": "<slug>" }
  ]
}
```

`name` is the review's name, as in the review list. Only occupied slots are
listed.

### Reviewer requests

The browser sends a tagged request with `s` set to the focused slug. Request
`v` is validated (`deny_unknown_fields` where applicable). Empty payloads may
be omitted, `null`, or `{}`.

The server forwards a corresponding message to the matching source or editor,
adding `reviewer_id` so the reply can be routed back to this reviewer.

| Browser tag | Browser `v` | Forwarded to |
| --- | --- | --- |
| `test.src.list-statuses` | empty | test source |
| `test.src.show-diff` | string test id | test source |
| `test.src.show-diff-curr` | `{ "id", "configs": [string] }` | test source |
| `test.src.show-output` | string test id | test source |
| `test.src.list-assoc-files` | string test id | test source |
| `test.src.show-src` | `{ "id", "assoc_file": string \| null }` | test source |
| `test.edit.update-diff` | `{ "id", "configs": [string] }` | test editor |
| `bench.src.list-statuses` | empty | bench source |
| `bench.src.show-annotate` | string bench id | bench source |
| `bench.edit.update-baseline` | string bench id | bench editor |

String ids that are modeled as transparent types on the wire are JSON strings,
not `{ "value": "..." }` objects.

### Provider-bound requests

Messages the server sends to a source or editor always include
`reviewer_id`. Other fields match the reviewer request (plus `reviewer_id`):

```json
{ "t": "<same tag>", "r": 17, "v": { "reviewer_id": "<uuid>", ... } }
```

## Provider replies

The CLI replies with the **same tag**. The server unwraps a source reply's
`client_response` (or an editor reply's `id` / `success` / optional `configs`)
and publishes that value to the requesting reviewer with `s` set to the
provider slug.

### Source reply envelope (CLI → server)

```json
{
  "t": "<request tag>",
  "r": 16,
  "v": {
    "reviewer_id": "<uuid>",
    "client_response": { }
  }
}
```

### Editor reply envelope (CLI → server)

```json
{
  "t": "test.edit.update-diff" | "bench.edit.update-baseline",
  "r": 16,
  "v": {
    "reviewer_id": "<uuid>",
    "id": "<id>",
    "success": true,
    "configs": ["..."]
  }
}
```

`configs` is present only for test recording updates.

### Illegal access

When a reviewer's request names a file outside the section's tree (or, for
content that isn't the provider user's own, one reached through a symlink),
the provider doesn't answer the request. It reports the reviewer instead:

```json
{ "t": "illegal-access", "r": 19, "v": { "reviewer_id": "<uuid>" } }
```

The server then closes that reviewer's socket with code **3001**, without
any message: the reviewer UI never sends such requests, and doesn't reconnect
by itself. For a reviewer in a `/batch` `review` subscription that is the
whole `/batch` socket, ending every subscription on it. A provider can only
report reviewers of its own review.

### Source `client_response` shapes (reviewer `v`)

Display payloads that carry file text use `content` (string or `null` if
missing) and may set `truncated: true` when the body exceeds 8 MiB
(`MAX_DISPLAY_CONTENT_SIZE`). Protocol messages themselves are capped at
10 MiB (`MAX_MESSAGE_SIZE`).

**`test.src.list-statuses`**

```json
{
  "configs": ["<config name>", "..."],
  "statuses": [
    {
      "name": "Changes List" | "Regressions List" | "Improvements List",
      "statuses": {
        "<test id>": {
          "groups": [
            {
              "diff_hash": "<hex>",
              "configs": ["<config>", "..."],
              "lines": [
                { "qualifier": "...", "variant": "...", "status": "..." }
              ]
            }
          ],
          "remark": "<optional>"
        }
      }
    }
  ]
}
```

**`test.src.show-diff`**, **`test.src.show-output`**

```json
{ "id": "<configured test id>", "content": "<text or null>", "truncated": false }
```

**`test.src.show-diff-curr`** — same as above, plus optional
`divergent_configs`: an array of config names whose live curr-diff does not
match the representative.

**`test.src.list-assoc-files`**

```json
{ "id": "<test id>", "files": ["relative/posix/path", "..."] }
```

**`test.src.show-src`**

```json
{
  "id": "<test id>",
  "assoc_file": "<path or null>",
  "content": "<text or null>",
  "truncated": false
}
```

**`bench.src.list-statuses`**

```json
{
  "summary": {
    "baseline": "<tag>",
    "baseline_ops": 0,
    "run_ops": 0
  },
  "benchmarks": [
    { "id": "<relative path>", "baseline_ops": 0, "run_ops": 0 }
  ]
}
```

**`bench.src.show-annotate`**

```json
{ "id": "<bench id>", "content": "<text or null>", "truncated": false }
```

### Editor `v` as seen by the reviewer

**`test.edit.update-diff`**

```json
{ "id": "<test id>", "success": true, "configs": ["<config>", "..."] }
```

**`bench.edit.update-baseline`**

```json
{ "id": "<bench id>", "success": true }
```

## Tag index

| Tag | Direction |
| --- | --- |
| `invalid` | server → any |
| `outdated-proto` | server → any |
| `role-occupied` | server → source/edit (or nested in `batch.event`) |
| `illegal-access` | source/edit → server |
| `meta.section.name` | provider → server (optional tab label) |
| `meta.review.name` | provider → server (optional review [rich name](#rich-names)) |
| `meta.review.timestamp` | provider → server (optional created_at) |
| `list-reviews` | server → list client |
| `list-reviews-patch` | server → list client |
| `review.connection-state` | server → reviewer |
| `test.missing-src` | server → reviewer |
| `test.missing-edit` | server → reviewer |
| `bench.missing-src` | server → reviewer |
| `bench.missing-edit` | server → reviewer |
| `batch.subscribe` | client ↔ batch |
| `batch.unsubscribe` | client ↔ batch |
| `batch.event` | client ↔ batch |
| `test.src.list-statuses` | reviewer ↔ test source |
| `test.src.list-assoc-files` | reviewer ↔ test source |
| `test.src.show-diff` | reviewer ↔ test source |
| `test.src.show-diff-curr` | reviewer ↔ test source |
| `test.src.show-output` | reviewer ↔ test source |
| `test.src.show-src` | reviewer ↔ test source |
| `test.edit.update-diff` | reviewer ↔ test editor |
| `bench.src.list-statuses` | reviewer ↔ bench source |
| `bench.src.show-annotate` | reviewer ↔ bench source |
| `bench.edit.update-baseline` | reviewer ↔ bench editor |

Namespaces:

- `test.src.*` — test source
- `bench.src.*` — benchmark source
- `test.edit.*` — test editor
- `bench.edit.*` — benchmark editor
- `batch.*` — multiplexed `/batch` control and events
