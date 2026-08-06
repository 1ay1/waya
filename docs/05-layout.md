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
stack(...)   // overlay — all children share one cell, centered
```

Control them with `gap`, `justify`, `align`, `wrap`, and the named shortcuts
`center`, `between`, `wrap` (see [Styling → Flex & alignment](04-styling.md#flex-alignment)).

```cpp
row(icon, text("Label")) | gap(8)
col(header, body, footer) | gap(20)
stack(image, caption | align(Align::end))
```

## Sizing intent: fill, hug, grow

Every node either **hugs** its content (the default) or **fills** available
space. Say which:

```cpp
text("A") | w(hug)         // content-sized (default)
box(...)  | w(fill)        // 100% of the parent's width
child     | grow(1)        // take all leftover space on the main axis
```

- `hug` (the default) — the node is as small as its content.
- `fill` — the node's width/height is 100% of its parent.
- `grow(n)` — in a `row`/`col`, this child absorbs leftover main-axis space,
  proportional to `n`.

```cpp
row(sidebar, content | grow(1))    // content takes the remaining width
```

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
