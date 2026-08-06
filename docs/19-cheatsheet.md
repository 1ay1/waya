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
