# Vocabulary Cheat Sheet

Every mod at a glance, grouped by intent. Pipe them onto any node with `|`.
If it's here, you don't need `css()`. (Everything takes a bare `px` number
*or* a `Len` like `16_px`/`1.5_rem`/`50_pct` where a size is expected.)

## Layout — containers

| Mod | Effect |
|-----|--------|
| `row(...)` `col(...)` `stack(...)` `grid(...)` | flow boxes (stack overlaps; grid is 2-D) |
| `gap(n)` | space between children *(typed: needs a container)* |
| `center` `between` | justify/align shortcuts |
| `justify_center` `justify_between` … `align_center` `align_stretch` | alignment *(typed dialect)* |
| `wrap_on` `wrap_off` | flex wrapping |
| `grow(n)` `shrink(n)` | flex item grow/shrink |
| `grid_cols(n|tracks)` `grid_rows(...)` `grid_areas(...)` `auto_grid(min)` | grid tracks |
| `col_span(n)` `row_span(n)` `area("name")` | grid child placement |
| `columns(n, cells…)` | quick n-column aligned grid (tables) |

## Layout — responsive & intrinsic

| Mod | Effect |
|-----|--------|
| `switcher(threshold, …)` | row that flips to a column when narrow |
| `sidebar(side, main, w)` | sidebar + fluid main that stacks |
| `center_col(...)` | centered reading column |
| `cluster(...)` | wrapping tag/chip row |
| `on_phone(mods…)` `on_desktop(mods…)` | breakpoint overrides |
| `hide_below(bp)` `only_phone()` `only_desktop()` | conditional visibility |

## Sizing & box model

| Mod | Effect |
|-----|--------|
| `w(n)` `h(n)` `size(n)` | width / height / square |
| `min_w(n)` `max_w(n)` `min_h(n)` `max_h(n)` | clamps |
| `fill` `hug` | 100% / auto |
| `aspect(ratio)` | aspect-ratio box |
| `pad(n)` `pad_x(n)` `pad_y(n)` `margin(n)` | spacing |
| `round(n)` | corner radius |
| `border(color, w)` `hairline(color)` | borders |

## Position & scroll

| Mod | Effect |
|-----|--------|
| `sticky_top(off)` `sticky_bottom(off)` | sticky header/footer, one word |
| `fixed` `absolute()` `relative` `sticky` | position mode |
| `positioned()` | make a box the anchor for absolute children |
| `pin_top_right(o)` `pin_top_left` `pin_bottom_right` `pin_bottom_left` | corner-pin a child |
| `top(n)` `bottom(n)` `left(n)` `right(n)` | edge offsets (explicit 0 works) |
| `pin()` `inset(t,r,b,l)` | fill / all-edges |
| `z(n)` | stacking order |
| `scroll` `scroll_x` `scroll_y` `clip` `no_scrollbar` `scroll_fill()` | overflow |

## Colour & fill

| Mod | Effect |
|-----|--------|
| `fg(color)` `bg(color)` | text / background (hex `0x…`, `rgb`, `rgba`, `hsl`, or `indigo`/`ink`/…) |
| `tint(color, a)` | translucent overlay tint |
| `gradient_bg(a, b)` `mesh(...)` `radial(...)` `aurora(...)` | gradients |
| `gradient_text(a, b)` `gradient_border(a, b, w)` | gradient text / edge |
| `glass()` `frost(px)` | frosted glass |

## Typography

| Mod | Effect |
|-----|--------|
| `font(n)` `font_fluid(min, max)` | size (fixed / responsive) |
| `bold` `semibold` `medium` `italic` `underline` `strike` | weight/style |
| `heading` `subtitle` `body` `caption` `label` | type scale |
| `leading(x)` `tracking(x)` | line-height / letter-spacing |
| `uppercase` `lowercase` `capitalize` | case |
| `tabular_nums` `mono` | figures / monospace |
| `truncate` `line_clamp(n)` | ellipsis (1 line / n lines) |
| `text_align(...)` | alignment |

## Effects & motion

