# The Vocabulary

The entire surface language is a handful of node **primitives** plus form
**controls**. Everything you can build is a composition of these. This chapter
covers *what* each node is; the [next chapter](04-styling.md) covers the mods
that shape them.

All builders live in `namespace waya::surface` and return a `NodeRef`.

## `box` — the container

```cpp
template <typename... Cs> NodeRef box(Cs... children);
```

A generic container. On its own it lays nothing out in a particular direction;
you give it a flow with a mod, or use `row`/`col`/`stack` which are boxes with
a flow preset.

```cpp
box(text("a"), text("b"))                 // a plain container
box(child) | pad(20) | round(16) | bg(0x1e293b)   // a card
```

`box()` with no children is a useful primitive on its own — a coloured/sized
rectangle (a divider, a dot, a bar):

```cpp
box() | size(8) | round(999) | bg(0x34d399)   // a status dot
```

## `row`, `col`, `stack` — boxes with a flow

```cpp
template <typename... Cs> NodeRef row(Cs...);    // horizontal
template <typename... Cs> NodeRef col(Cs...);    // vertical
template <typename... Cs> NodeRef stack(Cs...);  // overlay (all children share one cell)
```

- **`row`** lays children left-to-right and vertically centers them by default.
- **`col`** lays children top-to-bottom.
- **`stack`** overlays every child in the same cell, centered — the way you
  build a badge on an avatar, text over an image, or a spinner over content.

```cpp
row(icon, text("Label")) | gap(8)            // icon + text
col(header, body, footer) | gap(16)          // a page
stack(avatar, badge | align_self_end)        // overlay a badge
```

`gap(n)` sets the spacing between children; see [Layout](05-layout.md) for
alignment, wrapping, and the responsive containers (`grid`, `cluster`,
`switcher`, `sidebar`).

## `text` — a run of text

```cpp
NodeRef text(std::string s);
NodeRef text(int v);          // convenience: renders the number
NodeRef text(long long v);
```

A text node. Style it with `fg`, `font`, `weight`, `italic`, `leading`, etc.

```cpp
text("Heading") | font(28) | weight(Weight::bold)
text(m.count)   | fg(0x22d3ee)        // an int, directly
```

!!! note "Text is always escaped"
    User text is HTML-escaped automatically. `text("<script>")` renders the
    literal characters, never a tag. To inject trusted HTML, use `markup`
    (below) — and only with content you control.

## `image` — a picture

```cpp
NodeRef image(std::string src);
```

An image by URL. Size it with `w`/`h`/`size`, crop with `cover`, round with
`round`.

```cpp
image("/photo.jpg") | w(pct(100)) | h(px(200)) | round(12) | cover
```

## `path` — vector graphics

```cpp
struct Pt { float x, y; };
NodeRef path(std::vector<Pt> pts, bool closed = false);
```

A polyline / polygon from a list of points — the primitive for charts,
sparklines, and diagrams. Stroke it and/or fill it:

```cpp
std::vector<Pt> pts = { {0,40}, {20,10}, {40,25}, {60,5}, {80,20} };
path(pts) | stroke(0x22d3ee, 2)                 // a line chart
path(pts, /*closed=*/true) | fill(0x6366f1)     // a filled polygon
```

`stroke(color, width)` and `fill(color)` are mods (see [Styling](04-styling.md)).
Because it's one node, you can draw a 5,000-point chart and it diffs like any
other node.

### `scene` — vector drawing

`path()` draws one polyline. For anything richer — a chart with axes and labels,
a sprite, a game board, an animated backdrop — use `scene`, the vector
vocabulary. Shapes are values painted with a fluent chain; the whole thing
renders to one `<svg>` and diffs like any subtree.

```cpp
scene(400, 200,                                     // coordinate space (viewBox)
    vrect(0, 0, 400, 200).fill(0x0b1020),
    vline(0, 100, 400, 100).stroke(0x22d3ee, 2).dashed(),
    vcircle(200, 100, 40).fill(rgba(0x6366f1, 0.8f)),
    vtext(200, 105, "score").fill(0xffffff).anchor_mid().bold())
| w_full | h(200)                                   // the node sizes like any box
```

