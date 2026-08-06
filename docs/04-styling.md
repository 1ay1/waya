# Styling

Styling in waya is **modifiers** (`Mod`s) applied to nodes with `|`. There is no
CSS, no cascade, no selectors — a mod is a value, it applies to the node you
pipe it onto, and later mods win on conflict. This chapter is the reference for
every built-in styling mod.

```cpp
text("Save")
    | fg(0xffffff) | bg(0x6366f1)
    | pad_x(18) | pad_y(11) | round(11)
    | pointer | tap(Save{});
```

## Lengths and units

Sizes take a `Len` — a value plus a unit. Construct one with a factory:

| Factory | Unit | Example |
|---|---|---|
| `px(v)` | pixels | `w(px(200))` |
| `pct(v)` | percent | `w(pct(100))` |
| `rem(v)` | root em | `max_w(rem(48))` |
| `vw(v)` / `vh(v)` | viewport width/height | `h(vh(100))` |
| `fr(v)` | grid fraction | grid tracks |
| `fill` | 100% of the parent | `w(fill)` |
| `hug` | content-sized (auto) | `w(hug)` |

Most size mods also accept a bare `float`, treated as pixels: `w(200)` ==
`w(px(200))`, `pad(16)` == `pad(px(16))`, `round(12)`.

## Colour

Colours are `0xRRGGBB` hex integers.

```cpp
Mod fg(std::uint32_t c);   // text / foreground colour
Mod bg(std::uint32_t c);   // background colour
```

```cpp
text("Hi") | fg(0xe2e8f0) | bg(0x1e293b)
```