| Mod | Effect |
|-----|--------|
| `shadow()` `elevation(n)` `glow(color, r)` `ring(color)` | depth |
| `opacity(x)` `blur(px)` `backdrop_blur(px)` | filters |
| `scale(f)` `rotate(deg)` `translate(x, y)` | transforms |
| `transition(spec)` | animate property changes |
| `fade_in` `fade_up` `slide_in` `pop_in` `spin` `pulse` `float_` `breathe` | entrance / loop |
| `custom_animation(name, spec, ms)` | your own keyframe |
| `hover_lift(px)` `hover_glow(...)` `press()` `interactive()` | interaction feedback |
| `group()` + `group_hidden()` | reveal a child on parent hover |
| `ripple(color)` | material click ripple |

## Interaction & events

| Mod | Effect |
|-----|--------|
| `tap(Msg)` `optimistic()` | click → message (+ instant busy state) |
| `on_input(fn)` `on_change(fn)` | field value → message |
| `on(State, mods…)` | style on `Hover`/`Focus`/`Active`/`Disabled` |
| `on(event, Msg)` | wire ANY DOM event |
| `on_key("Enter", Msg)` `on_enter` `on_escape` `on_keydown(fn)` | keyboard (focused) |
| `on_shortcut("mod+k", Msg)` `hotkey(k, Msg)` | GLOBAL shortcut (no focus) |
| `on_hover(enter, leave)` `on_focus` `on_blur` | pointer / focus |
| `draggable(id)` `on_drop(fn)` | drag & drop |
| `on_submit(fn)` | form submit → gathered fields |

## Accessibility & semantics

| Mod | Effect |
|-----|--------|
| `as("nav")` `as_main` `as_header` … | semantic element (SEO/a11y) |
| `heading_level(n)` | h1–h6 |
| `role("...")` `aria(k, v)` `aria_label("...")` | ARIA |
| `sr_only` | visually hidden, read by screen readers |
| `live_region(assertive)` `status(text)` `alert(text)` | announce dynamic changes to screen readers |
| `autofocus()` `focus_ring(color)` `focus_within(mods…)` | focus management |
| `tab_index(n)` `focusable()` | keyboard focusability |

## Forms

| Mod / builder | Effect |
|---------------|--------|
| `input(v)` `textarea(v)` `checkbox(on)` `radio(g, v)` `select(opts)` | controls |
| `name("...")` `placeholder("...")` `type("...")` `checked(b)` `disabled(b)` | attributes |
| `form(fields…) | on_submit(fn)` | a real form; `FormData::parse` reads it |

## The escape hatches (rarely needed)

| Mod | Effect |
|-----|--------|
| `css("prop", "value")` | any CSS property — interned + diffed |
| `attr("k", "v")` | any HTML attribute |
| `var("--name", "value")` | a CSS custom property |
| `markup("<svg…>")` | raw trusted HTML |

If you find yourself reaching for `css()` for something common, it's probably
already a named mod — check this page first.

## Components & patterns (`#include <waya/ui.hpp>`)

| Call | What |
|------|------|
| `card(…)` `badge(l, tone)` `avatar("AB")` `divider()` `spinner()` | base parts |
| `button(l, Msg{}, Variant::…)` `toggle(on, Msg{})` `slider(v,min,max,fn)` | controls |
| `tabs(i, {{0,"A"}}, to_msg)` `progress(pct)` `tooltip(node, "tip")` | widgets |
| `sparkline(data)` `area_chart(data)` `bars(data)` | charts |
| `icon("search")` | inline SVG icon (tint with `fg`, size with `size`) |
| **`page_header(t, sub, actions…)`** `section(h, …)` `nav_bar(brand, …)` | structure |
| **`hero_section(head, sub, …)`** `feature_card(ico, t, body)` | marketing |
| **`sidebar_shell(brand, {items}, content)`** `sidebar_item(ico, l, active, Msg)` | app shell |
| **`stat(l, v, delta)`** `metric_card(…)` `list_row(lead, t, sub, trail)` `key_value(k, v)` | data |
| `tag("x")` `kbd("⌘")` `banner(msg, tone)` `empty_state(t, hint)` `code_block(c, lang)` | chrome |
| **`text_field(l, v, fn)`** `email_field` `password_field` `textarea_field` `select_field` `file_field` | form fields |
| `switch_field(t, desc, on, Msg)` `checkbox_field(l, on, Msg)` `form_actions(…)` | form fields |
| `confirm_dialog(open, t, msg, "OK", Yes{}, No{})` `dialog(open, Close{}, …)` | dialogs |

