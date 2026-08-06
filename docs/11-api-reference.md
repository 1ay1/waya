# API Reference

The complete, exhaustive listing of waya's public surface API. Everything is in
`namespace waya::surface`. Types marked *builder* return `NodeRef`; types marked
*mod* return `Mod` (apply with `|`).

!!! tip "How to read this"
    - **Builders** create nodes: `box`, `text`, `row`, `grid`, `input`…
    - **Mods** shape nodes: `fg`, `pad`, `round`, `tap`, `on(Hover, …)`…
    - Bare `const Mod` values (like `bold`, `center`, `pill`) take no arguments.
    - `Len` accepts `px()/pct()/rem()/…` or a bare `float` (px).

Headers: `node.hpp` (primitives + mods), `layout.hpp` (containers),
`sugar.hpp` (conveniences), `effect.hpp` (`Cmd`/`Sub`), `router.hpp`,
`meta.hpp`, `scale.hpp`, `live.hpp` (runtime). Including
`<waya/surface/live.hpp>` pulls in everything you need to build and run an app.

---

## Core types

```cpp
using NodeRef = std::shared_ptr<Node>;   // a surface node
struct Mod;                              // a node modifier; apply with |
struct Len { float value; Unit unit; };  // a length + unit
struct Pt  { float x, y; };              // a path point
struct Opt { std::string value, label; }; // a select option
```

### Enums

```cpp
enum class Flow    { none, row, col, stack };
enum class Justify { none, start, center, end, between, around, evenly };
enum class Align   { none, start, center, end, stretch, baseline };
enum class Wrap    { none, wrap, nowrap };
enum class Pos     { none, relative, absolute, fixed, sticky };
enum class Weight  { none, thin, light, normal, medium, semibold, bold, black };
enum class Cursor  { none, pointer, text, move, not_allowed };
enum class Unit    { px, pct, rem, em, vw, vh, fr, fill, hug };
enum class Kind    { box, text, image, path, input, textarea, checkbox,
                     radio, select, button, form, video, audio, markup };
enum class State   { Hover, Focus, Active, Disabled };   // + bare: Hover, Focus, Active, Disabled
enum class Break   { Sm, Md, Lg, Xl };                   // + bare: Sm, Md, Lg, Xl
```

### Length factories

```cpp
Len px(float);  Len pct(float);  Len rem(float);
Len vw(float);  Len vh(float);   Len fr(float);
constexpr Len fill;   // 100%
constexpr Len hug;    // content-sized (auto)
```

---

## Primitives (builders)

| Signature | Description |
|---|---|
| `NodeRef box(Cs... children)` | Generic container. |
| `NodeRef row(Cs... children)` | Horizontal flow (cross-axis centered). |
| `NodeRef col(Cs... children)` | Vertical flow. |
| `NodeRef stack(Cs... children)` | Overlay — children share one centered cell. |
| `NodeRef text(std::string)` | A text run (auto-escaped). |
| `NodeRef text(int)` / `text(long long)` | Render a number. |
| `NodeRef image(std::string src)` | An image. |
| `NodeRef path(std::vector<Pt>, bool closed=false)` | Vector polyline/polygon. |
| `NodeRef markup(std::string html)` | Trusted raw HTML (not escaped). |

## Form controls (builders)

| Signature | Description |
|---|---|
| `NodeRef input(std::string value={})` | Single-line text field. |
| `NodeRef textarea(std::string value={})` | Multi-line text field. |
| `NodeRef checkbox(bool on=false)` | Boolean toggle. |
| `NodeRef radio(std::string group, std::string value, bool on=false)` | Grouped choice. |
| `Opt option(std::string value, std::string label={})` | A `select` option. |
| `NodeRef select(std::vector<Opt>, std::string chosen={})` | Dropdown. |
| `NodeRef button(std::string label)` | A real `<button>`. |
| `NodeRef form(Cs... fields)` | A `<form>` grouping named controls. |
| `NodeRef video(std::string src)` | `<video>` with controls. |
| `NodeRef audio(std::string src)` | `<audio>` player. |

