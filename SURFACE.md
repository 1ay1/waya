# waya — The Surface Model

> You describe **what** to render with a tiny vocabulary.
> waya owns **how** — HTML, CSS, canvas, whatever renders it best.
> Powerful enough to draw anything. Simple enough to learn in a minute.

This is the core idea behind waya, and it reframes the DOM-oriented parts of
[DESIGN.md](DESIGN.md) as *one backend*, not the whole story.

---

## 1. The idea

Every web stack forces you to think in the browser's primitives — the box
model, the cascade, the DOM, event handlers, hydration. That complexity leaks
all the way up into your application code, forever.

waya's bet: **those primitives are an implementation detail you should never
touch.** You describe a **surface** — the visual state of your app — with a
small, complete vocabulary. A **rendering backend** turns that surface into
whatever the browser wants: a `<div>` here, styled text there, a `<canvas>`
draw for a chart. You never know which, and it doesn't matter. Swap the
backend and your app is unchanged.

This is exactly how a good rendering library already works one layer down:
you say "draw a rounded box with this text," and the library decides the pixels.
waya raises that to the whole application, and keeps it in sync over the network
by streaming only what changed.

## 2. The vocabulary

Four primitives. Everything you can build is a composition of these.

| primitive | what it is |
|---|---|
| `box(children…)` | a rectangle that holds other things — the universal container |
| `text(string)` | a run of characters |
| `image(src)` | a bitmap |
| `path(points)` | an arbitrary vector shape — a chart, an icon, a custom widget |

Plus layout sugar (`row`, `col`, `stack`, `center`) and a **complete** set of
chaining attributes — anything CSS and layout can express:

```cpp
col(
    text("Dashboard") | fg(0x3b82f6) | font(28) | bold,
    box( text("Requests"), text(count) )
        | pad(12) | round(12) | bg(0x1e293b) | shadow() | border(1, 0x334155)
        | on(Hover, css("transform","translateY(-2px)")),   // states are values
    path(cpu) | stroke(0x22d3ee, 2),                        // a chart — one node
    text("+") | pad(8) | round(8) | bg(0x6366f1) | tap(Inc),
) | gap(16) | pad(24) | center | at(Md, pad(40))            // responsive
```

- **colour / text**: `fg` `bg` `font(size)` `bold` `semibold` `weight(..)`
  `italic` `underline` `leading` `tracking` `text_align`
- **box model**: `pad` `pad_x` `pad_y` `margin` `w` `h` `min_w` `max_w`
  `round` `pill` `border(w, colour)` — lengths take a unit: `w(px(200))`,
  `w(pct(50))`, `w(fill)`, `w(hug)`
- **layout**: `row`/`col`/`stack`, `center`, `justify(..)` `align(..)` `wrap`
  `gap` `grow` `shrink`
- **position**: `absolute(top,left)` `fixed()` `sticky()` `z(n)`
- **effects**: `shadow()` `opacity(..)` `pointer` `transition()`
- **states & responsive**: `on(Hover, …)` `on(Focus, …)` `at(Md, …)` — a state or
  breakpoint is just another bundle of attributes
- **the universal channel**: `css("any-prop", "any-value")` and
  `var("name", "value")` — so **nothing** is ever off-limits (gradients,
  `backdrop-filter`, grid templates, custom properties, anything the browser
  grows tomorrow)
- **interactivity / identity**: `tap(msg)`, `key("id")`

Simple by default (named attrs cover the common 90%), never limiting (`css()`
reaches the other 10%, and `path` draws any shape). And every attribute is the
same kind of value, so they all compose the same clean way.

### Everything is a node; everything you do to one is a `Mod`

This is maya's principle, exact: there is **one** node type, and everything you
do to a node — a colour, a layout, a hover state, an event handler — is the
**same kind of thing**: a `Mod`, a function `Node → Node`, applied with `|`.

```cpp
text("Save") | fg(white) | pad(12) | round(8) | bg(brand)   // style
            | tap(Save)                                    // interactivity
            | on(Hover, bg(0x4f46e5))                       // a state — also a Mod
```

`fg`, `pad`, `tap`, `on(Hover,…)` are all `Mod`s and all pipe onto any node the
same way. Because a `Mod` is just a value, you can **name a bundle** and reuse
it — a "component" is just a shared `Mod` (or a function returning a node):

```cpp
Mod chip = pad(8) | pill | bg(bg2) | fg(ink) | font(13);
text("new")  | chip;
text("beta") | chip;                 // same look, one definition
```

There are no attributes-vs-props-vs-handlers as separate systems. One node, one
modifier, one operator — which is exactly why it stays a delight as the app grows.

### Real inputs, same vocabulary

`input(value)` is the fifth primitive — a real text field. It carries its value
up to `update` through the same message channel:

```cpp
input(m.query) | placeholder("search…") | on_input(QueryChanged) | pad(8) | round(8)

// update gains an optional value arg for input-carrying messages:
static Model update(Model m, Msg msg, std::string value) {
    if (msg == QueryChanged) m.query = value;
    return m;
}
```