For named tokens (`ink`, `muted`, `brand`, `good`…) see
[Styling with the palette](#the-palette) and the `sugar.hpp` colour tokens.

## Typography

```cpp
Mod font(float px);              // font size in px
Mod font(Len sz);                // font size with a unit
Mod font_fluid(float min, float max);  // clamps between two px sizes as viewport scales
Mod weight(Weight w);            // thin…black
Mod text_align(Justify j);       // start / center / end
Mod leading(float lh);           // line-height multiplier
Mod tracking(float ls);          // letter-spacing (px)
```

Named weight/style mods (no arguments):

| Mod | Effect |
|---|---|
| `bold` | weight 700 |
| `semibold` | weight 600 |
| `medium` | weight 500 |
| `italic` | italic |
| `underline` | underline |
| `strike` | line-through |
| `nowrap_text` | never wrap text |
| `truncate` | one line, ellipsis on overflow |

```cpp
text("Title") | font(28) | bold | leading(1.1f)
text(long_str) | truncate | max_w(rem(20))
text("HERO")   | font_fluid(32, 72) | weight(Weight::black)
```

`Weight`: `thin, light, normal, medium, semibold, bold, black`.

## The box model

### Padding

```cpp
Mod pad(float);   Mod pad(Len);            // all sides
Mod pad_x(float); Mod pad_x(Len);          // left + right
Mod pad_y(float); Mod pad_y(Len);          // top + bottom
Mod pad_fluid(float min, float max);       // padding that scales with viewport
Mod margin(float); Mod margin(Len);        // outer margin
```

### Size

```cpp
Mod w(float|Len);   Mod h(float|Len);      // width / height
Mod size(float|Len);                       // square: sets w and h
Mod max_w(Len); Mod min_w(Len);
Mod max_h(Len); Mod min_h(Len);
Mod aspect(float ratio);                   // width/height ratio (e.g. 16.0/9.0)
```

### Corners & borders

```cpp
Mod round(float|int|Len);                  // corner radius
const Mod pill;                            // fully rounded (999px)
Mod border(float width, std::uint32_t color);
```

```cpp
box(child)
    | pad(20) | round(16)
    | bg(0x1e293b) | border(1, 0x334155)
    | max_w(rem(30))
```

## Flex & alignment

`row`/`col` are flex containers. Control them with:

```cpp
Mod gap(float|Len);            // spacing between children
Mod justify(Justify j);        // main-axis distribution
Mod align(Align a);            // cross-axis alignment
Mod grow(float = 1);           // this child takes leftover space
Mod shrink(float = 1);         // this child may shrink
const Mod wrap;                // children wrap to new lines
const Mod nowrap;
const Mod center;              // center on both axes
const Mod between;             // space-between on the main axis
const Mod column;              // force column flow
const Mod horizontal;          // force row flow
```

`Justify`: `start, center, end, between, around, evenly`.
`Align`: `start, center, end, stretch, baseline`.

```cpp
row(logo, push(), nav) | between | center      // logo left, nav right
col(a, b, c) | gap(16) | center                // stacked, centered
row(sidebar, content | grow(1))                // content fills the rest
```

(`push()` is a flexible spacer; see [Layout](05-layout.md).)

## Overflow & scrolling

```cpp
Mod overflow(std::string v);   // raw overflow value
const Mod scroll;              // overflow: auto
const Mod clip;               // overflow: hidden
```

For a scrollable region that fills leftover space and scrolls internally (a
chat log, a list), use `scroll_fill()` from `sugar.hpp`.

## Position

```cpp
Mod absolute(Len top = {}, Len right = {}, Len bottom = {}, Len left = {});
const Mod fixed;
const Mod sticky;
const Mod relative;
Mod inset(Len t, Len r, Len b, Len l);
Mod pin();                     // inset: 0 (fill the positioned parent)
Mod z(int);                    // z-index
```

```cpp
box(badge) | absolute(px(-6), px(-6))          // top-right badge
overlay_content | fixed | pin() | z(100)       // full-screen layer
```

## Effects

```cpp
Mod shadow(std::string spec = "");    // box-shadow (default = a soft shadow)
Mod elevation(int level);             // 0..5 preset shadow depth
Mod opacity(float 0..1);
Mod blur(float px);                   // blur the element
Mod backdrop_blur(float px);          // frosted-glass blur behind it
Mod scale(float f);                   // transform scale
Mod rotate(float deg);                // transform rotate
Mod transition(std::string = "all .15s ease");   // animate property changes
```

```cpp
card | elevation(2) | transition() | on(Hover, elevation(4) | scale(1.02f))
panel | backdrop_blur(12) | bg(0x1e293b88)     // frosted glass
```

### Vector-graphics mods (for `path`)

```cpp
Mod stroke(std::uint32_t color, float width = 2);
Mod fill(std::uint32_t color);
```

## Animation

Ready-made entrance and looping animations. Each takes an optional duration in
ms.

| Mod | Effect |
|---|---|
| `fade_in(ms)` | fade from transparent |
| `fade_up(ms)` / `fade_down(ms)` | fade + slide vertically |
| `slide_in(ms)` / `slide_in_left(ms)` | slide + fade horizontally |
| `pop_in(ms)` | scale up + fade (great for modals) |
| `spin(ms)` | continuous rotation |
| `pulse(ms)` | opacity pulse |
| `bounce(ms)` | vertical bounce |
| `ping(ms)` | expanding ring |
| `shimmer(base, hi, ms)` | loading shimmer sweep |
| `delay(ms)` | delay the start of any animation above |
| `animate(keyframes, ms, ease, fill)` | a fully custom keyframe animation |

```cpp
card | fade_up(400) | delay(120)
spinner | spin(700)
skeleton | shimmer()
```

## States (`:hover`, `:focus`, `:active`, `:disabled`)

Apply a bundle of mods that only take effect in a UI state:

```cpp
template <typename... M> Mod on(State st, M... mods);
```

`State`: `Hover`, `Focus`, `Active`, `Disabled` (also available as the bare
constants `Hover`, `Focus`, `Active`, `Disabled`).

```cpp
text("Menu")
    | pad(10) | round(8)
    | transition()
    | on(Hover, bg(0x334155) | scale(1.03f))
    | on(Active, scale(0.98f));
```

This is how you build hover/focus behaviour without a stylesheet — the mods
inside `on(Hover, …)` are compiled into a scoped `:hover` rule for that node.

## Responsive breakpoints

Layout in waya is responsive by default (see [Layout](05-layout.md)), but when
you want explicit breakpoints, use `at` (min-width — apply at and above) and
`below` (max-width — apply below):

```cpp
template <typename... M> Mod at(Break b, M... mods);      // ≥ breakpoint
template <typename... M> Mod below(Break b, M... mods);   // < breakpoint
```

`Break`: `Sm` (640px), `Md` (768px), `Lg` (1024px), `Xl` (1280px), also as the
bare constants `Sm`, `Md`, `Lg`, `Xl`.

```cpp
col(a, b)
    | at(Md, horizontal | gap(24))     // becomes a row on tablets+
    | below(Md, gap(12));              // tighter on phones
```

## The `css()` escape hatch

Anything the mod vocabulary doesn't cover, set directly:

```cpp
Mod css(std::string property, std::string value);
```

```cpp
box(...) | css("background", "linear-gradient(135deg,#6366f1,#22d3ee)")
        | css("mix-blend-mode", "screen");
```

`css()` is the pressure valve — you never get stuck. But reach for a named mod
first: named mods carry intent, compose predictably, and stay backend-neutral.

## Semantics, attributes, and accessibility

```cpp
Mod as(std::string tag);       // render this box as a specific HTML element
Mod attr(std::string k, std::string v);   // any HTML attribute
Mod role(std::string r);       // ARIA role
Mod aria(std::string k, std::string v);   // aria-* attribute
Mod title(std::string t);      // tooltip / title attribute
Mod alt(std::string a);        // image alt text
Mod tab_index(int i);          // tabindex
Mod focusable();               // make a non-control focusable
```

Named semantic shortcuts: `as_main`, `as_nav`, `as_header`, `as_article`,
`as_section`, `as_footer`, `as_aside` (and `as("...")` for anything else).

```cpp
row(brand, push(), links) | as_nav
col(article_body) | as_article
image(url) | alt("A diagram of the render loop")
```

Using semantic elements improves accessibility and SEO for free — the DOM
backend emits real `<nav>`, `<main>`, `<article>` tags.

### Announcing live updates

waya streams DOM deltas so the page changes *silently* — which means a screen
reader won't notice "3 items added", "Saved", or "Error: email taken" unless you
mark the region that changes as **live**. This is the a11y counterpart to the
whole live-update model:

```cpp
col(cart_rows) ,
box(text(m.status)) | live_region()      // polite: announced after a pause
status("Saved")                          // ready-made polite region (role=status)
alert("Email already taken")             // assertive: interrupts (errors/alerts)
```

`live_region()` is `polite` by default (waits for a lull); pass `true` for
`assertive` (interrupts immediately — reserve it for errors). `status()` and
`alert()` are ready-made text regions with the right roles. Update their text
from your `Model` and the announcement happens automatically on the next paint.

## The palette

`sugar.hpp` ships a pleasant default palette so you can write intent, not hex:

```cpp
using namespace waya::surface::color;
text("Title") | fg(ink)          // 0xe2e8f0
text("Sub")   | fg(muted)        // 0x94a3b8
button        | bg(brand)        // 0x6366f1
badge         | bg(good)         // 0x34d399  (warn / bad also available)
```

For full design-system theming (semantic tokens that restyle a whole app in one
paint), see [Recipes → Theming](12-recipes.md#theming).

---

Next: [Layout](05-layout.md) — rows, columns, grids, and the containers that
make a UI responsive without a single media query.
