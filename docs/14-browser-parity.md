# Browser Parity

waya's mod vocabulary covers **everything HTML and CSS let you do** — not just
the common 80%. This page is the lookup table: whatever you'd reach for in CSS,
there's a named mod for it, so you rarely touch the `css()` escape hatch.

!!! tip "The escape hatches always exist"
    Anything not listed is reachable with `css(prop, value)` (any CSS property),
    `attr(name, value)` (any HTML attribute), `as("tag")` (any element), and
    `markup(html)` (arbitrary trusted HTML). But you should almost never need
    them — the named mods below are the browser's full surface.

## Layout — Flexbox

| CSS | waya |
|---|---|
| `flex-direction: row/column` | `row(...)` / `col(...)`, or `horizontal` / `column` |
| `flex-wrap: wrap` | `wrap` |
| `justify-content` | `justify(...)`, `center`, `between` |
| `align-items` | `align(...)`, `center` |
| `align-content` | `content_start/center/end/between/around/evenly` |
| `align-self` | `self_start/self_center/self_end/self_stretch/self_baseline` |
| `flex-grow` / `flex-shrink` | `grow(n)` / `shrink(n)` |
| `flex-basis` | `basis(len)` |
| `flex: 1` | `flex_1` |
| `order` | `order(n)` |
| `gap` / `row-gap` / `column-gap` | `gap(n)` / `row_gap(n)` / `col_gap(n)` |
| spacer | `push()` / `spacer()` |

## Layout — Grid

| CSS | waya |
|---|---|
| `display: grid` + columns | `grid(min)`, `grid_cols(n)`, `grid_template("...")`, `columns(n, ...)` |
| `grid-column` / `grid-row` | `col_line("1 / 3")` / `row_line("2 / span 2")` |
| `grid-column: span n` | `col_span(n)` / `row_span_(n)` |
| `grid-area` | `area("name")` |
| `place-self` / `justify-self` | `place_self("...")` / `justify_self_start/center/end/stretch` |
| `grid-auto-rows/columns/flow` | `auto_rows("...")` / `auto_cols("...")` / `auto_flow("dense")` |

## Sizing & box model

| CSS | waya |
|---|---|
| `width` / `height` | `w(len)` / `h(len)`, `size(len)`, `w_full`, `w_half`, `w_screen` |
| `min/max-width/height` | `min_w` / `max_w` / `min_h` / `max_h` |
| `padding` | `pad`, `pad_x`, `pad_y`, `pad_fluid` |
| `margin` | `margin(...)`, `mt/mb` (ui) |
| `border` / sides | `border(w, c)`, `border_top/right/bottom/left` |
| `border-radius` | `round(...)`, `pill`, `circle` |
| `box-sizing` | `box_sizing("content-box")` |
| `aspect-ratio` | `aspect(ratio)` |
| `object-fit` | `cover`, `contain`, `fit("...")` |
| `object-position` | `object_pos("...")` |

## Display, visibility, containment

| CSS | waya |
|---|---|
| `display: inline-block / inline-flex / block / contents` | `inline_block` / `inline_flex` / `block` / `contents` |
| `visibility` | `visible` / `invisible` |
| `isolation: isolate` | `isolate` |
| `content-visibility` | `content_visibility("auto")` |
| `will-change` | `will_change("transform")` |
| `contain` | `contain_("layout paint")` |
| `appearance: none` | `appearance_none` |
| `overflow` | `scroll`, `clip`, `overflow("...")`, `scroll_fill()` |

## Position & layering

| CSS | waya |
|---|---|
| `position: absolute/fixed/sticky/relative` | `absolute(...)` / `fixed` / `sticky` / `relative` |
| `inset` / `top/right/bottom/left` | `inset(...)`, `pin()`, `top/right/bottom/left` |
| `z-index` | `z(n)` |

## Interaction

| CSS | waya |
|---|---|
| `pointer-events` | `pointer_events("...")`, `no_pointer` |
| `touch-action` | `touch_action("pan-y")` |
| `user-select` | `user_select("...")`, `no_select`, `select_all` |
| `cursor` | `pointer`, `cursor_grab/grabbing/text/move/wait/help/disabled`, `cursor_("...")` |
| `resize` | `resize("vertical")` |
| `accent-color` | `accent(0x…)` |
| `caret-color` | `caret(0x…)` |

## Scroll