## Data, forms & navigation widgets (`waya/ui.hpp`)

| Call | What |
|------|------|
| `data_table<Row>(rows, cols)` | sortable / filterable / paginated table |
| `data_grid<Row>(rows, cols, ts, edit, …)` | editable spreadsheet (click-to-edit cells) |
| `stepper(v, fn, min, max, step)` `number_field(…)` `percent_field(…)` | numeric inputs |
| `star_rating(v, fn)` `stars(v)` | rating (editable / read-only) |
| `color_field(l, v, fn)` `swatch_picker(v, palette, fn)` | colour pickers |
| `segmented(active, labels, fn)` `breadcrumb({crumb(…)})` | nav controls |
| `tree_view(…)` `file_tree(root, ts, sel, onToggle, onSelect)` | trees / file explorer |
| `kanban(cols, renderCard, onMove)` | drag-and-drop board |
| `command_palette(…)` `spotlight(items, …)` | Cmd+K fuzzy launcher |
| `Toasts` + `toasts_layer(q, onDismiss[, onAction])` | notification queue (+ Undo button) |

## Effects, state & persistence (`Cmd` / `Sub`)

| Call | What |
|------|------|
| `Cmd::none()` `Cmd::emit(Msg)` `Cmd::batch(…)` | do nothing / send a msg now / several |
| `Cmd::after(ms, Msg)` `Cmd::task(fn)` | delayed msg / background work (off the pool) |
| `Cmd::fetch(url, on_done)` `Cmd::post(…)` | HTTP; result → msg |
| `Cmd::navigate(url)` `Cmd::set_title(t)` `Cmd::scroll_to(id)` `Cmd::focus(id)` | browser control |
| `Cmd::store(key, val)` `Cmd::store_clear(key)` | persist to `localStorage` |
| `Cmd::copy(text)` `Cmd::download(name, data, mime)` | clipboard / file |
| `Cmd::broadcast(topic, payload)` | multiplayer publish |
| `Sub::every(ms, Msg)` `Sub::on_route(fn)` `Sub::on_viewport(fn)` | timers / route / display |
| `Sub::on_storage(fn)` | replay persisted keys on connect (pair with `Cmd::store`) |
| `Sub::on_topic(topic, fn)` | multiplayer subscribe |

## Reusable widgets & testing

| Call | What |
|------|------|
| `map_msg<Child>(node, f)` | lift a child widget's view messages into the parent's `Msg` |
| `embed_update(m, &Model::field, child_update, msg, f)` | run a child widget's update in the parent |
| `cmd.map(f)` `sub.map(f)` | lift a child's commands / subscriptions |
| `test::harness<P>()` | drive a whole app in a test (`click`/`fill`/`send`, no server) |
| `test::widget_harness(state, &W::view, &W::update)` | drive one widget in a test |

## Spacing scale (`waya/ui/space.hpp`)

4px design-token scale so an app reads on a consistent rhythm. `sp(4)` == 16px.

| Mod | Effect | `p(4)` = |
|-----|--------|----------|
| `p(n)` `px_(n)` `py(n)` | padding: all / inline / block | `pad(16)` |
| `gx(n)` | gap between children | `gap(16)` |
| `ma(n)` `mt(n)` `mb(n)` | margin: all / top / bottom | `margin(16)` |

## Conditional rendering

| Call | What |
|------|------|
| `when(cond, [&]{ return node; })` | node when true, **nothing** (no gap) when false; lazy |
| `when(cond, node)` / `show(cond, node)` | eager form |
| `nothing()` | the empty, zero-space node directly |
| `screens(id, { {A, [&]{…}}, … })` | render the screen for the current route id |
