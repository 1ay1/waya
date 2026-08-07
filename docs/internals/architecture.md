# How It Works

This page explains what happens inside waya between your `view` and the pixels.
You don't need any of it to build apps — but it makes the design choices legible.

!!! tip "New to the web terms below?"
    This page uses **DOM**, **WebSocket**, **SSR**, **diffing**, and **HTTP**
    freely. If any are unfamiliar, the [Foundations](../foundations/00-how-the-web-works.md)
    track explains each from scratch — especially
    [The DOM & Rendering](../foundations/02-dom-and-rendering.md).

## The lifecycle of a frame

```
browser                          waya server (per session)
   │                                     │
   │  HTTP GET /route  ───────────────▶  init() → update(route) → view(model)
   │                                     │  DomBackend.render(surface) → html+css
   │  ◀───────────────  full HTML page   │  + meta(model) in <head>
   │                                     │
   │  open WebSocket   ───────────────▶  session created, previous surface retained
   │                                     │
   │  user taps  ─────  token  ───────▶  token → typed Msg → update(model, msg)
   │                                     │  view(new_model) → diff(prev, new)
   │  ◀──────────────  binary delta      │  retain new surface as prev
   │  apply patch                        │
   │                                     │
```

The server owns every frame. The browser only paints the initial HTML and
applies patches — there is no application logic or state in the client.

## SSR: the first byte is the finished page

When a request arrives, the runtime:

1. Calls `init()` to create the session's model.
2. Feeds the requested route through your `update` (via the `on_route`
   subscription path), so `model` reflects that route.
3. Calls `view(model)` and renders it with the **DOM backend** to HTML plus a
   block of interned CSS.
4. Inserts `meta(model)` into the `<head>`.
5. Sends the complete page.

So a crawler or a first-time visitor receives the fully-rendered route
immediately — no loading state, no hydration gap. This is why waya apps are
SEO-friendly by construction.

## The DOM backend

`DomBackend::render(node)` walks the surface and emits:

- **HTML** — each node becomes an element (`box`→`<div>` or its `as(...)` tag,
  `text`→text, `image`→`<img>`, `path`→`<svg>`, controls→real form elements).
- **Interned CSS** — styles are deduplicated into shared class rules, not inline
  styles. Two nodes with the same look share one class, so the CSS stays small
  even for a big page.

```cpp
DomBackend::Output out = DomBackend{}.render(*surface);
// out.html, out.css
```

The backend is the only place that knows about HTML. Everything above it — your
`view`, the layout primitives, the mods — is substrate-neutral. That's the
Surface Model's promise: swap the backend and the app is unchanged.

## Retained surfaces & diffing

waya keeps the last surface for each session. After `update`, it renders the new
surface and **diffs** it against the retained one, producing a minimal patch.
The op set is small and substrate-neutral:

- text change → `set_text`,
- style / attribute / handler change → `set_paint` (the client morphs the live
  element's attributes in place),
- vector-path change → `set_path`,
- structural change → `insert` / `remove` / `replace` / `move`,

each addressed by a node path (a dotted sequence of child indices). Only the
changed nodes travel. A counter click that changes one number produces a handful
of bytes, not a re-rendered page.

!!! note "`set_paint` ships a shell, not a subtree"
    The client applies `set_paint` by **morphing the element's own attributes**
    and leaving its children alone (they reconcile via their own deeper ops). So
    the server serialises only the element *shell* — its open tag + attributes,
    with an empty body — not the whole subtree. For a deep node that only changed
    a style, that's the difference between shipping ~130 bytes and ~600.

### Keyed lists

For lists that reorder or grow, give rows a `key(...)`. The diff then matches
rows by key and emits **moves** instead of rebuilding, so a reordered or
prepended list updates minimally and preserves focus/scroll where possible.

### Memoisation: making `view()` O(changed)

`view()` is pure, so by default it rebuilds the whole surface each frame; the
diff then discards the unchanged parts. For large or high-frequency screens the
`memo` / `list` / `list_versioned` primitives (`surface/component.hpp`) cache
built subtrees keyed by their inputs, so an unchanged region is neither rebuilt
nor re-hashed — the *whole frame* becomes proportional to what changed. The memo
cache is generation-swept (so churning keys don't leak) and records + replays a
subtree's wire tokens on a cache hit (so memoising an **interactive** subtree
doesn't break its taps). See [Performance](../24-performance.md) for the model
and the numbers.

## The client runtime

The only JavaScript waya ships is a small, fixed script (identical for every
app). It:

1. opens the WebSocket,
2. sends a compact token when a wired node is interacted with,
3. receives binary delta frames and applies the patch to the DOM,
4. handles history/route control frames (for `navigate`/`push_url`).

There is no application bundle, no virtual DOM in the browser, and nothing to
hydrate.

## Sessions & threads

Each connection is handled on its own thread and holds its **own model**. A slow
or blocked client cannot stall others, and per-user state is isolated by
default. Cross-session state is explicit, via `broadcast`/`on_topic`.

## Effects

`Cmd`s returned from `update` are performed by the runtime — `after`/`task`/
`fetch` run on worker threads and deliver their result back as a `Msg`;
`navigate`/`push_url` emit control frames; `broadcast` fans a payload out to
every session on a topic. Subscriptions (`Sub`) are reconciled each frame: the
runtime starts and stops timers/topic listeners to match what the current model
declares.

## Module layering

waya is deliberately layered so each concern lives in one file and the
dependency arrows only ever point **up** — the UI core never knows the runtime
exists. From bottom to top:

| Layer | Files | Depends on | Knows about… |
|-------|-------|-----------|-------------|
| **UI core** | `color`, `core/hash`, `surface/node`, `dom`, `diff`, `binary`, `wire`, `layout`, `typed`, `validate`, `component` | only each other | nodes, mods, diffing. **Nothing** about sockets, sessions, or the browser. |
| **The ideas** | `surface/effect` (Cmd/Sub), `surface/program` (the Elm hooks), `surface/meta`, `surface/router`, `surface/scale` | the core | the pure data-flow loop: how `init`/`update`/`view`/`subscribe` compose. Still no transport. |
| **The terminal** | `surface/client` | nothing (it's a JS string) | how to decode a binary frame and paint it. A dumb VT100 for the web. |
| **The transport/runtime** | `net/ws`, `net/http`, `surface/live` | everything above | sockets, the WebSocket handshake, HTTP serving, `Session`/`Hub`/`Pool`/`SessionStore`, and the shell that composes the SSR page + the client. |
| **The library** | `ui/*` + `ui.hpp` | the core + the ideas | ready-made components. Never the runtime. |

The load-bearing invariant, enforced and tested: **no core file references
`live`, `Session`, or a socket.** You can render a surface, diff two surfaces,
and run the whole `Model → update → view` loop with the core + the ideas alone;
`surface/live.hpp` is just one way to *serve* that loop. A different backend
(a test harness, a native shell, a different protocol) would reuse everything
below it unchanged.

This is why the pieces stay small and swappable: `client.hpp` is the entire
browser terminal (~200 lines, one string) with no C++ logic; `program.hpp` is
the Elm loop with no sockets; `live.hpp` is the serving glue and nothing else.

---

See [The Wire Protocol](wire-protocol.md) for the on-the-wire details.