| CSS | waya |
|---|---|
| `scroll-snap-type` | `scroll_snap_x` / `scroll_snap_y` |
| `scroll-snap-align` | `snap_start/snap_center/snap_end` |
| `scroll-behavior: smooth` | `smooth_scroll` |
| `overscroll-behavior` | `overscroll_("contain")` |
| `scrollbar-gutter` | `scrollbar_gutter("stable")` |
| `scroll-padding` / `scroll-margin` | `scroll_padding(px)` / `scroll_margin` |

## Transforms

| CSS | waya |
|---|---|
| `translate` | `translate(x,y)`, `translate_x/y/z(px)` |
| `rotate` | `rotate(deg)`, `rotate_x/y/z(deg)` |
| `scale` | `scale(f)`, `scale_xy(sx,sy)` |
| `skew` | `skew(x,y)` |
| custom | `transform("...")` |
| `transform-origin` | `transform_origin("top left")` |
| `perspective` | `perspective(px)` |
| `transform-style: preserve-3d` | `preserve_3d` |
| `backface-visibility: hidden` | `backface_hidden` |

## Visual effects

| CSS | waya |
|---|---|
| `box-shadow` | `shadow(...)`, `elevation(n)`, `inset_shadow(...)`, `ring_at(...)` |
| `opacity` | `opacity(f)` |
| `filter` | `blur`, `brightness`, `contrast`, `saturate`, `sepia`, `filter("...")` |
| `backdrop-filter` | `backdrop_blur(px)`, `frost(...)` |
| `clip-path` | `clip_path("circle(50%)")` |
| `mask` | `mask_("...")` |
| `mix-blend-mode` | `mix_blend("screen")` |
| `background-blend-mode` | `bg_blend("...")` |
| `outline` / `outline-offset` | `outline(w,c)` / `outline_offset(px)` |
| gradients | `gradient(a,b)`, `linear_gradient("...")`, `radial_gradient("...")`, `conic_gradient("...")`, `aurora(...)`, `mesh(...)` |

## Colour & typography

| CSS | waya |
|---|---|
| `color` / `background` | `fg(0x…)` / `bg(0x…)` + gradients above |
| `font-size` | `font(px)`, `font_fluid(min,max)` |
| `font-weight` | `weight(...)`, `bold`, `semibold`, `medium` |
| `font-family` | `font_family("...")`, `mono` |
| `font-style: italic` | `italic` |
| `text-decoration` | `underline`, `strike`, `text_decoration("...")` |
| `text-align` | `text_align(...)`, `text_left/center/right` |
| `line-height` | `leading(x)` |
| `letter-spacing` | `tracking(px)`, `tracking_em(em)` |
| `word-spacing` | `word_spacing(px)` |
| `text-transform` | `uppercase`, `lowercase`, `capitalize` |
| `text-shadow` | `text_shadow("...")` |
| `text-indent` | `text_indent(px)` |
| `white-space` | `white_space("pre-wrap")`, `nowrap_text` |
| `word-break` / `overflow-wrap` | `word_break("...")` / `overflow_wrap("...")`, `break_word` |
| `text-overflow: ellipsis` | `truncate` |
| `hyphens` | `hyphens("auto")` |
| `vertical-align` | `vertical_align("middle")` |
| `writing-mode` | `writing_mode("vertical-rl")` |
| `column-count` | `text_columns(n)` |
| `text-wrap: balance/pretty` | `text_wrap("balance")` |
| `list-style` | `list_style("none")` |
| tabular figures | `tabular_nums` |

## Animation & transition

| CSS | waya |
|---|---|
| `transition` | `transition(...)` |
| `@keyframes` presets | `fade_in`, `fade_up/down`, `slide_in`, `pop_in`, `spin`, `pulse`, `bounce`, `ping`, `shimmer`, `breathe`, `float_` |
| `animation-delay` | `delay(ms)` |
| custom keyframes | `animate(...)`, `custom_animation(...)`, register with `assets().css("@keyframes …")` |

## States & responsive

| CSS | waya |
|---|---|
| `:hover` / `:focus` / `:active` / `:disabled` | `on(Hover, …)` / `on(Focus, …)` / `on(Active, …)` / `on(Disabled, …)` |
| `:focus-within` | `focus_within(...)` |
| `::selection`, scrollbars, etc. | `assets().css("...")` (document-level) |
| media queries | `at(Break, …)` / `below(Break, …)`, `hide_below/hide_above` |

## Forms — every native control

