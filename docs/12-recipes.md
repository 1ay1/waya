# Recipes & Patterns

Copy-paste patterns for the situations you hit in every app. Each is built only
from the public vocabulary, so you can crack it open and adjust freely.

## A tappable button

```cpp
NodeRef button_(std::string label, auto msg, std::uint32_t bg_ = 0x6366f1) {
    return text(label)
        | fg(0xffffff) | bg(bg_)
        | pad_x(18) | pad_y(11) | round(11)
        | pointer | interactive()        // hover/press feedback
        | tap(msg);
}
```

## A card

```cpp
NodeRef card(NodeRef body) {
    return body
        | pad(20) | round(16)
        | bg(0x1e293b) | border(1, 0x334155)
        | transition() | on(Hover, elevation(3));
}
```

## A live search box

```cpp
// Model: std::string query;
// Msg:   struct SetQuery { std::string v; };

input(m.query)
    | placeholder("Search…")
    | on_input([](std::string v){ return SetQuery{ v }; });

// in view, filter with the query:
for (auto& item : m.items)
    if (m.query.empty() || contains_ci(item.name, m.query))
        rows.push_back(row_of(item));
```

## A list with a stable key

Give repeated rows a `key` so the diff moves them instead of rebuilding:

```cpp
col_(each(m.todos, [](const Todo& t){
    return todo_row(t) | key("todo:" + std::to_string(t.id));
}))
```

## Conditional content

```cpp
col(
    header,
    when(m.loading, spinner()),            // shown only while loading
    when(m.error.empty(), body, error_panel(m.error)),   // either/or
    when(!m.items.empty(), [&]{ return list(m.items); }) // lazy build
)
```

## A modal dialog

```cpp
// Model: bool show_modal;
when(m.show_modal,
    overlay(
        col(
            text("Delete this?") | font(20) | bold,
            row(
                button_("Cancel", CloseModal{}, 0x334155),
                button_("Delete", ConfirmDelete{}, 0xf87171)
            ) | gap(10) | between
        ) | pad(24) | round(16) | bg(0x1e293b) | max_w(rem(24)) | stop() | pop_in()
    ) | tap(CloseModal{})                  // click backdrop to close
)
```

`stop()` on the panel keeps a click inside from bubbling to the backdrop's
`tap`.

## A responsive dashboard grid

```cpp
grid(rem(18),
    stat_card("Revenue", "$42.1k", "+12%"),
    stat_card("Users",   "8,120",  "+3%"),
    stat_card("Churn",   "1.2%",   "-0.3%"),
    stat_card("Uptime",  "99.9%",  "")
)
```

Where a stat card is:

```cpp
NodeRef stat_card(std::string label, std::string value, std::string delta) {
    std::uint32_t dc = delta.empty() ? 0x94a3b8
                     : delta[0]=='+' ? 0x34d399 : 0xf87171;
    return col(
        text(label) | fg(0x94a3b8) | font(13) | css("text-transform","uppercase"),
        text(value) | fg(0xe2e8f0) | font(30) | weight(Weight::black),
        when(!delta.empty(), text(delta) | fg(dc) | font(13) | semibold)
    ) | gap(6) | pad(20) | round(16) | bg(0x1e293b) | border(1, 0x334155);
}
```

## A progress bar

```cpp
NodeRef progress(float frac /*0..1*/, std::uint32_t c = 0x6366f1) {
    frac = std::clamp(frac, 0.f, 1.f);
    return box(
        box() | h(pct(100)) | w(pct(frac*100)) | round(999) | bg(c)
              | css("transition","width .4s ease")
    ) | w(pct(100)) | h(px(8)) | round(999) | bg(0x1e293b) | clip;
}
```

## A chat bubble

```cpp
NodeRef bubble(std::string msg, bool mine) {
    auto b = text(msg) | font(15) | leading(1.45f) | pad_x(14) | pad_y(10)
        | css("max-width", "72%") | css("white-space", "pre-wrap");
    b = mine ? (b | fg(0xffffff) | bg(0x6366f1) | css("border-radius","16px 16px 4px 16px"))
             : (b | fg(0xe2e8f0) | bg(0x334155) | css("border-radius","16px 16px 16px 4px"));
    return row(mine ? push() : box(), b, mine ? box() : push());
}
```

## A live clock

```cpp
// Msg: struct Tick {};
static Sub<Msg> subscribe(const Model& m) {
    return Sub<Msg>::every(1000, Tick{});
}
static std::pair<Model,Cmd<Msg>> update(Model m, Msg msg) {
    if (std::holds_alternative<Tick>(msg)) { m.seconds++; }
    return { m, Cmd<Msg>::none() };
}
```

## Async data load

```cpp
// Msg: struct Load {}; struct Loaded { std::string body };
[&](Load) -> std::pair<Model,Cmd<Msg>> {
    return { m.loading(), Cmd<Msg>::fetch("http://api.local/items",
        [](std::string body){ return Loaded{ body }; }) };
},
[&](const Loaded& l) -> std::pair<Model,Cmd<Msg>> {
    return { m.with_items(parse(l.body)), Cmd<Msg>::none() };
},
```

## Theming

`sugar.hpp` ships a `Theme` — semantic tokens (page/surface/text/primary…) that
restyle a whole subtree in one paint:

```cpp
NodeRef view(const Model& m) {
    return col(app_content)
        | theme(m.dark ? Theme::midnight() : Theme::light())
        | themed()
        | theme_transition();     // animate the swap
}
```

Inside the themed subtree, reference tokens instead of hex:

```cpp
text("Title") | fg_text
card_body     | bg_surface | border_token()
button        | bg_primary | fg_on_primary
```

Flip `m.dark` and the entire app re-tints smoothly, because every token resolves
against the active theme.

## Multiplayer (broadcast)

```cpp
// publish on send
[&](Send){ return { m.clear_draft(), Cmd<Msg>::broadcast("room", m.draft) }; }

// receive on every session (incl. sender)
static Sub<Msg> subscribe(const Model&) {
    return Sub<Msg>::on_topic("room", [](std::string payload){
        return Received{ payload };
    });
}
```

Remember: the sender also receives its own broadcast — make the receive handler
the single source of truth, and don't also apply the change locally.

## A keyed drag-and-drop board (Kanban)

```cpp
// a draggable card
card(item) | draggable(std::to_string(item.id)) | key("card:" + std::to_string(item.id))

// a column that accepts drops, tagged with its own id
column_body
    | on("drop-arg", std::to_string(col.id))
    | on_drop([](std::string s){                 // s = "<cardId>:<colId>"
        return MoveCard{ parse_move(s) };
      });
```

---

For complete, runnable apps that combine these, see the
[Examples Walkthrough](13-examples.md).