---

## Styling mods

### Colour

| Mod | Description |
|---|---|
| `fg(std::uint32_t)` | Text / foreground colour. |
| `bg(std::uint32_t)` | Background colour. |
| `stroke(std::uint32_t, float width=2)` | Path stroke. |
| `fill(std::uint32_t)` | Path fill. |

### Typography

| Mod | Description |
|---|---|
| `font(float px)` / `font(Len)` | Font size. |
| `font_fluid(float min, float max)` | Font size that scales with viewport. |
| `weight(Weight)` | Font weight. |
| `bold` `semibold` `medium` | Named weights. |
| `italic` `underline` `strike` | Text decoration. |
| `text_align(Justify)` | Horizontal text alignment. |
| `leading(float)` | Line-height multiplier. |
| `tracking(float)` | Letter-spacing (px). |
| `nowrap_text` | Never wrap text. |
| `truncate` | One line, ellipsis on overflow. |

### Box model

| Mod | Description |
|---|---|
| `pad(float\|Len)` | Padding, all sides. |
| `pad_x` / `pad_y (float\|Len)` | Horizontal / vertical padding. |
| `pad_fluid(float min, float max)` | Padding that scales with viewport. |
| `margin(float\|Len)` | Outer margin. |
| `w` / `h (float\|Len)` | Width / height. |
| `size(float\|Len)` | Square (w = h). |
| `max_w` / `min_w` / `max_h` / `min_h (Len)` | Size bounds. |
| `aspect(float ratio)` | Aspect ratio. |
| `round(float\|int\|Len)` | Corner radius. |
| `pill` | Fully rounded. |
| `border(float width, std::uint32_t color)` | Border. |

### Flex & alignment

| Mod | Description |
|---|---|
| `gap(float\|Len)` | Spacing between children. |
| `justify(Justify)` | Main-axis distribution. |
| `align(Align)` | Cross-axis alignment. |
| `grow(float=1)` / `shrink(float=1)` | Flex grow / shrink. |
| `center` | Center on both axes. |
| `between` | Space-between. |
| `wrap` / `nowrap` | Wrapping. |
| `column` / `horizontal` | Force flow direction. |

### Overflow

| Mod | Description |
|---|---|
| `overflow(std::string)` | Raw overflow value. |
| `scroll` | `overflow: auto`. |
| `clip` | `overflow: hidden`. |

### Position

| Mod | Description |
|---|---|
| `absolute(Len t={}, Len r={}, Len b={}, Len l={})` | Absolute positioning. |
| `fixed` / `sticky` / `relative` | Position modes. |
| `inset(Len t, Len r, Len b, Len l)` | Inset offsets. |
| `pin()` | `inset: 0`. |
| `z(int)` | z-index. |

### Effects

| Mod | Description |
|---|---|
| `shadow(std::string="")` | Box shadow. |
| `elevation(int level)` | Preset shadow depth (0–5). |
| `opacity(float)` | Opacity 0–1. |
| `blur(float px)` | Blur the element. |
| `backdrop_blur(float px)` | Frosted-glass backdrop blur. |
| `scale(float)` / `rotate(float deg)` | Transforms. |
| `transition(std::string="all .15s ease")` | Animate changes. |

### Animation

| Mod | Description |
|---|---|
| `fade_in(int ms=300)` | Fade entrance. |
| `fade_up(int ms=400)` / `fade_down(int ms=400)` | Fade + vertical slide. |
| `slide_in(int ms=400)` / `slide_in_left(int ms=400)` | Slide + fade. |
| `pop_in(int ms=350)` | Scale + fade (modals). |
| `spin(int ms=900)` | Continuous rotation. |
| `pulse(int ms=1600)` | Opacity pulse. |
| `bounce(int ms=900)` | Vertical bounce. |
| `ping(int ms=1200)` | Expanding ring. |
| `shimmer(std::uint32_t base, std::uint32_t hi, int ms=1400)` | Loading shimmer. |
| `delay(int ms)` | Delay any animation start. |
| `animate(std::string keyframes, int ms, std::string ease, std::string fill)` | Custom animation. |

