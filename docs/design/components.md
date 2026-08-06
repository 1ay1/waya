# Components in waya

There is no `Component` class, no lifecycle, no boilerplate. **A component is a
plain function that returns a `NodeRef`.** You build and compose them with the
exact same `|`-pipe grammar you use for a single box — so a reusable widget and a
one-off element are written the same way.

```cpp
// A component: a function → NodeRef.
NodeRef badge(std::string label, std::uint32_t color) {
    return text(std::move(label))
         | fg(0xffffff) | font(12) | semibold
         | pad_x(10) | pad_y(4) | round(999) | bg(color);
}

// Use it anywhere, compose it with anything:
row(badge("LIVE", green), badge("beta", cyan)) | gap(8);
```

## Slot-passing (children)

Make the component **variadic** to accept arbitrary children — this is "slots":

```cpp
template <typename... Cs>
NodeRef card(Cs... cs) {
    return col(std::move(cs)...) | gap(16) | pad(20) | round(16) | bg(surface);
}

card(
    text("Title") | font(20) | bold,
    text("Body copy…") | fg(muted),
    button("OK") | tap(Ok)
);
```

## Data-driven lists

`each` / `each_i` / `each_keyed` map a range to nodes; feed them to `row_`/`col_`
(the vector-taking builders):

```cpp
col_(each(items, [](const Item& x){ return row(text(x.name), text(x.value)); }))

// with an index:
row_(each_i(tabs, [&](const std::string& t, std::size_t i){ … }))

// keyed → the diff reconciles by identity (drag/reorder is a MOVE, not a repaint):
col_(each_keyed(cards,
    [](const Card& c){ return "card:" + std::to_string(c.id); },   // key
    [](const Card& c){ return card_view(c); }))                    // view
```

## Conditionals

```cpp
col(
    header,
    when(loading, spinner()),               // node or empty when false
    when(tab == 0, table(), settings()),    // a or b
    modal(dialog_open, confirm_dialog())    // an overlay, only when open
)
```

## Composition all the way up

`view(Model)` is itself just a function returning a `NodeRef` — the root
component. There is nothing special about it; it composes smaller components the
same way they compose their children. See `examples/dashboard.cpp` for a dense,
real UI (avatar, badge, card, stat, toggle-row, tab-bar, data-table) built
entirely from these functions.

## Compliance

Everything renders to **valid HTML5 + CSS3** — the dashboard example passes the
W3C Nu HTML validator and the W3C CSS validator with **zero errors**. You never
write HTML, CSS, or event wiring; waya owns the output and it is standards-clean.

## One gotcha

`on` is a modifier name (`on(Hover, …)`, `on("keydown", …)`). Don't shadow it
with a local variable named `on` inside a `view`/component — the compiler will
report *"`on` cannot be used as a function."* Name the local something else
(`active`, `is_on`).