Builders: `vrect(x,y,w,h,r=0)`, `vcircle(cx,cy,r)`, `vellipse`, `vline`,
`vpolyline(pts)` / `vpolygon(pts)`, `vpath(d)` (raw SVG path data), `vtext(x,y,s)`,
and `vgroup(shapes…)` (a group one `.transform()`/`.opacity()` covers). Paint is
a chain — `.fill()`, `.stroke(color, w)`, `.opacity()`, `.dashed()`, `.round_cap()`,
`.anchor_mid()`, `.font_px()`, `.bold()`, `.mono()`, `.transform()`.

Two things matter: **text is escaped** (a `vtext("a<b")` can't break your markup
or inject anything — unlike a hand-built `"<text>"` string), and the scene
inherits `currentColor`, so `scene(…) | fg(0x22d3ee)` recolours every shape that
didn't set its own fill. This is the framework's promise — *you never write
markup by hand* — finally extended to pixels. `bars()`, and the matrix rain in
the examples, are built on it.

## Form controls

waya has a complete set of real, accessible form controls. Each is a node you
style like any other; you wire its value change to a message.

### `input` — single-line text

```cpp
NodeRef input(std::string value = {});
```

```cpp
input(m.query)
    | placeholder("Search…")
    | on_input([](std::string v){ return SetQuery{v}; })   // fires on each keystroke
    | on_enter(Submit{});                                  // fires on Enter
```

The `value` you pass is the field's current value — drive it from your model so
the field is a pure function of state.

### `textarea` — multi-line text

```cpp
NodeRef textarea(std::string value = {});
```

Same `on_input`/`on_change` flow as `input`. Set rows with `attr("rows","6")`.

### `checkbox` — a boolean

```cpp
NodeRef checkbox(bool on = false);
```

```cpp
checkbox(m.agreed) | on_change([](std::string v){ return SetAgreed{ v == "true" }; })
```

`on_change` fires with the string `"true"` or `"false"`.

### `radio` — one choice in a group

```cpp
NodeRef radio(std::string group, std::string value, bool on = false);
```

```cpp
radio("plan", "free", m.plan == "free") | on_change([](std::string v){ return SetPlan{v}; })
radio("plan", "pro",  m.plan == "pro")  | on_change([](std::string v){ return SetPlan{v}; })
```

`on_change` fires with this option's `value`.

### `select` — a dropdown

```cpp
struct Opt { std::string value, label; };
Opt option(std::string value, std::string label = {});   // label defaults to value
NodeRef select(std::vector<Opt> options, std::string chosen = {});
```

```cpp
select({ option("us","United States"), option("de","Germany") }, m.country)
    | on_change([](std::string v){ return SetCountry{v}; });
```

`on_change` fires with the chosen option's `value`.

### `button` — a real `<button>`

```cpp
NodeRef button(std::string label);
```

A keyboard-focusable, screen-reader-announced button. Pair with `tap(msg)`.
Prefer this over a tappable `box`/`text` when the target is genuinely a button.

```cpp
button("Submit") | bg(0x6366f1) | fg(0xffffff) | round(11) | pad_x(18) | pad_y(11) | tap(Submit{})
```

### `form` — grouped named controls

```cpp
template <typename... Cs> NodeRef form(Cs... fields);
```

A real `<form>`. Give inner controls a `name(...)`, and `on_submit` gathers
them all into one value string (`"name=value&name2=value2"`) when the form is
submitted (Enter in a field, or a button inside it):

```cpp
form(
    input() | name("email") | placeholder("Email"),
    input() | name("pw") | type("password") | placeholder("Password"),
    button("Sign in")
) | on_submit([](std::string body){ return SignIn{ body }; });
```

See [Events & Inputs](07-events.md) for the full event story.

## Media & capability elements

These are the elements with real browser *behaviour* you can't build from a box:
playback, embeds, vector art, drawing. All first-class and one-line simple
(from `<waya/surface/media.hpp>`, included by the umbrella).