### States & breakpoints

| Mod | Description |
|---|---|
| `on(State st, M... mods)` | Mods that apply only in `:hover`/`:focus`/`:active`/`:disabled`. |
| `at(Break b, M... mods)` | Mods applied at and above a breakpoint (min-width). |
| `below(Break b, M... mods)` | Mods applied below a breakpoint (max-width). |

### Escape hatch & semantics

| Mod | Description |
|---|---|
| `css(std::string prop, std::string val)` | Set any CSS property directly. |
| `as(std::string tag)` | Render this box as a specific HTML element. |
| `as_main` `as_nav` `as_header` `as_article` `as_section` `as_footer` `as_aside` | Semantic shortcuts. |
| `attr(std::string k, std::string v)` | Any HTML attribute. |
| `role(std::string)` / `aria(std::string k, std::string v)` | ARIA. |
| `title(std::string)` / `alt(std::string)` | title / alt text. |
| `tab_index(int)` / `focusable()` | Keyboard focus. |

---

## Event mods

| Mod | Description |
|---|---|
| `tap(Msg)` | Click/tap → message. |
| `on_input(Fn: std::string→Msg)` | Fires per keystroke with the value. |
| `on_input(Msg)` | Fires per keystroke (value ignored). |
| `on_change(Fn: std::string→Msg)` | Fires on commit (blur/change) with value. |
| `on_enter(Msg)` / `on_escape(Msg)` | Enter / Escape key. |
| `on_key(std::string key, Msg)` | Named key. |
| `on_keydown(Fn: std::string→Msg)` | Any keydown, with the key. |
| `on_double(Msg)` | Double-click. |
| `on_focus(Msg)` / `on_blur(Msg)` | Focus / blur. |
| `on_enter_pointer(Msg)` / `on_leave_pointer(Msg)` | Pointer enter / leave. |
| `on_hover(Msg enter, Msg leave)` | Both hover transitions. |
| `on_submit(Fn: std::string→Msg)` | Form submit; value is the query string. |
| `draggable(std::string payload={})` | Make draggable. |
| `on_drop(Fn: std::string→Msg)` | Drop target. |
| `on(std::string event, Msg, std::string arg={})` | Any DOM event → fixed message. |
| `on_ev(std::string event, Fn, std::string arg={})` | Any DOM event → mapped message. |
| `stop()` | Stop click propagation to an outer `tap`. |

### Control attribute mods

| Mod | Description |
|---|---|
| `placeholder(std::string)` | Input placeholder. |
| `type(std::string)` | Input type (`password`, `email`, `number`…). |
| `name(std::string)` | Form field name. |
| `checked(bool=true)` | Checkbox/radio checked. |
| `disabled(bool=true)` | Disable control. |
| `key(std::string)` | Stable key for keyed-list diffing. |

---

## Layout builders (`layout.hpp`)

| Signature | Description |
|---|---|
| `cluster(Cs...)` | Items flow and wrap (tags, chips). |
| `grid(Len min_col, Cs...)` | Responsive auto-fit card grid. |
| `switcher(Len threshold, Cs...)` | Row that flips to a column below `threshold`. |
| `sidebar(NodeRef side, NodeRef main, Len side_w=rem(16))` | Sidebar + main that wraps. |
| `center_col(Cs...)` | Centered reading-width column. |
| `hero(Cs...)` | Viewport-height centered section. |
| `columns(int n, Cs... cells)` | Fixed n-column aligned grid. |
| `grid_cols(int n)` *(mod)* | Make a container an n-column grid. |
| `grid_template(std::string tracks)` *(mod)* | Explicit grid tracks. |
| `col_span(int n)` *(mod)* | Span n grid columns. |
| `spacer()` | Flexible empty box. |
| `flexible` *(mod)* | `grow(1)` — make any node a spacer. |

---

## Conveniences (`sugar.hpp`)

