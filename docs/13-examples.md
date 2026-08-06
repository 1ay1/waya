# Examples Walkthrough

waya ships several complete, single-file apps in `examples/`. Each is a real
`SurfaceProgram` you can build and run. Build any of them:

```bash
cmake -S . -B build && cmake --build build -j
./build/blog      # then open http://localhost:8080
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
