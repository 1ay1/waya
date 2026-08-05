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

## 2. The vocabulary (this is all of it)

Four primitives. Everything you can build is a composition of these.

| primitive | what it is |
|---|---|
| `box(children)` | a rectangle that holds other things — the universal container |
| `text(string)` | a run of characters |
| `image(src)` | a bitmap |
| `path(points)` | an arbitrary vector shape — a chart, an icon, a custom widget |

Plus layout sugar (`row`, `col`, `stack` are just a `box` with a flow) and a
handful of visual attributes applied by chaining, maya-style:

```cpp
pad(bg(col({
    bold(size(fg(text("Dashboard"), 0x3b82f6), 28)),
    round_(bg(path(cpu_history), 0x0f172a), 8),        // a chart — one primitive
    row({
        tap(round_(pad(text("+"), 8), 8), Inc),         // tap → a message
        tap(round_(pad(text("reset"), 8), 8), Reset),
    }),
}), 0x0b1020), 24)
```

Notice what is absent: no `<div>`, no `flex`, no `border-radius`, no `<canvas>`,
no `ctx.fillRect`, no `onclick`. Those are how waya renders; they never appear in
your code. Interactivity is one concept: `tap(node, msg)` — this node responds
with a message, which flows through the Elm loop (`Model` / `Msg` / `update` /
`view`) exactly as the rest of waya already works.

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
message is `[["set_text","1.1","43"]]` — one op, a couple dozen bytes, not a
re-render. The connection is a dumb byte pipe; waya is the smart end. (This is
the same delta engine described in DESIGN.md §4, generalised from DOM nodes to
surface primitives.)

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
