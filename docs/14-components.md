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

## Patterns — the page-shaped building blocks

The components above are the *parts*. **Patterns** (`waya/ui/patterns.hpp`, included
by `waya/ui.hpp`) are the page-shaped assemblies you'd otherwise hand-build from
them every time — a nav bar, a hero, a dashboard sidebar, a stat card, a form
field. Each is one call, reads the theme, and is a plain node so `| anymod` still
composes. A full product page is a dozen readable lines instead of hundreds.

### Page structure

- `page_header("Title", "subtitle", actions…)` — a big title + muted subtitle on
  the left, action nodes pushed right.
- `section("Heading", children…)` — a titled block: a small uppercase heading with
  a hairline, then the content.
- `nav_bar(brand, items…)` — a sticky, blurred top nav: brand left, links/actions
  right. Pair with `nav_link("Docs")` (muted, brightens on hover).
- `hero_section("Headline", "subhead", actions…)` — a centred hero: a fluid
  headline, a max-width subhead, and a row of CTAs. The top of a landing page.

### App shells

- `sidebar_shell(brand, {nav_items…}, content)` — a full dashboard layout: a
  fixed, **sticky** sidebar that **collapses on phones** (so the app goes
  full-width) beside a scrolling content column that's capped at a comfortable
  width. The pulse-style layout every dashboard rebuilds, in one call.
- `sidebar_item(icon, "Label", active, msg)` — a keyboard-reachable nav row,
  highlighted when `active`.

```cpp
sidebar_shell(
    row(logo, text("Acme") | semibold) | gap(10) | items_center,
    { sidebar_item("home",     "Overview", m.tab == 0, Nav{0}),
      sidebar_item("user",     "Team",     m.tab == 1, Nav{1}),
      sidebar_item("settings", "Settings", m.tab == 2, Nav{2}) },
    col(page_header("Overview", "Your dashboard"), /* … */))
) | theme(midnight()) | bg_page | fg_text
```

### Data display

- `stat("Label", "Value", "+12%", tone)` — a KPI cell: muted label over a big
  number, with an optional coloured delta chip.
- `metric_card(label, value, delta, tone, chart?)` — `stat` in a card, optionally
  with a chart/sparkline under it. `flex: 1 1 200px`, so a `row(…) | wrap` of them
  tiles responsively.
- `list_row(leading, "Title", "subtitle", trailing?)` — a list row: optional
  leading (avatar/icon), a title + subtitle, optional trailing (a time, badge,
  chevron). Absent leading/trailing add **no** node (no phantom gap).
- `key_value("Label", "Value")` — a label:value row (a definition list).

### Small chrome

- `tag("design")` — a subtle outlined chip (categories, filters); lighter than a
  `badge`. Add `tap` to make it a filter.
- `kbd("⌘")` — a keyboard-key cap for shortcut hints: `row(kbd("⌘"), kbd("K"))`.
- `banner("message", tone)` — a full-width inline alert bar with an icon and a
  tinted background (success/warning/danger/info).
- `empty_state("No results", "hint", action?, icon?)` — the friendly placeholder
  for an empty list/search: a centred icon, title, hint, and optional CTA.
- `code_block("code", "lang")` — a monospace code panel with a language tag.
- `feature_card(icon, "Title", "body", tone)` — a marketing feature cell: an
  accented icon tile over a title + paragraph.

### Forms

A labelled control in **one** call — each wraps `input + input_skin + a real
<label> + on_input`, and maps its live value to your `Msg` via a
`std::string -> Msg` mapper (or a bare `Msg` for toggles/checkboxes), so you
never wire `on_input` by hand:

```cpp
card(
  section("Account",
    email_field   ("Email",    m.email, [](auto v){ return SetEmail{v}; }, "you@x.com", "Never shared."),
    password_field("Password", m.pw,    [](auto v){ return SetPw{v};    }),
    textarea_field("Bio",      m.bio,   [](auto v){ return SetBio{v};   }),
    select_field  ("Plan", {option("Free","free"), option("Pro","pro")}, m.plan,
                            [](auto v){ return SetPlan{v}; }),
    switch_field  ("Notifications", "email + push", m.notify, ToggleNotify{}),
    checkbox_field("I agree to the terms", m.agree, ToggleAgree{})),
  form_actions(button("Cancel", Cancel{}, Variant::secondary),
               button("Save",   Save{})))
```

- `text_field(label, value, to_msg, placeholder?, hint?, kind?)` and the typed
  aliases `email_field` / `password_field` (right input type, mobile keyboard,
  masking).
- `textarea_field(…)` — a resizable multiline field.
- `select_field(label, {options}, chosen, to_msg, hint?)` — a labelled dropdown.
- `switch_field(title, desc, on, msg)` — a settings row: title + description on
  the left, a toggle on the right.
- `checkbox_field(label, on, msg)` — a checkbox + clickable inline label.
- `form_actions(buttons…)` — a right-aligned button bar for a form footer.

### Dialogs

- `confirm_dialog(open, "Title", "message", "Confirm", on_confirm, on_cancel,
  variant?)` — a ready-made yes/no modal (title, body, Cancel + primary/danger
  action bar). Renders **nothing** when closed. Built on the core `dialog()`.

## Spacing scale

`waya/ui/space.hpp` adds a 4px design-token scale so an app reads on a consistent
rhythm without ad-hoc pixel numbers. `sp(step)` is the scale (`sp(4)` == 16px);
these mods apply it by *step*:

| Mod | Meaning | `p(4)` = |
|-----|---------|----------|
| `p(n)` | padding, all sides | `pad(16)` |
| `px_(n)` | padding inline (l+r) | — |
| `py(n)` | padding block (t+b) | — |
| `gx(n)` | gap between children | `gap(16)` |
| `ma(n)` | margin, all sides | — |
| `mt(n)` / `mb(n)` | margin top / bottom | — |

```cpp
card(…) | p(6) | gx(5)          // padding 24px, gap 20px
row(…)  | gx(3) | py(4)         // gap 12px, vertical padding 16px
```

The scale is the convenient default, never a cage — reach for the raw pixel mods
(`pad(14)`) any time you need an off-scale value. (The shorthands are named to
avoid clashes: `px_` because the core `px` is a `Len` constructor, and `ma` for
margin because a bare `m` would shadow a `Model` named `m` — the universal
variable.)

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