## 3. Not limiting — `path` is the "do anything" escape

The vocabulary is tiny, but `path` makes it complete: any 2-D shape is a list of
points with a stroke or fill. A line chart, a sparkline, a pie slice, a hand-
drawn icon, a game sprite outline, a signature pad — all one `path` node. When
you want something the named primitives don't cover, you don't drop to raw HTML;
you describe the shape. That keeps even the escape hatch substrate-agnostic: the
same `path` renders as SVG on the DOM backend and as canvas draw-ops on the
canvas backend.

So: **simple by default (4 primitives), unlimited by construction (`path` draws
anything), and never tied to a substrate.**

## 4. Why substrate-agnostic matters

Three payoffs, and waya gets all three from the same design:

1. **Simplicity.** One mental model. You never learn CSS, the DOM, or the canvas
   API. New primitive to learn? There basically aren't any.
2. **Power via the right backend per case.** A form is best as accessible DOM
   with real inputs; a 10,000-point chart is best drawn on a canvas in one pass.
   waya can render *different parts of the same surface* with different backends
   and you write the same `path` / `box` either way.
3. **Portability.** Because the surface never mentions HTML, the same waya
   program can later render to a native window, a PDF, an image, or an actual
   terminal — new backend, zero app changes. The app is defined by *what it
   shows*, not *what technology shows it*.

## 5. How it stays in sync (the network)

The surface is **retained** — waya keeps last frame's surface. Each frame it
diffs the new surface against the old and sends only the changed nodes. That
minimal delta is the entire payload over the wire: change a counter and the
message is `[[set_text,"1.1","43"]]` — one op, a couple dozen bytes, not a
re-render. The connection is a dumb byte pipe; waya is the smart end.

### The architecture, precisely

Think of it the way maya thinks of a terminal:

| maya | waya |
|---|---|
| **the framework** owns the cell grid, diffs it, decides the frame | **the framework** owns the surface, diffs it, decides the frame |
| writes a **byte stream** (ANSI) to the **TTY** | writes a **byte stream** (frames) to the **WebSocket** |
| the **terminal emulator** paints the cells — dumb, no app logic | the **client** paints the frame — dumb, no app logic, no app state |

The client is a *terminal*: it holds **zero application state and zero
application logic**. It receives frames and paints them as fast as the browser
can, using the browser's own native rendering (real DOM, CSS, fonts, GPU
compositing — all the nice web things). It is smarter than a VT100 only in that
it paints with the browser's engine; it is exactly as dumb in that it just
applies what the bytes say.

**One frame shape, always.** Everything the client ever receives is
`{css, ops}`. A *delta* is the changed ops. A *full paint* is the same shape
with a single `paint` op carrying the whole root — the "all cells changed"
case. The client does not distinguish them; it has exactly one code path:
inject css, apply ops. This means the terminal is **trivially resyncable** —
hand it a full paint at any moment (first load, reconnect, drift) and it is
correct, with no negotiation and nothing to reconcile, because it never held a
truth the server didn't. This is SSR in the truest sense: the server *decides*
every frame; the browser never runs your app, it only displays it.

(The wire is a compact **binary** frame protocol — the framework's private
"ANSI": varint-packed `{css, ops}`, op paths as index sequences, no quoting. A
counter delta is **7 bytes** (vs ~30 as JSON). The terminal decodes it on the
same single code path and coalesces every op of a frame into one
`requestAnimationFrame`, so it touches the DOM once per frame — fewest bytes
down the pipe *and* fewest paints in the browser. None of this changes the
model or your code.)

## 6. Proven

`spike/surface/` (run `spike/surface/run.sh`) demonstrates the whole claim in
one file, **15/15 green**:

- A real dashboard `view(count, chart)` — written purely in the 4-primitive
  vocabulary, not one HTML/CSS/canvas token in sight.
- Rendered through **two backends unchanged**:
  - `DomBackend` → `<div style="display:flex…">`, `<span>`, `<svg>`
  - `CanvasBackend` → `[{"op":"rect"…},{"op":"text"…},{"op":"poly"…}]`
- The **diff** turns `count 42→43` into a single `set_text` op; a chart-data
  change into one `set_path` op.
- A **5,000-point chart** is one `path` node → one draw-op.

That is the thesis, working: describe what to render, render it any way, sync
by delta — powerful, simple, substrate-free.

## 7. Where this sits relative to the rest of waya

The type-state HTML DSL, styling, layout components, and the DOM diff engine
(DESIGN.md §§3–5) are **the DOM backend** of this model — the conservative,
ship-first layer that gives real HTML output, accessibility, and SEO today. The
Surface Model is the layer that makes the substrate a *choice*: the same Elm
`Program` can target the DOM backend now and gain a canvas backend for the parts
that need it, without the application code knowing. The Elm runtime, the delta
streaming, and per-session state are shared by both.

*Design language note: internally we sometimes reason about this as "the browser
is a surface we render deltas to over a byte stream." That's a builder's mental
model — it never appears in the user-facing API or docs. To a waya user this is
simply a clean, substrate-free way to build a UI.*