### Colour tokens (`namespace color`)

`ink`, `muted`, `faint`, `bg0`, `bg1`, `bg2`, `line`, `brand`, `brand2`,
`good`, `warn`, `bad`, `white`. Plus `float sp(int step)` — a 4px spacing scale.

### Combinators

| Signature | Description |
|---|---|
| `when(bool cond, NodeRef node)` | `node` if true, else an empty box. |
| `when(bool cond, NodeRef a, NodeRef b)` | `a` or `b`. |
| `when(bool cond, Fn build)` | Lazy — builds only if true. |
| `show(bool, NodeRef)` | Alias for `when`. |
| `each(range, Fn: item→NodeRef)` | Map a range to nodes. |
| `screens(int active, {{id, Fn}…})` | Render the active screen (route switch). |
| `fragment(std::vector<NodeRef>)` | Transparent wrapper (no box). |
| `push()` | Flexible spacer. |
| `box_(std::vector<NodeRef>)` / `row_(...)` / `col_(...)` | Vector-form builders. |
| `when_(bool, Mod)` / `when_(bool, Mod a, Mod b)` *(mod)* | Conditional mod. |

### UI pieces

| Signature | Description |
|---|---|
| `avatar(std::string url, int size=40)` | Round cover-cropped image. |
| `avatar_text(std::string initials, std::uint32_t c, int size=40)` | Monogram avatar. |
| `card(Cs...)` | Themed panel: surface + border + pad + radius + elevation. |
| `divider(bool vertical=false)` | A hairline rule. |
| `link(std::string label)` | A styled link. |
| `scroll_fill()` *(mod)* | Fill leftover space and scroll internally. |

### Floating layers

| Signature | Description |
|---|---|
| `anchored(NodeRef trigger, NodeRef floating, std::string place="bottom")` | Position a floater relative to a trigger. |
| `popover(...)` / `modal(...)` / `overlay(...)` | Dropdowns, dialogs, backdrops. |
| `toast_layer(...)` | Toast notification stack. |

### Theming

| Signature | Description |
|---|---|
| `struct Theme` | Semantic design tokens (bg, surface, text, primary…). |
| `theme(Theme)` *(mod)* | Apply a theme to a subtree. |
| `themed()` / `theme_transition()` *(mods)* | Use tokens / animate theme changes. |
| `fg_token` `bg_token` `border_token` … | Token-referencing mods. |
| `fg_text` `fg_muted` `fg_primary` `bg_page` `bg_surface` `bg_raised` `bg_primary` `bg_accent` | Named token mods. |

---

## Effects (`effect.hpp`)

### `Cmd<Msg>`

| Factory | Description |
|---|---|
| `Cmd<Msg>::none()` | No effect. |
| `Cmd<Msg>::quit()` | Stop the session. |
| `Cmd<Msg>::emit(Msg)` | Feed a message back immediately. |
| `Cmd<Msg>::after(long ms, Msg)` | One-shot timer. |
| `Cmd<Msg>::task(std::function<Msg()>)` | Background work → message. |
| `Cmd<Msg>::fetch(std::string url, std::function<Msg(std::string)>)` | Async GET → message. |
| `Cmd<Msg>::navigate(std::string url, bool replace=false)` | Change route. |
| `Cmd<Msg>::push_url(std::string url)` | Update address bar only. |
| `Cmd<Msg>::broadcast(std::string topic, std::string payload={})` | Publish to a topic. |
| `Cmd<Msg>::batch(cmds… \| vector)` | Several commands. |

### `Sub<Msg>`

| Factory | Description |
|---|---|
| `Sub<Msg>::none()` | No subscriptions. |
| `Sub<Msg>::every(long ms, Msg)` | Repeating timer. |
| `Sub<Msg>::on_route(std::function<Msg(std::string)>)` | Route-change → message. |
| `Sub<Msg>::on_topic(std::string topic, std::function<Msg(std::string)>)` | Broadcast → message. |
| `Sub<Msg>::batch(subs… \| vector)` | Combine subscriptions. |

