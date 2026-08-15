# Performance

waya is built to be the **fastest way to put a live UI on a screen** — not just
fast to write, but fast to run, at scale, on every frame. This page explains
*why* it's fast, *what* the numbers actually are, and *how* to keep a large or
high-frequency app pinned at the ceiling.

The short version:

- The **wire is O(changed)**: after the first paint, an interaction streams only
  the changed nodes — tens of bytes, not a re-render — regardless of page size.
- The **diff is O(changed)**: an unchanged subtree is skipped in O(1) by its
  content hash.
- With `memo` / `list` / `list_versioned`, **`view()` becomes O(changed) too** —
  so the *whole frame* costs time proportional to what changed, not to how big
  the page is. A 10,000-row screen where one clock ticks costs the same as a
  1-row screen.
- Taps feel **instant** (`tap_pop`) even across a network hop, because feedback
  is local while the authoritative result streams back.

---

## The frame, end to end

Every interaction runs one pass of the loop on the server:

```
token ──▶ update(model, msg) ──▶ view(model) ──▶ diff(prev, next) ──▶ encode ──▶ WebSocket
             (your logic)         (rebuild)        (delta)             (bytes)      (send)
```

Three of those stages have already been driven to their floor; the fourth
(`view`) is the one *you* influence, and the memo primitives below flatten it.

| Stage | Cost | Notes |
|-------|------|-------|
| `diff(prev, next)` | **O(changed)** | Unchanged subtree → O(1) skip by hash. ~0.6 µs for a 1-field change on a 1000-node tree. |
| `encode` (delta) | **O(changed)** | Binary frame: varint paths, one op byte, UTF-8 payloads. A `set_paint` ships an element *shell*, not its subtree (the client morphs attributes in place and keeps existing children). |
| WebSocket send | **O(bytes)** | A counter tick is ~22 bytes. |
| `view(model)` | **O(tree)** by default | Rebuilds + re-hashes every node. This is what `memo`/`list` make O(changed). |

---

## Making `view()` O(changed)

`view()` is a pure function of the model, so by default it rebuilds the whole
surface each frame. For most apps that's completely fine — building a node is
cheap and the diff throws away the unchanged parts. You only need the tools below
when a screen is **large** (hundreds+ of live nodes) or **high-frequency**
(something repaints every frame while most of the screen is static).

### `memo(props…, build)` — cache one subtree

The builder runs only when a prop changes; otherwise the cached node is returned
and the diff O(1)-skips it. The call site is identified by the builder's unique
type, so there are no ids to invent.

```cpp
// the header only changes when score/combo change — not on the 30 fps tick
auto header = memo(m.score, m.combo, [&]{
    return row(text("Score"), text(m.score) | bold) | gap(8);
});
```

!!! success "memo is safe on interactive subtrees"
    A memoised subtree may contain `tap`, `on_input`, `on_key`, … Their wire
    tokens are **recorded on build and replayed on every cache hit**, so they
    always resolve *and* every later sibling's token stays correctly ordered.
    Earlier versions of the idea were only safe for static content; waya's is
    safe everywhere.

!!! note "the cache is bounded"
    Memo slots are swept on a generation timer, so keys may **churn freely** — a
    game that memoises by a per-frame coordinate will not leak. You never manage
    the cache.

### `list(id, range, key_fn, view_fn)` — a memoised keyed list

Two wins in one call:

1. builds each row through `view_fn` (wrap it in `memo` for O(1) unchanged rows);
2. memoises the **container itself** — when no row changed it returns the same
   parent node, skipping the vector build *and* the subtree re-hash.

Rows are keyed by `key_fn`, so reorders reconcile as `move` ops and each row's
DOM (focus, scroll, media) survives.

```cpp
list(0, m.items,
    [](const Item& it){ return std::to_string(it.id); },
    [&](const Item& it){ return memo(it.id, it.done, it.title,
                                     [&]{ return todo_row(it); }); });
```

### `list_versioned(id, version, range, key_fn, view_fn)` — O(1) when unchanged

The fastest list. Supply a cheap **version** — a counter you bump in `update()`
whenever the list's data changes (or a hash you already keep). On any frame where
the version is unchanged, the whole list is returned in **O(1)**: the range is
never iterated, no row is built, nothing is allocated.

This is the tool for a big list on a hot loop — a table under a ticking clock, a
feed behind a live cursor, a leaderboard during an animation.

```cpp
struct Model {
    std::vector<Row> rows;
    long             rows_ver = 0;   // bump this in update() when rows change
};

// in view():
list_versioned(0, m.rows_ver, m.rows,
    [](const Row& r){ return std::to_string(r.id); },
    [&](const Row& r){ return row_view(r); });
```

---

## The numbers

A dashboard whose clock ticks **every frame** while the table stays unchanged.
"Full frame" is `view() + diff + encode` — the entire per-frame server cost:

