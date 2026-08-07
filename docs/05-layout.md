# Layout

waya's layout system is **intrinsically responsive**: containers adapt to the
space available on their own, with no media-query breakpoints. It combines two
proven ideas —

- **SwiftUI's sizing intent** — a node either *hugs* its content or *fills* the
  available space, and you say which directly; and
- **Every Layout's primitives** — composable containers (`cluster`, `switcher`,
  `sidebar`, `grid`) that reflow by themselves as space changes.

Everything here is just a node with the right mods, so it composes with the
whole vocabulary.

## The three flow boxes

You've met these in [The Vocabulary](03-vocabulary.md):

```cpp
row(...)     // horizontal, cross-axis centered
col(...)     // vertical
stack(...)   // overlay — all children share one cell, each FILLING it
```

Control them with `gap`, `justify`, `align`, `wrap`, and the named shortcuts
`center`, `between`, `wrap` (see [Styling → Flex & alignment](04-styling.md#flex-alignment)).

```cpp
row(icon, text("Label")) | gap(8)
col(header, body, footer) | gap(20)
stack(backdrop, content)                    // layers: each fills the cell
stack(image, caption | align(Align::end))   // a child places itself in the cell
```

> `stack` is a ZStack — its children are overlaid in one grid cell and each
> **stretches to fill** it by default (the layering use-case: a backdrop behind
> content, an overlay over an image/canvas). A child that wants to be small and
> centred says so itself (a size, `center`, or absolute positioning).

## Sizing intent: fill, hug, grow

Every node either **hugs** its content (the default) or **fills** available
space. Say which:

```cpp
text("A") | w(hug)         // content-sized (default)
box(...)  | w(fill)        // 100% of the parent's width
child     | grows          // take all leftover space on the MAIN axis
```

- `hug` (the default) — the node is as small as its content.
- `fill` — the node's width/height is 100% of its parent.
- `grows` (or `grow(n)`) — in a `row`/`col`, this child absorbs leftover
  main-axis space. In a `col` it fills height; in a `row` it fills width.
- `stretches` — stretch to fill the parent's **cross** axis.

```cpp
row(sidebar, content | grows)    // content takes the remaining width
```

## Full-height app shells (the #1 layout question)

The most common layout confusion is "how do I make one region fill the leftover
height and scroll inside itself instead of pushing the page taller?" waya names
it directly — no `flex`/`min-height:0` incantations:

```cpp
template <typename... Cs> NodeRef viewport(Cs... children);  // a 100dvh flex-column shell
Mod flex_col;   Mod flex_row;   // a height/width-distributing flex container
Mod vscroll();  Mod hscroll();  // grow to fill AND scroll internally (y / x)
```

- **`viewport(...)`** — a root that fills exactly the viewport height and is a
  height-distributing flex column. Header/toolbar stay put; a `grows`/`vscroll`
  region fills the rest; the page never scrolls. The one-call app frame.
- **`flex_col` / `flex_row`** — turn any node into a flex container that
  distributes space to children *and* lets a scrolling child be bounded
  (`min-height:0` / `min-width:0`).
- **`vscroll()` / `hscroll()`** — mark the region that should grow and scroll
  internally (a list, a chat log; a board, a shelf).

```cpp
// a classic header + scrolling body + pinned footer, filling the screen:
viewport(
    top_bar,                        // stays put
    message_list | vscroll(),       // fills the middle, scrolls inside
    composer                        // pinned to the bottom
)
```

This is exactly how the `nova` example builds its board (a full-height shell with
scrolling lanes) — no raw CSS.

## Sizing intent details (legacy names)

`flexible` and `flex_1` are older aliases of `grows`; `scroll_fill()` is the
older alias of `vscroll()`. Prefer the new names — they read as intent.

## Spacers

A **spacer** is a flexible empty box that pushes siblings apart:

```cpp
NodeRef push();       // from sugar.hpp
NodeRef spacer();     // from layout.hpp — same thing
const Mod flexible;   // == grow(1), to make any node a spacer
```

```cpp
row(logo, push(), nav)                 // logo left, nav pushed right
row(a, push(), b, push(), c)           // three items, evenly gapped
```

## Conditional rendering

Show a node only when a condition holds. The hidden branch collapses to
`nothing()` — a `display:none` node that takes **zero layout space**, so it never
leaves a phantom gap between its siblings:

```cpp
col(
    header,
    when(m.loading, [&]{ return spinner(); }),   // built only when shown
    when(m.error.empty(), [&]{ return body; }),
    body_footer
) | gap(16)
```

- `when(cond, node)` — the node when true, nothing when false.
- `when(cond, [&]{ return node; })` — **lazy**: the builder runs only if shown
  (skip the work of building a hidden subtree).
- `when(cond, a, b)` — a or b.
- `show(cond, node)` — reads well for a visibility toggle; same as `when`.
- `nothing()` — the empty, zero-space node directly, when you need it.

> Because the hidden branch is `nothing()` (not an empty `box`), a `col`/`row`
> `gap` never adds space around a hidden conditional. `modal(open, …)`,
> `dialog(open, …)` and `screens(id, …)` (no match) collapse the same way.

For a screen SWITCH (routing), prefer the flat `screens` table over a `when`
ladder:

```cpp
screens(m.route, {
    { Home,   [&]{ return home_view(m); } },
    { Detail, [&]{ return detail_view(m); } },
    { NotFound, [&]{ return not_found(); } },   // catch-all
})
```

## Responsive containers

These are the heart of waya's "responsive without breakpoints" claim. Each
adapts to available width on its own.

### `cluster` — items that wrap

```cpp
template <typename... Cs> NodeRef cluster(Cs... items);
```

Items flow horizontally and **wrap** to new lines when they run out of room —
tags, chips, button groups, nav pills. Centered, 12px gap by default.

```cpp
cluster(tag("rust"), tag("c++"), tag("simd"), tag("gpu"))
```

### `auto_grid` — a responsive card grid

```cpp
Mod auto_grid(float min_px);
```

Turns a container into a grid with as many equal columns as fit at ≥ `min_px`
wide, reflowing automatically. The one-liner responsive card grid (auto-fit +
minmax), and it never overflows on a narrow phone.

```cpp
grid(card1, card2, card3, card4) | auto_grid(320)   // 1–4 columns by width
// or apply it to any container:
box(card1, card2, card3) | auto_grid(320) | gap(16)
```

### `switcher` — row that becomes a column

```cpp
template <typename... Cs> NodeRef switcher(Len threshold, Cs... items);
```

Lays items in a row when there's room; **flips to a column** below `threshold`
total width. No media query — it's pure flex math.

```cpp
switcher(rem(40), left_panel, right_panel)   // side-by-side, then stacked
```

### `sidebar` — a sidebar + main that wraps

```cpp
NodeRef sidebar(NodeRef side, NodeRef main, Len side_w = rem(16));
```

A fixed-ish sidebar next to a fluid main area; the sidebar drops **below** the
main area when the viewport gets too narrow.

```cpp
sidebar(nav_column, article, rem(18))
```

### `center_col` — a centered reading column

```cpp
template <typename... Cs> NodeRef center_col(Cs... cs);
```

A column capped at a comfortable reading width (~760px) and centered on the
page. Perfect for article bodies.

```cpp
center_col(title, byline, article_body) | gap(20)
```

### `hero` — a viewport-height centered section

```cpp
template <typename... Cs> NodeRef hero(Cs... cs);
```

Fills the viewport height and vertically centers its content — landing hero
sections.

```cpp
hero(headline, subtitle, cta) | gap(24)
```

### `board` — a responsive horizontal-scroll track (kanban, carousel, shelf)

```cpp
template <typename... Cs> NodeRef board(Len item_w, Cs... items);
NodeRef board_(Len item_w, std::vector<NodeRef> items);   // vector form
```

A row of fixed-width items that **scrolls horizontally inside itself** and
**never widens the page**. Each item shrinks on small screens (clamped to the
viewport) and the track **snaps** item-to-item. This is the correct way to place
"columns side by side" without the classic *"4 fixed columns overflow the
phone"* bug — on desktop you see them all, on a phone you swipe.

```cpp
board(rem(18), column_a, column_b, column_c, column_d)
// or build the columns in a loop:
std::vector<NodeRef> cols;
for (auto& lane : lanes) cols.push_back(lane_view(lane));
board_(px(290), std::move(cols))
```

!!! tip "Board vs. grid"
    Use **`grid(min)`** / **`auto_grid`** when items should *reflow* (wrap to
    more rows as space allows) — a card gallery. Use **`board`** when items are
    columns that stay in one row and *scroll* — a kanban, a media carousel, a
    horizontally-paged shelf.

### `split` — two responsive panes

```cpp
NodeRef split(NodeRef a, NodeRef b, float ratio = 0.5f, Len stack_below = rem(48));
```

Two panes side by side, `a` taking `ratio` of the width (default half). They
**stack to a column** below `stack_below` total width — no media query. For an
editor + preview, a list + detail, a form + summary.

```cpp
split(editor_pane, preview_pane, 0.6f)   // 60/40, stacks on narrow screens
```

### `ratio_box` / `video_box` / `square_box` — aspect-ratio media

```cpp
NodeRef ratio_box(float w, float h, NodeRef child);   // locked to w:h
NodeRef video_box(NodeRef child);                     // 16:9 shorthand
NodeRef square_box(NodeRef child);                    // 1:1 shorthand
```

A container locked to an aspect ratio that clips its child to fit — a video
embed, a cover image, a map tile. The child fills it.

```cpp
video_box(image("/thumb.jpg") | cover)   // always 16:9, never distorts
square_box(avatar_img)
```

### `masonry` — a Pinterest-style packed grid

```cpp
template <typename... Cs> NodeRef masonry(Len min_col, Cs... items);
NodeRef masonry_(Len min_col, std::vector<NodeRef> items, float gap_px = 16);
```

Items flow into as many columns as fit at ≥ `min_col` wide, each column packed
independently so variable-height cards tile with no gaps. Pure CSS multicol — no
JS. (Column-major order.)

```cpp
masonry(rem(16), photo_card, photo_card, photo_card, /* … */)
```

### Centering

```cpp
Mod dead_center;   // perfectly centre children on BOTH axes (a spinner, splash)
Mod center_y;      // centre on the VERTICAL axis only
Mod center;        // (from styling) centre on both axes for a flex box
```

```cpp
box(spinner()) | dead_center | h(vh(100))   // a full-screen loading state
```

## Real grids (tables, dashboards, any 2-D layout)

Faking columns with per-row `row(grow(1)…)` never aligns — each row splits its
own free space independently. For content that must line up **down** the page
(tables, aligned card grids) or any true 2-D layout (dashboards, the holy-grail
sidebar/header/footer), use a real CSS grid via `grid()` and its mods:

```cpp
template <typename... Cs> NodeRef grid(Cs... cells);  // a grid container

Mod grid_cols(int n);                 // n equal columns
Mod grid_cols(std::string tracks);    // explicit tracks, e.g. "2fr 1fr 1fr"
Mod grid_rows(int n);                 // n equal rows
Mod grid_rows(std::string tracks);    // explicit row tracks
Mod grid_areas(std::string tmpl);     // named template areas
Mod auto_grid(float min_px);          // responsive auto-fit columns

Mod col_span(int n);                  // a child spans n columns
Mod row_span(int n);                  // a child spans n rows
Mod area(std::string name);           // place a child into a named area

template <typename... Cs> NodeRef columns(int n, Cs... cells);  // n-col grid box
```

Any of the `grid_*`/`auto_grid` mods turn their container into a grid — you
don't need `grid()` first — so `box(...) | grid_cols(3)` just works.

```cpp
// a 3-column table: header row + data rows, all columns aligned
columns(3,
    text("Name") | bold, text("Role") | bold, text("Since") | bold,
    text("Ada"),  text("Eng"),  text("2019"),
    text("Linus"),text("Ops"),  text("2021"))
| gap(8)

// unequal columns
grid(a, b) | grid_cols("240px 1fr")

// the holy-grail layout in two lines
grid(nav, header, main, footer)
    | grid_areas("'nav header' 'nav main' 'nav footer'")
    | grid_cols("200px 1fr") | grid_rows("auto 1fr auto")
// then place children:
//   nav    | area("nav")
//   header | area("header")   ...
```

## Scrollable regions

For a region that takes leftover space and scrolls *internally* (instead of
pushing siblings off-screen) — a chat log, a file list — use `scroll_fill()`
from `sugar.hpp`:

```cpp
col(
    header,
    message_list | scroll_fill(),   // grows, scrolls inside
    composer
) | h(vh(100))
```

Other scroll mods: `scroll` (both axes), `scroll_x` / `scroll_y` (one axis),
`clip` (hide overflow), and `no_scrollbar` (scroll works, chrome hidden — for
carousels).

## Positioning — no raw CSS needed

The everyday position patterns have named mods, so you never drop to
`css("position", …)` / `css("top", …)`:

```cpp
// a sticky header (the single most common one) — one word
header | sticky_top()          // sticks to the top of the scroll container
header | sticky_top(64)         // … 64px down (below a fixed bar)

// a badge pinned to a card corner
box(content, badge | pin_top_right(8)) | positioned()
//   positioned() makes the box the anchor; pin_* corners: pin_top_right,
//   pin_top_left, pin_bottom_right, pin_bottom_left.

// a full-screen fixed overlay
modal | fixed | pin()          // pin() = inset:0, fills the viewport

// individual edges + stacking
el | absolute() | top(0) | right(0) | z(50)
```

`sticky` / `fixed` / `absolute()` / `relative` set the mode; `top`/`bottom`/
`left`/`right` take a bare px number or a `Len` (`right(rem(1))`); `z(n)` sets
stacking order.

!!! tip "css() is the escape hatch, not the everyday tool"
    Sizing (`min_w`/`max_w`/`min_h`/`max_h` — all take a bare number *or* a
    `Len`), transforms (`translate`/`rotate`/`scale`), filters (`blur`/
    `backdrop_blur`), text (`line_clamp`/`uppercase`/`tabular_nums`) — all named.
    A normal view shouldn't contain `css("prop", "value")`; when you *do* reach
    for it (a genuinely one-off value like `min(80vw, 600px)`), it's still
    interned and diffed like any other mod. Nothing is off-limits, but the
    common 90% reads clean.

## Putting it together: a responsive app shell

```cpp
NodeRef view(const Model& m) {
    return col(
        // top bar
        row(logo, push(), nav_links) | between | center | pad(16) | as_nav,

        // body: sidebar + content that stacks on narrow screens
        sidebar(
            col(nav_items) | gap(6),
            center_col(page_content) | gap(20),
            rem(16)
        ) | grow(1) | pad(20),

        // footer
        row(copyright, push(), social) | pad(20) | as_footer
    ) | gap(0) | h(vh(100));
}
```

No breakpoints, no media queries in your code — every container above reflows
on its own. Reach for explicit breakpoints (`at`/`below`, see
[Styling](04-styling.md#responsive-breakpoints)) only for the rare case the
intrinsic primitives don't cover.

---

Next: [The Runtime](06-runtime.md) — the Elm loop that drives your app, and how
`init`/`update`/`view` fit together.
