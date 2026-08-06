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

!!! note "memo is for distinct subtrees, not loop bodies"
    Use `memo`/`component` for a subtree that appears **once** per frame. For a
    **list**, use `each_keyed` (below) — it gives each item identity so the diff
    reconciles by key, which is both faster and what enables the animations.

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

Or the combinator form:

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

## Putting it together

The [`living` example](13-examples.md) is a todo list where rows are memoised
components, items are keyed by id, and `animated()` makes add / remove / reorder
/ filter glide. It's the whole story in one screen — copy from it.