---

## Router (`router.hpp`)

```cpp
Router router();                              // start a route table
Router& Router::at(std::string pattern, int value);   // register (chainable)
Match  Router::match(std::string path) const;         // first match wins
std::size_t Router::size() const;

struct Match {
    bool matched;
    int  value;
    std::unordered_map<std::string,std::string> params;
    std::string param(const std::string& key) const;   // "" if absent
};
```

Patterns: literal segments, `:name` (capture one segment), trailing `*`
(capture the rest). Query strings and trailing slashes ignored.

---

## SEO (`meta.hpp`)

```cpp
struct Meta {
    std::string title, description, canonical, image;
    std::string type = "website", site_name, author, keywords, robots;
    std::string locale = "en_US", card = "summary_large_image", json_ld;
    std::string lang = "en";
    bool has() const;
};

std::string jsonld(std::string type,
                   std::vector<std::pair<std::string,std::string>> fields);
```

Program hooks: `static Meta meta(const Model&)`,
`static const char* site_url()`, `static std::vector<std::string> sitemap()`.
The runtime auto-serves `/robots.txt` and `/sitemap.xml`.

---

## Scaling (`scale.hpp`)

```cpp
template <typename Model> struct Feature { int base, span; /* reducer */ bool owns(int); };

Feature<Model> feature(int base, Fn reducer, int span = 100);

std::pair<Model, Cmd<int>> combine(Model m, int msg, const std::string& value, Fs... features);
std::pair<Model, Cmd<int>> combine(Model m, int msg, Fs... features);
```

---

## Runtime (`live.hpp`)

```cpp
template <typename P> requires SurfaceProgram<P>
int live(LiveConfig cfg = {});   // start server, block until Ctrl-C

struct LiveConfig {
    int port = 8080;
    const char* host = "0.0.0.0";
    bool open = true;
    std::uint32_t page_bg = 0x0b1020;
    const char* title = "waya";
};
```

### The `SurfaceProgram` concept

A `Program` must provide:

```cpp
struct P {
    struct Model { … };
    using   Msg = …;                              // typically a std::variant
    static Model   init();
    static NodeRef view(const Model&);            // required

    // one of:
    static Model                    update(Model, Msg);
    static std::pair<Model,Cmd<Msg>> update(Model, Msg);
    static std::pair<Model,Cmd<Msg>> update(Model, Msg, std::string value);

    // optional:
    static Sub<Msg> subscribe(const Model&);
    static Meta     meta(const Model&);
    static const char* site_url();
    static std::vector<std::string> sitemap();
};
```

### `overload`

```cpp
template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
```

The helper for `std::visit` over a `std::variant` `Msg`.

### Environment variables

| Variable | Effect |
|---|---|
| `WAYA_PORT` | Override listen port. |
| `WAYA_HOST` | Override bind address. |
| `WAYA_NO_OPEN` | Don't auto-open the browser. |

### Rendering backend (`dom.hpp`)

```cpp
class DomBackend {
    struct Output { std::string html, css; };
    Output render(const Node& root);
};
```

Used internally by the runtime and directly in tests to render a surface to
HTML + interned CSS.

---

See also: [Recipes & Patterns](12-recipes.md) for higher-level component
patterns, and the [Examples Walkthrough](13-examples.md) for complete apps.

---

## Appendix: every mod family, in use

A quick, copy-ready example of each family, so you can see the shape rather than
just the signature.

### Colour & text

```cpp
text("Heading")   | fg(0xe2e8f0) | font(28) | weight(Weight::bold)
text("subtitle")  | fg(0x94a3b8) | font(16) | italic
text("CODE_PATH") | fg(0x22d3ee) | font(13) | tracking(0.5f)
text(long_text)   | leading(1.7f) | max_w(rem(34))          // comfortable reading
text(one_liner)   | truncate | max_w(px(200))               // ellipsis on overflow
```

### The box model (padding, size, corners, border)

