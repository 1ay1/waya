# The Component Library (`waya::ui`)

waya ships in two layers, and the split is the whole point.

- **The core** (`waya/surface/*`) is a complete, *unopinionated* substrate. It
  gives you primitives (`box`, `text`, `image`, `path`, inputs), a uniform mod
  vocabulary applied with `|`, layout, effects, the Elm runtime, and universal
  escape hatches (`css`, `attr`, `var`, `as`). It never picks a look for you.
  With it you can build **any** UI.

- **The component library** (`waya/ui.hpp`) is a batteries-included set of
  ready-made components — buttons, cards, dialogs, tabs, badges, and more —
  built **entirely on top of the public core**. No private API, no runtime
  hooks. Every component is a plain function you can read, copy, and own.

That mirrors maya: the core gives you a complete low-level vocabulary and dic­
tates nothing; the library is a good default so you don't rebuild the common
90% from scratch. If a component isn't quite what you want, open it — it's a
15-line function over the same `col`/`row`/`text` and `|` mods you already use.

## Using it

```cpp
#include <waya/surface/live.hpp>   // the framework
#include <waya/ui.hpp>             // the components

using namespace waya::surface;
using namespace waya::ui;

static NodeRef view(const Model& m) {
    return card(
        row(avatar("AB"), text("Ada Lovelace") | heading, push(),
            badge("pro", Tone::success), dot()),
        divider(),
        field("Email", input(m.email) | input_skin() | on_input(Edit{}),
              m.error),
        row(button("Save", Save{}),
            button("Cancel", Cancel{}, Variant::ghost))
          | gap(10)
    ) | theme(midnight());
}
```

Everything a component paints reads **theme tokens** (`var(--wa-*)`), so it
recolours automatically when you swap the theme at the root — and each carries
a sensible fallback so it looks right with no theme at all.

## The roster

### Layout & structure

- `card(children…)` — the ubiquitous panel: themed surface, border, padding,
  radius, soft elevation.
- `divider(bool vertical=false)` — a hairline rule.
- `link(label)` — an inline link look (primary colour, underline on hover).
  Pair with `tap(msg)`.

### Buttons

`Variant` picks emphasis: `primary` (filled brand), `secondary` (raised
surface), `ghost` (text-only until hover), `danger` (destructive).

- `button(label, msg, variant = primary)` — a themed button wired to a tap.
- `button_node(child, msg, variant)` — button chrome around any node (icon+text).
- `icon_button(glyph, msg, variant = ghost)` — a compact square button.

### Forms

- `field(label, control, hint = "")` — a labelled control with an optional
  helper/error line. Wrap any core input.
- `input_skin()` — a mod that applies the library's input chrome (themed,
  focus-ringed) to a raw `input()`/`textarea()`/`select()`, so all your fields
  match.

### Status & identity

`Tone` = `neutral | primary | success | warning | danger`.

- `badge(label, tone = neutral)` — a small pill.
- `dot(tone = success)` — a tiny status dot.
- `avatar(initials, d = 36)` / `avatar_img(url, d = 36)` — a circular avatar.

### Loading

- `spinner(d = 22, stroke = 0)` — a rotating ring. Registers its own keyframe
  through the [asset registry](15-assets.md) — works anywhere, no setup.
- `skeleton(w, h)` — a shimmering placeholder block for loading content.

### Navigation

- `tabs(active, {{id, "Label"}…}, to_msg)` — a themed tab bar. `to_msg` maps a
  tab id to a `Msg`; the active tab is underlined in the primary colour.

### Floating layers

Built on the core `overlay` / `anchored` primitives.

- `popover(open, trigger, panel, place = "bottom-right")` — an anchored dropdown
  with frosted-panel chrome and auto show/hide.
- `tooltip(trigger, text, place = "top")` — a hover tooltip. Uses a registered
  group-hover CSS rule, so it needs **no state** in your Model.
- `dialog(open, close_msg, panel_children…)` — a complete modal: dimmed backdrop
  that closes on click, a stopped panel so content clicks don't close it, plus
  frosted chrome and a pop-in entrance.
