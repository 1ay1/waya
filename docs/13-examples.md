# Examples Walkthrough

waya ships eleven complete, single-file apps in `examples/`. Each is a real
`SurfaceProgram` you can build, run, and learn from. The easiest way to run one
is the CLI (it builds Release automatically and opens your browser):

```bash
waya run                 # arrow-key picker of every example
waya run aurora          # or name one directly
waya list                # see them all with a one-line blurb
```

Or with plain CMake:

```bash
cmake -S . -B build && cmake --build build -j
./build/aurora           # then open the printed URL
```

Every example is auto-discovered — dropping a new file in `examples/` makes a new
target and a new `waya run` entry with no edits anywhere.

There are three flavours, so you can compare "pure core" vs "batteries":

| Flavour | Examples | Uses |
|---|---|---|
| **Real product** | **nova** — a full issue tracker | drag & drop + multiplayer + palette |
| **Product UI** | aurora, pulse, prism, flow, lumen, showcase | `waya/ui.hpp` (components + patterns) |
| **Live generative** | orbit | core `markup()` + a tick `Sub` |
| **Nerdy compute** | life, mandel, sort | pure C++ in `update`, streamed as deltas |

## nova — a real issue tracker (the flagship)

**`nova` is the app that proves the point:** a complete, polished Linear/Trello-
class issue tracker, built entirely in C++ with **no HTML, CSS, or JavaScript**
and no client bundle. It does everything a modern web app does:

- **Drag-and-drop** cards between columns — they *glide* to their new place,
  because rows are keyed and the diff emits a `move`, not a re-render.
- **Live multiplayer** — open two tabs: create, move, edit, or delete in one and
  it appears in the other instantly (pub/sub `broadcast` + `on_topic`).
- **Command palette** (⌘K / Ctrl-K anywhere) — fuzzy-filter actions, arrow-key
  through them, Enter to run (`on_shortcut`).
- **Inline card creation**, an editable **detail drawer**, **live search** +
  priority filter, and **undo** (⌘Z).

It's ~500 lines of pure `update` over a plain `Model`. Study it to see how the
hard parts of a real web app — DnD, real-time sync, keyboard-first UX, optimistic
state — all collapse into ordinary, testable C++.

```bash
waya run nova            # then open the printed URL in TWO tabs
```

**Learn from it:** `draggable` + `drop_target` for real DnD, `broadcast`/
`on_topic` for multiplayer, `on_shortcut` for global keys, keyed lists for glide
animation, and how a substantial stateful UI stays a pure function of its model.

---

## `aurora` — a premium landing page

A modern SaaS landing page: a **living aurora backdrop** (three soft radial blobs
that drift on a slow server tick), a syntax-coloured code-window hero mockup, a
gradient-clip headline, staggered `fade_up` entrances, a gradient-bordered CTA,
a logo cloud, and a feature grid.

**Learn from it:** how far the style/motion vocabulary goes (`aurora`,
`gradient`, `glow`, `fade_up`, `delay`), and how a `Sub::every` drives an ambient
animation that streams as one tiny delta every 2 s.

## `pulse` — a real-time analytics dashboard

A product-grade dashboard: a sticky, mobile-collapsing **sidebar**, four KPI
cards with live `sparkline`s and trend deltas, an `area_chart` with a segmented
range switch, a `bars` chart, and a rolling activity feed. One server clock
drives it; a pause control shows a `Sub` is just a function of the Model.

**Learn from it:** dashboard layout (`sidebar_shell`-style), the chart helpers,
and adaptive subscriptions (`m.live ? Sub::every(…) : Sub::none()`).

## `prism` — a live theme playground

A settings panel that re-tints entirely when you flip a palette: buttons, badges,
inputs, a toggle, a slider, a progress bar and cards all read theme tokens
(`var(--wa-*)`), so `theme(t)` on the root recolours everything in **one paint**.

**Learn from it:** the token system and live theming (`theme()`, `bg_surface`,
`fg_text`), and real control state that round-trips over the socket.

## `flow` — a keyed kanban board

Move a card between Todo / Doing / Done and it **glides** to its new column
instead of blinking — because the rows are `key(...)`-ed, so the diff emits a
`move`, not a re-render, and the client FLIPs it into place. Live per-column
counts and a progress header.

**Learn from it:** keyed lists and move-diffing — the single most important
performance-and-polish idea in the framework.

## `lumen` — a command palette (⌘K)

Press Cmd/Ctrl-K anywhere to summon a Raycast-style launcher: type to fuzzy-filter
a keyed list (results reorder with a glide), arrow to move, Enter to run, Esc to
close. The chosen command updates a live workspace beneath it.

**Learn from it:** a global shortcut (`on_shortcut("mod+k", …)`), live input
(`on_input`), keyboard events (`on_key`/`on_enter`/`on_escape`), a `modal` layer,
and keyed animated results.

## `showcase` — the pattern library, end to end

A full product page assembled almost entirely from `waya/ui` **patterns**:
`nav_bar`, `hero_section`, `page_header`, `stat`/`metric_card`, `section`,
`list_row`, `feature_card`, `banner`, `empty_state`, `kbd`. It shows how little
code a real, polished screen takes when the building blocks are one call each —
about 110 readable lines.

**Learn from it:** the patterns layer (see
[Components → Patterns](14-components.md#patterns--the-page-shaped-building-blocks)),
and conditional rendering with `when` (the hidden tabs collapse to nothing).

## `orbit` — live generative art

A constellation of nodes swings around drifting attractors; every node is linked
to its nearest neighbours, so the whole web re-weaves ~30×/s. Pure math becomes an
SVG string handed to waya as one `markup()` node, and only the delta ships each
frame. Speed and node-count controls.

**Learn from it:** the `markup()` escape hatch for "anything renders", and a fast
tick `Sub` driving smooth real-time motion (each frame is a single `set_inner`).

## `life` — Conway's Game of Life

Click cells to draw, then Play and the cellular automaton evolves on a server tick,
streaming only the changed cells. Presets (glider, Gosper gun, pulsar), a speed
dial, generation + population counters. The board is a `std::vector<char>`; the
grid is a keyed tap overlay over one SVG so only toggled cells diff.

**Learn from it:** heavy state in the Model, an interactive grid, and
`role="gridcell"` accessibility for a canvas-like surface.

## `mandel` — a Mandelbrot explorer

Each pixel's escape count is computed in C++, coloured by one of four palettes,
packed into a 24-bit BMP, and shipped as a base64 data URI (BMP so there's no
PNG/zlib dependency). A 3×3 tap grid zooms toward the tapped third.

**Learn from it:** raw compute in `update`, and shipping a generated image as one
node — "your algorithm is the app."

## `sort` — a sorting-algorithm visualiser

Bubble / insertion / selection / quicksort run as a **resumable step machine** on
a 24 ms tick, colouring the bars being compared (amber) and swapped (rose), with
comparison + swap counters. All four are correct sorts (verified permutations,
realistic op counts).

**Learn from it:** modelling an algorithm as a state machine you advance one step
per tick — the general pattern for animating any process.

---

## Reading tip

Every example is one file, top to bottom: a `Model`, a `Msg` variant, `init` /
`update` / `view`, and a `main` that calls `live<T>({...})`. Open the one closest
to what you're building and copy the shape — that's the whole learning curve.
