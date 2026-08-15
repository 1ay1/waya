# The Wire Protocol

The contract between the waya server and its fixed browser client. You never
touch this — it's here so the "streams only the delta" claim is concrete and
auditable.

## Transport

- **Initial page:** an HTTP `GET` returns the fully server-rendered HTML for the
  route, including the interned CSS and the small fixed client script.
- **Live channel:** the client opens a **WebSocket** to the same origin. It
  carries both directions of the live session.

## Client → server: interactions

When the user interacts with a wired node, the client sends a **compact token**
identifying the message (and, for value-carrying events, the value):

- A **tap** sends the message's opaque wire token (an integer waya assigned when
  it registered the `tap(Msg{})`).
- An **input/change** sends the token plus the current value.
- A **keydown** sends the token if the key matches the mod's filter.

The server maps the token back to your **typed `Msg`** and dispatches it through
`update`. Your app never sees the integer.

Special control messages travel as text frames prefixed with `@`:

- `@route|<path>` — a navigation occurred; the runtime routes it through the
  app's `on_route` subscription.
- `@env|<w>|<h>|<dark>|<tz>` — the display's self-report (the browser's
  SIGWINCH): sent on connect, on debounced resize when something changed, and
  on OS colour-scheme flips. Mapped through `Sub::on_viewport`; dropped
  server-side if the app doesn't subscribe.
- `@hide` / `@show` — tab visibility. While hidden the server suppresses
  deltas (the model keeps running; nothing is painted to a screen nobody is
  watching); on `@show` one full frame resyncs the display iff anything was
  suppressed.

A picked file (from `file_input` + `on_file`) rides up as a text frame
`f<token>|<name>|<mime>|<base64>` — read client-side with `FileReader`, capped
at 8 MB raw, decoded server-side into a `FileData` before your `update` sees it.

## Server → client: paint frames

After each `update`, the server renders the new surface, diffs it against the
retained one, and sends a **binary frame**. The frame is a compact,
LEB128-varint-encoded structure:

```
frame := css-string, op-count, op*
op    := opcode, node-path, payload
```

- **css-string** — any *new* CSS rules introduced this frame (interned classes
  are only sent once).
- **op-count** — number of ops that follow.
- **node-path** — a dotted path addressing the target node in the tree.

### Opcodes

The diff emits one of these operations per changed node (`enum class Op` in
`diff.hpp`). The set is **orthogonal**: a node's mutable content is partitioned
into channels — the *shell* (attributes/class), the *text body*, the *inner*
HTML, a reflected *property*, and *children* — and each op transports exactly one
channel. A node that changed two channels (e.g. a button's style AND its label)
emits two ops that can't overlap. (See
[wire-protocol-design.md](wire-protocol-design.md) for the design rationale.)

| Wire | Op | Channel | Client action |
|---|---|---|---|
| 0 | `replace` | struct/identity | swap the whole element for a fresh subtree |
| 1 | `set_shell` | shell (attrs+class) | morph attributes in place; children untouched |
| 2 | `set_text` | text body | set `textContent` (a `<span>`/`<button>` label) |
| 3 | `set_inner` | inner HTML | set `innerHTML` (a `markup()` / SVG body) |
| 4 | `set_prop` | one reflected property | set `value` / `checked` / `src` by name |
| 5 | `remove` | struct | remove a child |
| 6 | `insert` | struct | append a child subtree |
| 7 | `move` | struct | move a keyed child `[from, to]` |
| 8 | `insert_at` | struct | insert a child subtree before index `[to]` |
| 9 | `paint` | full | repaint the whole surface (first frame / resync) |

`set_shell` ships only the element **shell** (open tag + attributes, empty body):
the client morphs attributes in place and keeps the live children, so focus,
caret and scroll survive. A counter increment produces exactly one `set_text`
op — a few bytes total. An animated `markup()` (a live SVG scene) produces one
`set_inner` per frame.

There is deliberately **no** `set_paint` (its old double duty — "attributes, and
maybe the body, depending on kind" — silently dropped `markup()`/button bodies).
Body changes ride `set_text`/`set_inner`/`set_prop`; the shell rides `set_shell`.

### Keyed reconciliation

When children carry `key(...)`, the diff matches them by key across frames and
emits `move`/`insert`/`remove` instead of rebuilding — so reordering or
prepending a list is minimal and preserves DOM identity (focus, scroll,
in-progress CSS transitions).

## Client application

The client:

1. reads the binary frame,
2. appends any **not-yet-installed** CSS rules to a single `<style>` — the
   incoming CSS is split into top-level rules and each is added at most once, so
   a long-running/animated app can't grow the stylesheet without bound (the
   server re-emits a class rule whenever it re-renders a node carrying it),
3. applies each op to the DOM by node-path,
4. coalesces every op of a frame into **one `requestAnimationFrame`**, so the
   DOM is touched once per frame regardless of how many nodes changed.

FLIP motion (for `animated()` keyed rows) is only computed on frames that carry a
**structural** op (`insert`/`move`/`remove`): a pure text/attribute/path tick
never forces the layout read that FLIP needs, so a 60 fps animation doesn't
thrash layout.

Text frames are display-side **control**, all `@`-prefixed. Navigation:
`@nav|<url>` (push history + re-route), `@rep|<url>` (replace), `@url|<url>`
(address-bar sync only). Browser effects, each the client half of a `Cmd`:
`@title|<text>` (document.title), `@scroll|<0/1>|<target>` (scroll to an
`anchor()`/`"top"`/`"bottom"`; smooth flag), `@focus|<target>` / `@blur|`,
`@copy|<text>` (clipboard, with an execCommand fallback for non-secure
contexts), and `@dl|<name>|<mime>|<base64>` (Blob + object-URL download).
`@build|<id>` is the dev hot-reload signal. Scroll and focus are deferred two
rAFs so they run strictly **after** any paint queued in the same update —
`scroll_to` a row you just inserted works.

## Error boundary

Dispatch is wrapped so that a throwing `update()` leaves the model unchanged and
the session alive — a bug in one handler can't take down the connection.

## Why this shape

- **Binary + varints** keep frames tiny; typical updates are tens of bytes.
- **Interned CSS sent once** means repeated looks cost nothing after the first
  use.
- **rAF coalescing** means many-node updates still cost one paint.
- **Opaque tokens** keep the app fully type-safe while the wire stays compact.
- **Retained surface + diff** means your `view` can be written naively (rebuild
  the whole tree every time) and still update the page minimally.

That combination is what lets you write plain, pure C++ `view` functions and
get small, fast updates for free.