- `toast(message, tone = neutral)` and `toast_layer(nodes)` — a fixed,
  non-interactive top-right stack for toasts.

### Stateful widgets

These take their state as a plain argument and emit wired messages, so the state
stays in your `Model` and the component stays a pure function — no hidden state.

- `toggle(on, msg)` — an iOS-style switch; sends `msg` on change.
- `progress(pct, tone = primary)` — a determinate bar, 0..100.
- `slider(value, min, max, msg, step = 1)` — a themed range; sends the new value
  as a string on input.
- `menu(open, trigger, items…)` + `menu_item(label, msg, icon = "")` — a dropdown
  whose open state lives in your Model.
- `accordion(open_id, {{title, body}…}, on_toggle)` — collapsible sections;
  `on_toggle(i)` maps a header click to a Msg.
- `data_table(rows, columns)` — a typed, aligned CSS-grid table. Each `Column`
  maps a row to a cell node, so it's fully generic:

```cpp
data_table<User>(users, {
    { "Name",  [](const User& u){ return text(u.name); } },
    { "Role",  [](const User& u){ return badge(u.role); } },
    { "",      [](const User& u){ return icon_button_i("edit", Edit{ u.id }); } },
});
```

### Icons

`icon("name", size = 24)` returns an inline SVG that tints with `fg(…)` (it uses
`currentColor`) and scales with `size`. Only the icons you name end up in your
binary. `icon_button_i("trash", Delete{})` puts one in a themed button.

Available: `check` `x` `plus` `minus` `search` `menu` `chevron-{down,up,left,
right}` `arrow-{left,right}` `user` `settings` `trash` `edit` `heart` `star`
`bell` `home` `mail` `external` `info` `alert` `loader`.

### Charts

Data viz built on the `path` primitive — each is a node you colour and size:

- `line_chart(values, w, h)` / `sparkline(values, w, h)` — a polyline; tint with
  `stroke(hex, width)`.
- `area_chart(values, w, h)` — a filled line closed to the baseline.
- `bars(values, w, h)` — a bar chart; colour with `fg(hex)`.

```cpp
sparkline(cpu_history) | stroke(0x22d3ee, 2) | w(120) | h(32)
bars({4, 9, 2, 7}) | fg(0x8b5cf6)
```

### Forms

`field(label, control, hint)` labels any control; `input_skin()` themes a raw
input. On submit, `FormData` (core) turns the gathered `"a=1&b=2"` string into a
keyed lookup so you read fields by name:

```cpp
form(
    field("Email",    input(m.email)    | name("email")    | input_skin()),
    field("Password", input("")          | name("password") | input_skin()
                                         | input_type("password")),
    button("Sign in", std::monostate{})
) | on_submit([](std::string body){
    auto f = FormData::parse(body);
    return SignIn{ f.get("email"), f.get("password") };
});
```

## Themes

The core ships the token *mechanism* (`theme()`, `fg_*`/`bg_*` token mods, the
`Theme` struct, and the neutral `Theme::dark()`). `waya/ui.hpp` adds
ready-made palettes:

```cpp
root | theme(light());       // crisp light
root | theme(midnight());    // near-black, violet accent
root | theme(ocean());       // deep teal / cyan
root | theme(rose());        // warm light, rose accent
```

These are just `Theme` values — nothing here is privileged over a theme you
write yourself. To recolour just the accent of any theme:

```cpp
root | theme(Theme::dark().tint(0x22c55e));
```

## Writing your own

A component is a function that returns a `NodeRef`. Read any library component
as a worked example — here's `badge`, verbatim:

```cpp
inline NodeRef badge(std::string label, Tone tone = Tone::neutral) {
    auto [bg, fg] = tone_colors(tone);
    return text(std::move(label))
         | pad_x(9) | pad_y(3) | round(999)
         | css("background", bg) | css("color", fg)
         | css("font-size", "12px") | semibold;
}
```

No base class, no macro, no registration. Take arguments, return a node, chain
mods on the result. That's the whole model — and it's why the library needs no
special privileges the core doesn't already give you.
