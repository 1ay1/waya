# Examples Walkthrough

waya ships several complete, single-file apps in `examples/`. Each is a real
`SurfaceProgram` you can build and run. Build any of them:

```bash
cmake -S . -B build && cmake --build build -j
./build/blog      # then open http://localhost:8080
```

`counter`, `splash`, and `orbit` use only the **core** (`waya/surface/*`).
`studio`, `pulse`, `blog`, and `dash` also pull in the **component library**
(`#include <waya/ui.hpp>`) for cards, buttons, dialogs, and theme presets — a
good side-by-side of "pure core" vs "batteries."

## `dash` — the component library, end to end

A live dashboard: tabs, an animated line chart, a toggle, a progress bar + slider,
and a typed `data_table` with avatars, badges, and bar charts per row — ticking
itself via a `Sub::every` subscription. The showcase for `waya/ui.hpp`.

**Learn from it:** how icons, widgets, and charts compose like any other node,
and how a rich screen still streams tiny deltas on each interaction.

```bash
./build/dash
```

## `palette` — a Cmd+K command palette

The "wait, C++ did that?" demo. A **global keyboard shortcut** (`mod+k`, from
anywhere) opens a centered, **autofocused**, live-**filtered** command list with
**arrow-key navigation** and Enter-to-run — all from three pure functions and
the mod vocabulary, with zero client state.

**Learn from it:** `on_shortcut` (global), `autofocus`/`on_key`/`on_enter`/
`on_escape` (keyboard), a filtered+highlighted list driven purely by the Model.

```bash
./build/palette
```

## `living` — reusable components that glide

A todo list where rows are **memoised components** (rebuilt only when their own
data changes), items are **keyed** by id (so the diff reconciles by identity),
and `animated()` makes every add / remove / reorder / filter **glide** (FLIP) —
with zero animation state in the Model. The reusable-component story end to end.

**Learn from it:** `memo`, `key()` + `animated()`, and how a pure sort/filter in
`view()` becomes a smooth on-screen animation for free.

```bash
./build/living
```

## `counter` — the smallest complete app (pure core)

State, messages, a pure `update`, and a `view` built from `col`/`row`/`text`
and `|` mods — nothing else. A button is a local function. This is the floor
everything else is built on.

**Learn from it:** the Elm loop end to end, and that a component is just a
function returning a node.

```bash
./build/counter
```

## `splash` — an animated landing page

A striking marketing page: a living aurora headline, a breathing status pill,
gradient-bordered glass cards that lift on hover, a glow CTA, and a live
counter. Everything is one-line mods — no HTML, CSS, or JS.

**Learn from it:** the animation and effect mods (`aurora`, `pulse`, `frost`,
`hover_lift`, `elevation`), fluid type (`font_fluid`), and how a static-looking
page still runs the full Elm loop.

```bash
./build/splash
```

## `studio` — live theming

A design studio showing every polish mod on one screen. Tap a palette and the
**whole app re-tints in a single paint**, smoothly. Built from `theme` tokens,
`themed()`, and `theme_transition()`.

**Learn from it:** semantic theming — how `theme(...)` + token mods
(`bg_surface`, `fg_text`, `bg_primary`) let one state change restyle everything;
and how theme switching stays a pure `update`.

```bash
./build/studio
```

## `orbit` — live generative art

A field of orbiting nodes drawn as SVG `path`s, ticked ~30×/second by a
`Sub::every` subscription. Pure math produces a `NodeRef`; waya streams only
what moved.

**Learn from it:** the `path` primitive for vector graphics, high-frequency
subscriptions, and the proof that "anything renders" — a physics animation is
just a surface that changes every frame, diffed like any other.

```bash
./build/orbit
```

## `pulse` — a real-time collaborative dashboard

A live ops dashboard: animated metric bars, a keyed live activity feed, and
**broadcast presence** — open two tabs, ping in one, and it lights up in the
other instantly.

**Learn from it:** `Cmd::broadcast` + `Sub::on_topic` for multiplayer, keyed
lists (`key(...)`) for smooth feed updates, and combining several subscriptions
with `Sub::batch`.

```bash
./build/pulse    # open two browser tabs
```

## `blog` — "hypertext", a full content site

The most complete example: a whole blog engine in one file. It has a real
router (`/`, `/post/:slug`, `/tag/:tag`, `/archive`, `/about`), a seed corpus
with full metadata, **live search** with a result count, a **featured hero
post**, tag filtering and a tag cloud, monogram author avatars, per-post SEO
(Open Graph + Article JSON-LD), reading time, a **scroll-driven reading-progress
bar**, an **auto-generated table of contents**, **related posts**, and
**prev/next navigation**. Every route server-renders crawlable HTML.

**Learn from it:** routing wired into the Elm loop
([Routing & SEO](09-routing-seo.md)), the `screens` route switch, per-route
`meta`, `sitemap()`/`site_url()` for SEO, and how a large-ish app is organised
with component functions.

```bash
./build/blog
```

Key structure to study in `examples/blog.cpp`:

- `corpus()` — the seed data.
- `routes()` — the `Router` table.
- `update(Model, Msg, std::string)` — handling `Route`/`Nav`/`SetQuery`.
- `subscribe` — `Sub::on_route` to track navigation.
- `view` — the `screens(...)` switch.
- `meta` — per-route SEO.
- Component functions: `post_card`, `featured_card`, `post_view`, `post_footer`.

## Building your own

Start from the [counter in Getting Started](01-getting-started.md), then reach
for the example closest to your goal:

- **Marketing / landing:** `splash`.
- **Dashboard / data:** `pulse`.
- **Content / docs / blog:** `blog`.
- **Graphics / visualisation:** `orbit`.
- **Theming / design system:** `studio`.

Each is small enough to read in one sitting and copy from.
