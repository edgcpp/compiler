---
name: acknowledg-client
description: >-
  Works on the AcknowlEDG Python client: BatchClient vs DirectClient,
  ReviewClient Protocol, resilient websocket reconnect, and test/bench
  provider handlers. Use when editing edgacknowledg.client, edgy-review,
  edg-bench-review, edg-acknowledg-cli, or section subscribe/reconnect logic.
---

# AcknowlEDG client

Canonical package: [`dev_tools/pylibs/edgacknowledg/`](../../../dev_tools/pylibs/edgacknowledg/).
`dev_tools/services/acknowledg/server/edgacknowledg` is a **symlink** — edit
pylibs only.

Wire protocol (tags, routes, auth, occupancy):
[PROTOCOL.md](../../../dev_tools/services/acknowledg/PROTOCOL.md). Keep
`PROTOCOL_REVISION` in sync with
`dev_tools/services/acknowledg/client/src/sockets.js`.

## Public API

From `edgacknowledg.client` (`__all__`):

- `BatchClient` / `DirectClient` — async context managers; constructor takes
  endpoint settings and optional `status_listener`
- `BatchClientSession` / `DirectClientSession` — yielded by client `__aenter__`;
  `create_review` / `remove_review`, `close()`, `stopped()`, `start()`, `stop()`
  (client `__aexit__` awaits `stopped()` then `stop()`)
- `ConnectionStatusListener` — `connected` / `attempting_reconnect` /
  `disconnected`
- `ReviewClient` (structural **Protocol**): `review_id`, `name`, `timestamp`,
  `sections()`, `add_section` / `remove_section`
- URL/env helpers: `add_endpoint_arguments`, `form_section_http_url`,
  `default_*` host/port helpers
- Shared: `BackgroundWorkerPool`, errors

Usage:

```python
async with BatchClient(
  api_host=..., api_port=..., status_listener=listener
) as session:
  review = await session.create_review(...)
  # Block ends when session.stop() is called, or on cancellation.
```

Private (leading `_`): `_ClientReview`, `_BatchClientReview`,
`_DirectClientReview`, channel helpers. Callers should type against
`ReviewClient`, not private classes.

## Batch vs Direct

| | Batch | Direct |
| --- | --- | --- |
| Socket | One resilient `/batch` | Per-section `/source|edit/<uuid>/<kind>/<slug>` |
| Mux | `batch.subscribe` / `unsubscribe` / `event` | Dedicated WS per section |
| CLI | `edg-acknowledg-cli` | `edgy-review`, `edg-bench-review` |
| Reconnect | Drop local subs, `_restore_subscriptions()` | Each section's `ResilientWebsocket` loop |

Direct URL mapping uses handler `subscription_type` (e.g. `test.src` →
`/source/.../test/<slug>`, `bench.edit` → `/edit/.../bench/<slug>`).

## Reconnect

[`client/websocket.py`](../../../dev_tools/pylibs/edgacknowledg/client/websocket.py):
`ResilientWebsocket`, exponential backoff + jitter on connect failure;
jitter-only delay after a drop. Pattern:

```python
async for websocket in ResilientWebsocket(...):
  try:
    ...
  except ConnectionClosed:
    continue
```

Auth header: `X-AcknowlEDG-API-Key` (never put the key in query strings).
`RoleOccupied`: discard that handler / stop reconnecting that section; keep
other sections running.

## Batch subscription maps

`_BatchClientReview` tracks:

- `_subscriptions: Dict[int, uuid.UUID]` — `id(handler)` → server subscription id
- `_subscription_tasks: Dict[uuid.UUID, asyncio.Task]` — subscription id → task

Unsubscribe needs the server id; the handler-key map provides it after
reconnect bookkeeping.

## Providers

| Package | Handlers |
| --- | --- |
| `client/test/__init__.py` | `TestSourceHandler` (`test.src`), `TestEditHandler` (`test.edit`) |
| `client/bench/__init__.py` | `BenchSourceHandler` (`bench.src`), `BenchEditHandler` (`bench.edit`) |

Shared: `ReviewClientHandler`, `Channel`, `announce_section`,
`trunc_display_content` in `client/shared.py`.

## Env / URLs

- `EDG_ACKNOWLEDG_API_{HOST,PORT}`, `EDG_ACKNOWLEDG_UI_{HOST,PORT}`,
  `EDG_ACKNOWLEDG_API_KEY`
- Defaults: API `localhost:2727`, UI `localhost:5173`; non-localhost hosts
  default port 80
- Print UI section URLs as soon as the section exists (`form_section_http_url`)
- Timestamps: `utc_now_iso` / `is_iso_timestamp` — no `Z` / `+00:00` munging

## Python style here

Follow skill **edg-python-style** (2-space indent, ` = ` in kwargs,
`f"..."` / `'...'`, match neighbors).
