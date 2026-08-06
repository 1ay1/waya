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

The diff emits one of these operations per changed node
(`enum class Op` in `diff.hpp`):

| Opcode | Meaning |
|---|---|
| `set_text` | Replace a text node's text. |
| `set_paint` | Re-apply a node's class/attributes (style changed). |
| `set_path` | Update an SVG `path`'s geometry. |
| `set_src` | Update an image/media `src`. |
| `replace` | Replace a subtree (kind changed). |
| `insert` | Insert a new child subtree at an index. |
| `remove` | Remove a child. |
| `move` | Move a keyed child to a new index. |

A counter increment produces exactly one `set_text` op — a few bytes total.

### Keyed reconciliation

When children carry `key(...)`, the diff matches them by key across frames and
emits `move`/`insert`/`remove` instead of rebuilding — so reordering or
prepending a list is minimal and preserves DOM identity (focus, scroll,
in-progress CSS transitions).

## Client application

The client:

1. reads the binary frame,
2. appends any new CSS to a single `<style>`,
3. applies each op to the DOM by node-path,
4. coalesces every op of a frame into **one `requestAnimationFrame`**, so the
   DOM is touched once per frame regardless of how many nodes changed.

Text frames (`@route|…`, `@url|…`) are handled as history/navigation control.

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
