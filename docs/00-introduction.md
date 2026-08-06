# Introduction

## What waya is

waya is a **server-side web framework** written in C++26. You build a web
application by writing three pure functions —

- **`init`** — the initial state,
- **`update`** — how a message changes the state,
- **`view`** — how the state looks —

and waya turns that into a real, running website: it renders the UI to HTML +
CSS, serves it over HTTP, and keeps every connected browser in sync over a
WebSocket, sending only the parts of the page that changed.

You never write HTML, CSS, DOM code, event handlers, or JavaScript. You never
manage client-side state. You describe *what* the screen should look like as a
function of your data, and waya handles *how* it gets there and stays there.

## The core idea: the Surface Model

Every web stack forces you to think in the browser's primitives — the box
model, the cascade, the DOM, `onclick`, hydration. That complexity leaks all
the way up into your application code and never leaves.

waya's bet: **those primitives are an implementation detail you should never
touch.** You describe a **surface** — the visual state of your app — with a
small, complete vocabulary. A **rendering backend** turns that surface into
whatever the browser needs: a `<div>` here, styled text there, an SVG path for
a chart. You never know which, and it doesn't matter.

This is exactly how a good graphics library works one layer down: you say
"draw a rounded box with this text," and the library decides the pixels. waya
raises that idea to the whole application, and keeps it in sync over the
network by streaming only what changed.

!!! note "The vocabulary is tiny"
    Four primitives — **box**, **text**, **image**, **path** — plus a text
    **input** family, and a set of chaining **modifiers** (colour, size,
    layout, effects, events). That is the entire surface language. Everything
    you can build is a composition of these.

## The three pillars

waya rests on three well-worn, proven ideas, combined:

1. **The Surface Model.** Describe *what*, not *how*. The DOM is one backend,
   not the whole story. (See [The Mental Model](02-mental-model.md).)

2. **The Elm Architecture.** A pure `Model`, a pure `update`, a pure `view`;
   side effects described as data and performed by the runtime. Your app logic
   is a set of pure functions you can unit-test with `==` — no server, no
   browser, no mocks. (See [The Runtime](06-runtime.md).)

3. **Delta streaming.** The surface is retained on the server. On every
   interaction, waya computes the new surface, diffs it against the previous
   one, and sends only the changed nodes as a compact binary frame. The
   browser applies the patch — no re-render, no reconciliation in your code.
   (See [How It Works](internals/architecture.md).)

## What you get for free

Because the server owns every frame and renders real HTML:

- **SSR & SEO.** Every route server-renders crawlable HTML with correct
  `<title>`, Open Graph, Twitter cards, and JSON-LD. (See
  [Routing & SEO](09-routing-seo.md).)
- **No client bundle.** There is no application JavaScript to ship, bundle,
  or hydrate. The only script is a tiny, fixed runtime that opens the socket
  and applies patches.
- **Tiny updates.** A button click that changes one number sends a handful of
  bytes, not a re-rendered page.
- **Responsive by default.** Layout primitives adapt to space on their own.
  (See [Layout](05-layout.md).)
- **Real-time & multiplayer.** Timers, async effects, and pub/sub broadcast
  are first-class; two tabs can update each other live. (See
  [Effects & Subscriptions](08-effects.md).)

## Who it's for

waya is for people who want to build web UIs in C++ without adopting the
browser's mental model — dashboards, internal tools, content sites, live
data views, small products — and who value a small, testable core over a
sprawling ecosystem.

## What it is not

- It is **not** a client-side framework. There is no waya running in the
  browser beyond the fixed patch-applier.
- It is **not** a templating engine or a React binding. Your `view` is C++,
  compiled, type-checked, and diffed.
- It is **strictly a UI-rendering / view layer.** waya renders
  node/component-based UI extremely well and deliberately owns nothing else —
  no host-process management, no database layer, no auth. Those belong in your
  application, not the framework.

Ready? [Get it building](01-getting-started.md).
