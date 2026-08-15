# Reusable Components

A component in waya is **just a function that returns a node**. No base class, no
macro, no lifecycle — take arguments, return a `NodeRef`, chain mods:

```cpp
NodeRef avatar(std::string url, int size) {
    return image(url) | w(size) | h(size) | round(999) | css("object-fit","cover");
}
// use it anywhere:
row(avatar(user.pic, 40), text(user.name) | bold) | gap(10) | center
```

That's the whole model. This page is about making reuse **fast** (skip rebuilding
unchanged subtrees) and **beautiful** (lists that glide on every change).

## Fast: `memo` and `component`

`view()` runs every frame. Building a small node is cheap (styling is
[zero-cost](18-typed.md#zero-cost-styling)), and the diff already **O(1)-skips**
an unchanged subtree by its content hash — so most components need nothing.

Reach for memoisation only when *building* a subtree is itself costly (a big
list, a chart, a heavy computation) and its inputs change rarely.

### `memo(props…, build)`

The last argument is a `[&]{ return NodeRef; }` builder; the leading arguments
are the props it depends on. The builder runs only when a prop changes; otherwise
the cached node is returned. No ids to invent — the call site is identified by
the builder's unique type.

```cpp
memo(user.id, user.name, user.avatar, [&]{
    return expensive_profile_card(user);   // rebuilt only when a prop changes
});
```

### `component(fn)`

Wrap a `fn(Props…) -> NodeRef` once into a reusable, **auto-memoised** component:

```cpp
auto Card = component([](std::string title, int count){
    return col(text(title) | heading, text(count) | font(32) | bold)
         | pad(20) | round(16) | bg_surface;
});

Card("Users", m.users);     // rebuilt only when (title, count) change
Card("Revenue", m.revenue); // a separate slot; its own props
```

Props are hashed to decide freshness — arithmetic, `bool`, `enum`, `std::string`,
and `Color` all work out of the box.

!!! success "Safe on interactive subtrees, and bounded"
    A memoised subtree may contain `tap`/`on_input`/`on_key` handlers: their wire
    tokens are recorded on build and replayed on every cache hit, so they always
    resolve. The cache is also swept on a timer, so memo keys may **churn
    freely** without leaking. You never manage it. See
    [Performance](24-performance.md#making-view-ochanged) for the details.

!!! note "memo is for a single subtree; use `list` for a loop"
    Use `memo`/`component` for a subtree that appears **once** per frame. For a
    **list**, use `list`/`list_versioned` (below) — they give each item identity
    *and* memoise the container, which is faster and enables the animations.

## Beautiful: keyed lists that glide

### Keyed identity

Give each list item a stable `key(...)` so the diff reconciles by **identity**,
not position. A reorder becomes a single `move` op (not a re-render), an insert a
single `insert`, a remove a single `remove` — the browser keeps the real DOM
nodes, preserving focus, scroll, and media state.

```cpp
std::vector<NodeRef> rows;
for (auto& it : sorted_filtered(m.items))
    rows.push_back(todo_row(it) | key("todo-" + std::to_string(it.id)));
```

### `list(id, range, key_fn, view_fn)` — a memoised keyed list

The combinator that makes a big list cheap. It builds each row (wrap the row in
`memo` so an unchanged row is an O(1) hit) **and** memoises the container itself,
so a frame where no row changed reuses the same parent node — no vector build, no
re-hash. Rows are keyed, so reorders reconcile as `move` ops.

```cpp
list(0, m.items,
    [](const Item& it){ return std::to_string(it.id); },        // key
    [&](const Item& it){ return memo(it.id, it.done, it.title,   // row (memoised)
                                     [&]{ return todo_row(it); }); });
```

### `list_versioned(id, version, range, key_fn, view_fn)` — O(1) when unchanged

The fastest list. Supply a cheap `version` you bump in `update()` whenever the
list's data changes; on a frame where the version is unchanged the whole list is
returned in **O(1)** — the range is never even iterated. Ideal for a big list on
a hot loop where *other* state repaints every frame but the list rarely moves.

```cpp
// Model: bump rows_ver whenever you touch rows
list_versioned(0, m.rows_ver, m.rows,
    [](const Row& r){ return std::to_string(r.id); },
    [&](const Row& r){ return row_view(r); });
```

See [Performance](24-performance.md#the-numbers) for the measured impact — a
1 000-row list drops from ~13.7 ms to ~11 µs per frame, flat in list size.

### `each_keyed(range, key_fn, view_fn)` — the low-level keyed map

When you want the keyed rows but will place them in your own container, `each_keyed`
maps a range to keyed nodes (no container memo):

```cpp
each_keyed(m.items,
    [](const Item& it){ return std::to_string(it.id); },   // key
    [](const Item& it){ return todo_row(it); });           // view
```

### `animated()` — motion for free

Add `animated()` to each keyed item and the browser **smoothly animates every
change** with FLIP (First-Last-Invert-Play): an item slides from its old position
to its new one on reorder, and fades + rises in when first inserted — with **zero
animation state in your Model**. Sorting, filtering, adding, and removing all
glide.

```cpp
todo_row(it) | key("todo-" + std::to_string(it.id)) | animated();
```

The client snapshots item positions before applying a frame's ops, then plays the
delta as a transform — so a `sort` or `filter` that reorders the list animates
automatically. No `@keyframes`, no per-item state, no timers.

### `map_msg<Child>(node, f)` — embed a widget with its OWN message type

The view-side complement of `Cmd::map`. A truly reusable widget wires its taps
and inputs in terms of **its own** message type — not the consuming app's — so a
widget library never has to know about the variants of every app that uses it.
`map_msg` lifts every message a subtree emits from the child's type to the
parent's, via `f : Child -> Parent`, at the embed site:

```cpp
// a reusable dropdown widget, defined in some widget library. Its view is
// wired entirely in terms of Dropdown::Msg { Open, Close, Pick{int} }.
namespace Dropdown {
    struct Open{}; struct Pick{ int i; };
    using Msg = std::variant<Open, Pick>;
    NodeRef view(const State& s);   // uses tap(Open{}), on_input(...) -> Msg
}

// the app embeds TWO of them and tells them apart purely by the map — no shared
// state, no widget-specific plumbing in Dropdown itself:
struct SizeEvent { Dropdown::Msg m; };
struct ColorEvent{ Dropdown::Msg m; };
using AppMsg = std::variant<SizeEvent, ColorEvent, /* ... */>;

col(
    map_msg<Dropdown::Msg>(Dropdown::view(m.size),  [](Dropdown::Msg d){ return AppMsg{SizeEvent{d}};  }),
    map_msg<Dropdown::Msg>(Dropdown::view(m.color), [](Dropdown::Msg d){ return AppMsg{ColorEvent{d}}; })
)
```

On the round-trip, a click inside the first dropdown resolves to
`AppMsg{SizeEvent{Dropdown::Open{}}}`; the second, `ColorEvent`. The event's
value (a field's text, a picked index) flows through the map untouched. This is
what makes a widget **self-contained**: its message type never has to be a
top-level alternative of the app's variant — the parent maps it in. Mapping is
safe to nest and idempotent-safe (a handler that isn't a `Child` value is left
alone), so wrapping a widget that itself wrapped a sub-widget just composes.

**The complete contract.** A self-contained *stateful* widget exposes its own
`view`, `update`, optional `subscribe`, and its own `Msg` — and the parent embeds
all of it with four mirror-image maps that share ONE lifter `f : Child -> Parent`:

| Widget exposes | Parent embeds with | Flows |
|---|---|---|
| `view(state) -> NodeRef` | `map_msg<Child>(node, f)` | messages up |
| `update(state, msg) -> (state, Cmd)` | `embed_update(model, &Model::field, update, msg, f)` | state down, cmd up |
| `subscribe(state) -> Sub<Child>` | `childSub.map(f)` | timers / listeners / topics up |

`embed_update` focuses the widget's sub-state at `model.*field`, runs the
widget's own `update`, writes the new state back, and lifts the returned command
via `f` — so the parent handles a wrapped child event in one line, with no
get/run/set boilerplate, and no widget internals leaking into the app's types. A
widget library ships a widget as `{ view, update, subscribe, Msg }`; a consumer
wires it in with a single mapper.

## Putting it together

The [`living` example](13-examples.md) is a todo list where rows are memoised
components, items are keyed by id, and `animated()` makes add / remove / reorder
/ filter glide. It's the whole story in one screen — copy from it.