| rows   | plain `view()` | per-row `memo` | `list_versioned` |
|--------|----------------|----------------|------------------|
| 100    | ~1.3 ms        | ~32 µs         | **~12 µs**       |
| 1 000  | ~13.7 ms       | ~207 µs        | **~11 µs**       |
| 10 000 | ~140 ms        | ~2 ms          | **~11 µs**       |

Read the last column: `list_versioned` is **flat in list size**. At 1 000 rows
that's a **1 250× speedup** over the naïve path, and the frame cost is now
governed by *what changed*, not by how big the page is. That is the property that
keeps a large waya app feeling instant.

!!! tip "Measure your own hot path"
    `bench/frames.cpp` measures the wire delta and diff cost on a realistic
    dashboard. Copy its structure to profile a specific screen: build `prev`
    once, then time `view()` + `diff` + `encode_delta` in a loop.

## The engine underneath

The numbers above are `list_versioned` *avoiding* work. But even the naive path
— rebuild the whole tree, diff it, ship the delta — is fast, because the core is
tuned for exactly that cycle:

- **The diff is O(1)-skip.** Every node carries a content hash of its whole
  subtree; `diff` compares hashes and returns immediately on a match, so an
  unchanged subtree costs one integer compare no matter how deep. `diff+encode`
  is ~1 µs per frame regardless of app size.
- **Node storage is pooled.** A node is ~350 bytes and `view()` rebuilds the
  whole tree each frame. The blocks are recycled through a thread-local pool —
  frame N's nodes are freed as frame N+1's are built, feeding a free-list — so
  steady-state rendering makes **zero `malloc` calls for node storage**.
- **Hashing is word-wise.** A node's style is hashed 8 bytes at a time over its
  packed POD fields, not field-by-field — the single biggest per-node cost, cut
  ~11×. Building a styled node is ~300 ns.
- **The stylesheet interns in O(1).** Identical styles collapse to one CSS class
  via a hash lookup, so a page with N distinct styles is O(N), not O(N²).
- **Nothing redundant per message.** Subscriptions reconcile only when they
  actually change (a fingerprint skips the global-lock topic sync on a
  keystroke); the wire frame is built in one allocation; and only a non-empty
  delta is ever sent — an identical re-render sends zero bytes.

The upshot: a 2 000-row app rebuilds, diffs, and recycles its **entire tree in
~1 ms** (>1000 fps of headroom), and only the delta — a few hundred bytes —
crosses the wire. You get this for free; the `memo`/`list_versioned` tools above
are for when even that whole-tree rebuild is more than you want to pay.

---

## Perceived performance: instant taps

Raw throughput isn't the whole story — *felt* latency is. Two mods make an
interaction feel immediate even when the server is a network hop away:

### `tap_pop()` — instant press feedback

On pointer-down the element plays a tiny scale "pop" **right away**, with zero
round-trip, while the authoritative result streams back and paints a moment
later. Use it on lively tap targets — game pieces, toggles, cards.

```cpp
star | tap(Pop{s.id}) | tap_pop();
```

### `optimistic()` — instant busy state

The perceptual counterpart for actions that *commit*: on click the element shows
a busy state (dim + disabled) immediately, cleared on the next paint. Use it on a
submit/save button so a slow link still feels responsive.

```cpp
button("Save") | tap(Save{}) | optimistic();
```

---

## Animating without layout thrash

For per-frame motion (a drifting sprite, a moving cursor), two rules keep the
browser on the compositor instead of thrashing layout:

- **Position with `at_pct(top%, left%)`**, not an interned position class. It
  emits an inline `top`/`left` + `translate(-50%,-50%)` + `will-change`, so a
  node that moves every frame keeps a **stable CSS class** (only the cheap attr
  delta changes) instead of minting a new interned class per coordinate.
- **FLIP only runs on structural frames.** The client takes a
  `getBoundingClientRect` snapshot (a forced layout) only when a frame actually
  reorders (`insert`/`move`/`remove`). A pure attribute/text/path tick — the
  common animation case — skips it entirely.

Both are automatic; `at_pct` is the one you opt into.

```cpp
box(disc) | at_pct(star.y / H * 100.f, star.x / W * 100.f) | tap(Pop{star.id});
```

See the [`swarm` example](13-examples.md#swarm-a-real-time-multiplayer-game) for the whole pattern: memoised
static regions, `at_pct` sprites, `tap_pop` targets, and a shared leaderboard —
a real-time multiplayer game that stays smooth.

---

## A checklist for a fast screen

- **Most apps:** do nothing. The diff + wire are already O(changed).
- **A big list?** `list(...)` with per-row `memo`, or `list_versioned(...)` if
  you can supply a version.
- **A static region under a high-frequency tick?** `memo(...)` it by the props
  that actually change.
- **Per-frame motion?** `at_pct(...)` for position, `tap_pop()` for feedback.
- **A committing action over a slow link?** `optimistic()`.

Reach for these when a screen is large or hot; otherwise, plain `view()` is the
clearest code and already fast.

---

Next: the [Component Library](14-components.md) — the ready-made buttons, cards,
charts, and dialogs built on this foundation.
