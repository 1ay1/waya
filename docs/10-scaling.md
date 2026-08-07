# Scaling to Big Apps

The Elm Architecture stays clean for small apps automatically. At scale — many
features, a large `Model`, dozens of messages — the risk is one giant `update`
where every feature's logic and message ids collide. waya's `scale.hpp` gives
you the same fix Elm and Redux use: **feature modules**.

## The problem

Without structure, a big app tends toward:

- One `Model` struct with everything jammed in.
- One `Msg` type where feature A's messages sit next to feature B's.
- One 3,000-line `update` switch nobody can navigate.

## The fix: feature modules

Split the app into features, each owning:

- **a slice of the shared `Model`** (a struct field),
- **a contiguous block of message ids** (so ids never collide), and
- **its own small `update`** that only knows its slice.

Each feature is written once, sees its messages **relative to its base**
(`0, 1, 2…`), and drops in at any base without collision.

```cpp
struct Model {
    Auth auth;      // auth feature's slice
    Cart cart;      // cart feature's slice
    Feed feed;      // feed feature's slice
};

// Each feature owns a 100-id block.
enum : int { AuthBase = 100, CartBase = 200, FeedBase = 300 };

// A feature reducer: sees LOCAL msg ids (0,1,2…), mutates the shared model,
// returns any Cmd.
Cmd<int> auth_update(Model& m, int local, const std::string& value) {
    switch (local) {
        case 0: m.auth.login(value); break;
        case 1: m.auth.logout(); break;
    }
    return Cmd<int>::none();
}
```

## `feature` and `combine`

`feature(base, reducer)` registers a feature owning `base .. base+span-1`
(span defaults to 100). `combine(model, msg, value, features…)` routes each
message to the feature that owns it, calling its reducer with the **local**
(rebased) id.

```cpp
static std::pair<Model, Cmd<int>> update(Model m, int msg, std::string value) {
    return combine(std::move(m), msg, value,
        feature(AuthBase, auth_update),   // handles 100..199 → local 0..99
        feature(CartBase, cart_update),   // handles 200..299
        feature(FeedBase, feed_update));  // handles 300..399
}
```

Your top-level `update` becomes a **table**, not a monster. Adding a feature is
one line; each reducer stays ~50 lines and is independently testable.

### Reducer shapes

`feature` accepts several reducer signatures and adapts:

```cpp
Cmd<int> r(Model&, int local, const std::string& value);  // full
Cmd<int> r(Model&, int local);                            // value-less
void     r(Model&, int local, const std::string& value);  // no Cmd (returns none)
```

### The `Feature` type

```cpp
template <typename Model>
struct Feature {
    int base;    // first msg id owned
    int span;    // how many ids (default 100)
    std::function<Cmd<int>(Model&, int local, const std::string&)> update;
    bool owns(int msg) const;   // base ≤ msg < base+span
};
```

You rarely touch it directly — `feature(...)` builds it and `combine(...)`
consumes it.

## Organising a big codebase

A workable layout for a large waya app:

```
src/
  model.hpp          // the shared Model struct (feature slices as fields)
  msg.hpp            // the id-base enum (AuthBase, CartBase, …)
  app.cpp            // init, the combine() update, the view router, main()
  features/
    auth.hpp/.cpp    // Auth slice + auth_update + auth view functions
    cart.hpp/.cpp    // Cart slice + cart_update + cart view functions
    feed.hpp/.cpp    // …
  ui/
    components.hpp    // shared components (cards, buttons, chrome)
```

- Each feature file exports its slice type, its reducer, and its view
  functions.
- `app.cpp`'s `view` uses the [route/screen switch](09-routing-seo.md) to pick
  which feature renders.
- Shared components live in `ui/` and are just functions returning `NodeRef`.

## Testing at scale

Feature reducers are pure functions over the shared model, so each is testable
in isolation:

```cpp
Model m{};
auto cmd = auth_update(m, /*local=*/0, "ada@x.com");
assert(m.auth.user == "ada@x.com");
assert(cmd == Cmd<int>::none());
```

And the whole `combine` dispatch is testable by feeding global message ids:

```cpp
auto [m2, cmd] = App::update(m, AuthBase + 1, "");   // routes to auth_update(local=1)
assert(!m2.auth.logged_in);
```

## When to reach for this

- **Small app (one feature, a handful of messages):** don't. A single `Model`,
  a `std::variant` `Msg`, and one `update` is clearer.
- **Growing app (several independent features):** adopt feature modules early —
  the id-base discipline prevents the collisions that make big Elm apps painful.

## Scaling the render, not just the code

Every frame, `view(model)` returns a fresh surface and the runtime diffs it
against the last one, streaming only the delta. The **diff and the wire are
already O(changed)** — a one-field change is tens of bytes regardless of page
size. The one cost that scales with the tree is `view()` itself rebuilding
nodes. Three primitives make that O(changed) too, so a big app stays at
thousands of frames per second of headroom.

### `memo(props..., build)` — skip rebuilding an unchanged subtree

```cpp
memo(user.id, user.name, user.avatar, [&]{ return profile_card(user); });
```

The builder runs only when a prop changes; otherwise the cached node is reused
and the diff O(1)-skips it. Safe on **interactive** subtrees too — a memoised
node's `tap`/`on_input` handlers are recorded and replayed on a cache hit, so
their tokens always resolve. The cache is bounded (stale slots are swept), so
keys may churn freely.

### `list(id, range, key_fn, view_fn)` — a memoised keyed list

Builds each row (wrap it in `memo` for O(1) unchanged rows) **and** caches the
container itself, so a list whose rows didn't change reuses the same parent node
— no vector build, no re-hash. Rows are keyed, so reorders reconcile as moves.

### `list_versioned(id, version, range, key_fn, view_fn)` — O(1) when unchanged

The fastest list. Supply a cheap `version` (a counter you bump in `update()`
whenever the list's data changes). On a frame where the version is unchanged the
whole list is returned in **O(1)** — the range is never iterated, no per-row work,
no allocation. Use it for a big list on a hot loop where *other* state (a clock,
a cursor, a drag) repaints every frame but the list rarely moves.

```cpp
// bump m.rows_ver in update() whenever you touch m.rows
list_versioned(0, m.rows_ver, m.rows,
    [](auto& r){ return std::to_string(r.id); },
    [&](auto& r){ return row_view(r); });
```

**Measured** (a dashboard whose clock ticks every frame while the table is
unchanged), full frame = `view() + diff + encode`:

| rows  | plain `view()` | per-row `memo` | `list_versioned` |
|-------|----------------|----------------|------------------|
| 100   | ~1.3 ms        | ~32 µs         | ~12 µs           |
| 1000  | ~13.7 ms       | ~207 µs        | ~11 µs           |
| 10000 | ~140 ms        | ~2 ms          | ~11 µs           |

`list_versioned` is **flat in list size** — the frame cost is O(what changed),
not O(page). That is the property that keeps a large waya app feeling instant.

---

Next: the [API Reference](11-api-reference.md) — every function, mod, and type
in one place.
