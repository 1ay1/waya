# Glossary

**Surface**
: The visual state of your app expressed as a tree of nodes. You describe *what*
  the UI is; waya decides *how* to render it. The central metaphor of the
  framework.

**Node** / **`NodeRef`**
: One element of a surface — a `box`, `text`, `image`, `path`, or a form control.
  `NodeRef` is `std::shared_ptr<Node>`; you build nodes with builders and never
  touch `Node` directly.

**Primitive**
: One of the base node kinds: `box`, `text`, `image`, `path`, plus the `input`
  family. Everything else is composed from these.

**Mod**
: A value that modifies a node — a colour, size, layout rule, animation, state
  behaviour, or event handler. Applied with the pipe operator `|`. Mods are
  first-class: you can name, store, pass, and combine them.

**Builder**
: A function that returns a `NodeRef` (`box`, `row`, `grid`, `text`, `input`…).

**Component**
: A function that returns a `NodeRef`. waya has no component base class,
  lifecycle, or registration — a component is just a function.

**Program** / **`SurfaceProgram`**
: The struct that defines an app: a `Model` type, a `Msg` type, and the static
  functions `init`/`update`/`view` (plus optional `subscribe`/`meta`). The
  `SurfaceProgram` concept formalises the requirements.

**Model**
: A value type holding all of your application state. Each session has its own.

**Msg**
: A `std::variant` of message structs — the only channel through which state
  changes. Usually matched in `update` with `std::visit` + `overload`.

**`update`**
: The pure state transition `(Model, Msg) → Model` (or `→ (Model, Cmd)` when it
  has effects). Contains no I/O — that's what makes it testable.

**`view`**
: The pure render function `(const Model&) → NodeRef`. The same model always
  produces the same surface.

**The Elm Architecture**
: The `Model` / `update` / `view` / `Msg` loop, with side effects described as
  data. waya's runtime pattern.

**`Cmd<Msg>`**
: A description of a one-shot side effect (fetch, timer, navigate, broadcast…)
  returned from `update`. The runtime performs it and feeds results back as
  messages, keeping `update` pure.

**`Sub<Msg>`**
: A description of a standing effect (a clock tick, a subscription) declared by
  `subscribe(const Model&)`. Reconciled each frame — turn effects on/off by
  changing state.

**Diff / delta**
: The minimal set of changes between the previous surface and the new one.
  After each `update`, waya diffs surfaces and streams only the delta.

**SSR (server-side rendering)**
: Rendering the surface to complete HTML on the server before the browser runs
  any script. Every waya route is server-rendered and crawlable.

**Session**
: One browser connection. Each session runs on its own thread with its own
  model, isolating per-user state.

**Broadcast / topic**
: The pub/sub mechanism (`Cmd::broadcast` + `Sub::on_topic`) by which otherwise
  isolated sessions share state — the basis of multiplayer.

**Backend (rendering backend)**
: The layer that turns a surface into a concrete substrate. Today that's the
  **DOM backend** (`DomBackend`), which emits HTML + interned CSS. The Surface
  Model treats the DOM as one backend, not the whole story.

**Route / `Router` / `Match`**
: The URL routing system. `router().at(pattern, id)` builds a table; `match`
  returns a `Match` with the id and captured params.

**Meta**
: The per-route SEO metadata struct (`<title>`, Open Graph, Twitter card,
  JSON-LD) returned by `meta(const Model&)`.

**Feature module**
: A unit of a large app owning a slice of the model and a block of message ids,
  composed with `feature(...)` + `combine(...)` from `scale.hpp`.

**Token (theme token)**
: A semantic colour/role in a `Theme` (page, surface, text, primary…). Token
  mods (`bg_surface`, `fg_text`) resolve against the active theme, so one state
  change re-tints a whole subtree.
