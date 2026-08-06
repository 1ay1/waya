# waya

> **Describe a surface with a tiny vocabulary. waya renders it to the browser and keeps it in sync by streaming only what changed.**

waya is a **C++26 server-side web framework**. You write a pure function from
state to a **surface** — a tree of nodes described with four primitives and a
set of chaining modifiers. waya renders that surface to real HTML + CSS,
serves it, and on every interaction streams back only the minimal delta over a
WebSocket. There is no HTML, no CSS, no DOM, no JavaScript, and no client-side
state in your code.

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {};
    using Msg = std::variant<Inc, Dec>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Inc){ m.n++; },
            [&](Dec){ m.n--; },
        }, msg);
        return m;
    }

    static NodeRef view(const Model& m) {
        return col(
            text(m.n) | font(48) | weight(Weight::black),
            row(
                text("−") | pad(12) | round(10) | bg(0x1e293b) | pointer | tap(Dec{}),
                text("+") | pad(12) | round(10) | bg(0x6366f1) | pointer | tap(Inc{})
            ) | gap(12)
        ) | gap(20) | pad(40) | center;
    }
};

int main() { return live<Counter>({ .port = 8080 }); }
```

That is a complete, running web app: open `http://localhost:8080`, click a
button, and the number updates — with a **7-byte** frame over the wire, no
re-render, no framework in the browser.

---

## Why waya

Every web stack forces you to think in the browser's primitives — the box
model, the cascade, the DOM, event handlers, hydration — and that complexity
leaks all the way up into your application code, forever.

waya's bet is that **those primitives are an implementation detail you should
never touch.** You describe *what* to show; waya owns *how* to show it.

- **One vocabulary.** Four primitives (`box`, `text`, `image`, `path`) plus
  `input`, and a complete set of chaining modifiers. No CSS, no DOM APIs.
- **Everything is a node; everything you do to one is a `Mod`.** A colour, a
  layout, a hover state, an event handler — all the same kind of value, all
  applied with `|`. A "component" is just a function that returns a node.
- **The Elm Architecture.** A pure `Model`, a pure `update`, a pure `view`.
  Side effects are *described* as data (`Cmd`) and performed by the runtime,
  so your logic stays testable with `==` and has no I/O.
- **SSR by construction.** The server decides every frame; the browser only
  paints. Every route server-renders real, crawlable HTML — great for SEO out
  of the box.
- **Sync by delta.** The surface is retained; each frame waya diffs the new
  surface against the last and sends only the changed nodes as a compact
  binary frame.
- **Intrinsically responsive.** Layout primitives (`grid`, `cluster`,
  `switcher`, `sidebar`) adapt to available space *by themselves* — with zero
  media-query breakpoints — and you can still target breakpoints when you want.
- **Real-time & multiplayer.** Timers, async work, and pub/sub broadcast are
  first-class. Two open tabs can update each other instantly.

---

## Start here

<div class="grid cards" markdown>

- :material-rocket-launch: **[Getting Started](01-getting-started.md)**
  Install, build, and run your first app in a few minutes.

- :material-school: **[Tutorial: Build a To-Do App](tutorial-todo.md)**
  A zero-to-finished, line-by-line walkthrough. Start here if you're new.

- :material-brain: **[The Mental Model](02-mental-model.md)**
  Surfaces, nodes, mods, and the Elm loop — the whole idea in one page.

- :material-alphabetical: **[The Vocabulary](03-vocabulary.md)**
  Every primitive and how they compose.

- :material-palette: **[Styling](04-styling.md)**
  Colours, type, the box model, effects, states, and the `css()` escape hatch.

- :material-view-grid: **[Layout](05-layout.md)**
  Rows, columns, grids, and responsive-by-default containers.

- :material-book-open-variant: **[API Reference](11-api-reference.md)**
  The complete, exhaustive listing of every function, mod, and type.

</div>

---

## At a glance

| You want… | You write… |
|---|---|
| A red bold heading | `text("Hi") \| fg(0xef4444) \| bold` |
| A padded rounded card | `col(...) \| pad(20) \| round(16) \| bg(0x1e293b)` |
| A button that sends a message | `text("Save") \| pointer \| tap(Save{})` |
| A text field bound to state | `input(m.q) \| on_input([](std::string v){ return SetQ{v}; })` |
| A responsive card grid | `grid(rem(20), card, card, card)` |
| A hover effect | `box(...) \| on(Hover, bg(0x334155))` |
| A chart from 5,000 points | `path(points) \| stroke(0x22d3ee, 2)` |
| A one-second clock tick | `Sub<Msg>::every(1000, Tick{})` |
| Fetch JSON asynchronously | `Cmd<Msg>::fetch("/data.json", Loaded{})` |
| Navigate to another route | `Cmd<Msg>::navigate("/next")` |

Read on — the [Mental Model](02-mental-model.md) makes all of this click in a
single page.