```cpp
box(content)
    | pad(20)                       // 20px all sides
    | pad_x(24) | pad_y(12)         // or per-axis
    | w(pct(100)) | max_w(rem(30))  // full width, capped
    | round(16)                     // rounded corners
    | border(1, 0x334155)           // 1px border
    | bg(0x1e293b);                 // background
```

### Flex layout

```cpp
row(a, b, c) | gap(12) | center               // horizontal, vertically centered
row(logo, push(), nav) | between              // logo left, nav right (push = spacer)
col(header, body | grow(1), footer) | h(vh(100))   // body fills the height
row(items) | wrap | gap(8)                     // items wrap to new lines
```

### Position & layering

```cpp
stack(
    image(url) | w(pct(100)),
    text("NEW") | absolute(px(8), px(8)) | bg(0xef4444) | fg(0xffffff) | pad_x(8) | round(6)
)                                              // a badge over an image

dialog | fixed | pin() | z(100)                // full-screen overlay layer
```

### Effects & motion

```cpp
card | elevation(2) | transition() | on(Hover, elevation(4) | scale(1.02f))
panel | backdrop_blur(12) | bg(0x1e293b88)     // frosted glass
row(items) | fade_up(400) | delay(120)         // staggered entrance
spinner | spin(700)                            // continuous rotation
placeholder | shimmer()                        // loading shimmer
```

### States (hover / focus / active)

```cpp
text("Menu item")
    | pad(10) | round(8) | transition()
    | on(Hover,  bg(0x334155))
    | on(Active, scale(0.98f))
    | on(Focus,  border(2, 0x6366f1));
```

### Responsive breakpoints

```cpp
col(sidebar, content)
    | at(Md, horizontal | gap(24))   // becomes a row on tablets and up
    | below(Md, gap(12));            // tighter spacing on phones
```

### Events

```cpp
text("Save")        | pointer | tap(Save{})
input(m.q)          | on_input([](std::string v){ return SetQuery{v}; }) | on_enter(Search{})
checkbox(m.on)      | on_change([](std::string v){ return SetOn{ v == "true" }; })
card(item)          | draggable(std::to_string(item.id))
column              | on_drop([](std::string s){ return Move{ s }; })
```

### Semantics & accessibility

```cpp
row(brand, push(), links) | as_nav
col(article_body)         | as_article
image(chart_url)          | alt("Quarterly revenue, up 12%")
button("Menu")            | aria("expanded", m.open ? "true" : "false")
```

### The `css()` escape hatch

```cpp
box(...) | css("background", "linear-gradient(135deg,#6366f1,#22d3ee)")
        | css("mix-blend-mode", "screen")
        | css("clip-path", "circle(50%)");
```

---

## Appendix: common gotchas

- **`col` vs `col_`.** `col(a, b, c)` takes children as arguments; `col_(vec)`
  takes a `std::vector<NodeRef>`. Build children in a loop → use `col_`. Same for
  `row`/`row_`, `box`/`box_`.
- **The input value comes from the model.** Write `input(m.draft)`, not
  `input()`. The model is the source of truth; the field reflects it. To clear a
  field, clear the model value in `update`.
- **Style-on-hover doesn't need a message.** Use `on(Hover, …)` (pure CSS) to
  *look* different on hover. Use `on_hover(Msg, Msg)` only when hovering must
  change your **model**.
- **The sender receives its own `broadcast`.** A `Cmd::broadcast` fans out to
  every subscribed session *including the sender*. Don't also apply the change
  locally, or you'll double it.
- **`tap` needs `pointer` for the cursor.** `tap(Msg{})` wires the click;
  `pointer` shows the hand cursor. Use `interactive()` for hover/press feedback
  too. For real buttons, use the `button` primitive.
- **Give reordering lists a `key`.** Without `key(...)`, a reordered list is
  rebuilt; with it, the diff emits moves and preserves focus/scroll.
- **`update` must stay pure.** No file/network/clock inside it. Need those?
  Return a `Cmd` (fetch/after/task) and let the runtime perform it.