### Video & audio

```cpp
NodeRef video(std::string src);   // a <video> with controls
NodeRef audio(std::string src);   // an <audio> player
```

Option mods make them usable without touching `attr`:

```cpp
video("/hero.mp4") | autoplay() | loop_media() | silent() | plays_inline() | no_controls()
video("/clip.mp4") | poster("/still.jpg") | preload("metadata")
```

`autoplay()` · `loop_media()` · `silent()` (muted — named so it doesn't clash with
the `muted` colour) · `no_controls()` · `poster(url)` · `preload(how)` ·
`plays_inline()`.

### Embeds — any iframe, made safe

```cpp
NodeRef embed(std::string url, std::string title = "Embedded content");
NodeRef youtube(std::string video_id, bool autoplay = false);
NodeRef vimeo(std::string video_id);
NodeRef google_map(std::string query);
```

`embed` renders a **sandboxed** `<iframe>` (it can't script your page) and
scheme-sanitises the URL. The common providers are one call — just the id or a
query. Wrap in `video_box(...)` for a responsive 16:9 frame.

```cpp
video_box(youtube("dQw4w9WgXcQ"))          // responsive YouTube player
google_map("Golden Gate Bridge")            // a live map from a query
embed("https://example.com/widget")         // any embeddable page
```

!!! note "Embeds are for trusted providers"
    `embed` is sandboxed but grants `allow-scripts allow-same-origin` (needed by
    YouTube/Maps). Use it for known providers, not arbitrary user-supplied URLs.

### Inline SVG

```cpp
NodeRef svg(std::string inner, std::string view_box = "0 0 24 24");
NodeRef svg_raw(std::string full_svg);   // a complete exported <svg>
```

Vector art beyond a single `path`. Pass the shapes; the `<svg>` + viewBox is
provided. Shapes using `fill="currentColor"` pick up `fg(...)`; size with `size(...)`.

```cpp
svg("<circle cx='12' cy='12' r='10'/>") | fg(0xff5c8a) | size(px(64))
```

### Canvas

```cpp
NodeRef canvas(int w = 300, int h = 150);
```

A real `<canvas>` drawing surface. Give it an `id(...)` and drive it from a
client script, or use it as a paint target; `w`/`h` are the buffer resolution
(it scales to its CSS box).

### Responsive images

```cpp
NodeRef picture(std::string fallback_src,
                std::vector<std::pair<std::string,std::string>> sources = {},
                std::string alt_text = "");
```

An `<img>` with per-media-query sources (art direction / resolution) and a
graceful fallback everywhere.

```cpp
picture("/photo.jpg", { {"(max-width:600px)", "/photo-small.jpg"} }, "A photo")
```

## `markup` — the raw-HTML escape hatch

```cpp
NodeRef markup(std::string html);
```

Injects **trusted** raw HTML — for rich text you generate, an embedded SVG
icon, or a third-party embed. This is the one primitive that is **not**
auto-escaped.

!!! danger "Never pass user input to `markup`"
    `markup` bypasses escaping. Passing untrusted content to it is an XSS hole.
    Use it only with HTML you construct yourself.

```cpp
markup("<svg viewBox='0 0 24 24'>…</svg>") | font(20)
markup(render_markdown_to_html(post.body))   // trusted, generated by you
```

## Semantic elements

By default a `box` renders as a `<div>`. To emit a semantic landmark for
accessibility and SEO, tag it with `as(...)` or one of the named mods:

```cpp
header | as_header       // <header>
nav    | as_nav          // <nav>
main   | as_main         // <main>
col(...) | as("section") // any tag
```

See [Styling → Semantics & attributes](04-styling.md#semantics-attributes-and-accessibility).

## The `Kind` enum

Every node has a `Kind` (you rarely reference it directly):

```
box, text, image, path,
input, textarea, checkbox, radio, select, button, form,
video, audio, markup
```

That's the whole vocabulary. Next: [Styling](04-styling.md) — the mods that
turn these primitives into a finished UI.