| HTML | waya builder |
|---|---|
| `<input type=text>` | `input(v)` |
| `<input type=number>` | `number_input(v)` / `number_input(v,min,max,step)` |
| `<input type=range>` | `range_input(v,min,max,step)` |
| `<input type=date/time/datetime-local/month/week>` | `date_input` / `time_input` / `datetime_input` / `month_input` / `week_input` |
| `<input type=color>` | `color_input(v)` |
| `<input type=password/email/tel/url/search>` | `password_input` / `email_input` / `tel_input` / `url_input` / `search_input` |
| `<input type=file>` | `file_input(multiple, accept)` |
| `<input type=hidden>` | `hidden_input(name, value)` |
| `<input type=checkbox>` / `<input type=radio>` | `checkbox(on)` / `radio(group, value, on)` |
| `<textarea>` | `textarea(v)` |
| `<select>` / `<option>` | `select(opts, chosen)` / `option(value, label)` |
| `<optgroup>` | `option_group(label, opts)` |
| `<datalist>` autocomplete | `with_list(id, input, suggestions)` |
| `<progress>` | `progress_el(value, max)` |
| `<meter>` | `meter_el(value, min, max)` |
| `<fieldset>` / `<legend>` | `fieldset(legend, fields…)` |
| `<label for=…>` | `label_for(text, id)` |
| `<button>` | `button(label)` |
| `<form>` | `form(fields…)` + `on_submit(...)` |

### Input attributes & validation

| HTML attribute | waya mod |
|---|---|
| `required` / `readonly` | `required()` / `readonly()` |
| `min` / `max` / `step` | `min_val(v)` / `max_val(v)` / `step_by(v)` / `step_any()` |
| `pattern` | `pattern("[0-9]{3}")` |
| `maxlength` / `minlength` | `maxlength(n)` / `minlength(n)` |
| `inputmode` | `inputmode("numeric")` |
| `enterkeyhint` | `enterkey("send")` |
| `autocomplete` | `autocomplete("email")` |
| `spellcheck` / `autocapitalize` | `spellcheck(bool)` / `autocapitalize("none")` |
| `rows` / `cols` / `size` | `rows(n)` / `cols(n)` / `size_attr(n)` |
| `multiple` / `accept` / `capture` | `allow_multiple()` / `accepts("image/*")` / `capture("user")` |
| `id` / `value` / `form` / `list` | `id(...)` / `default_value(...)` / `form_id(...)` / `list_id(...)` |
| `title` (validation msg) | `title_hint(...)` |

### Input & element events

| DOM event | waya mod |
|---|---|
| `input` / `change` | `on_input(fn)` / `on_change(fn)` |
| `keydown` (Enter/Escape/any) | `on_enter` / `on_escape` / `on_key(k, ...)` / `on_keydown(fn)` |
| `keyup` / `beforeinput` | `on_keyup(fn)` / `on_beforeinput(Msg)` |
| `focus` / `blur` | `on_focus` / `on_blur` |
| `invalid` (validation) | `on_invalid(Msg)` |
| `search` | `on_search(fn)` |
| `paste` / `copy` / `cut` | `on_paste` / `on_copy` / `on_cut` |
| `select` (text) | `on_select_text(Msg)` |
| `wheel` / `scroll` | `on_wheel(Msg)` / `on_scroll(Msg)` |
| `contextmenu` | `on_context(Msg)` |
| `dblclick` | `on_double(Msg)` |
| pointer enter/leave, hover | `on_enter_pointer` / `on_leave_pointer` / `on_hover(a, b)` |
| drag & drop | `draggable` / `on_drop` / `drop_target` |
| any other DOM event | `on("eventname", Msg)` / `on_ev("eventname", fn)` — the client auto-delegates ANY `data-ev-*` |

## Media & graphics

| HTML/SVG | waya |
|---|---|
| `<img>` | `image(src)` |
| `<video>` / `<audio>` | `video(src)` / `audio(src)` |
| `<svg>` polyline/polygon | `path(points, closed)` + `stroke`/`fill` |
| arbitrary SVG/embed | `markup("<svg…>")` |

## Semantics & accessibility

| HTML | waya |
|---|---|
| `<main>/<nav>/<header>/<footer>/<article>/<section>/<aside>` | `as_main` / `as_nav` / `as_header` / `as_footer` / `as_article` / `as_section` / `as_aside` |
| any element | `as("tag")` |
| ARIA roles/attributes | `role(...)`, `aria(k,v)`, `aria_label(...)` |
| `title` / `alt` | `title(...)` / `alt(...)` |
| `tabindex` | `tab_index(n)`, `focusable()` |
| screen-reader-only | `sr_only` |

---

If you ever hit something genuinely not covered, `css()` / `attr()` / `as()` /
`markup()` are always there. But the point of this page is: you almost never
will.
