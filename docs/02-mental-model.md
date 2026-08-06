# The Mental Model

Everything in waya follows from four ideas. Once they click, the whole API is
obvious. This page is the most important one in the docs.

## 1. A surface is a tree of nodes

Your UI is a **surface**: a tree of **nodes**. Every node is one of a handful
of kinds — a `box`, some `text`, an `image`, a vector `path`, or an `input`.
You build the tree by nesting builder calls:

```cpp
col(                          // a vertical box
    text("Title"),            // a text node
    row(                      // a horizontal box
        text("A"),
        text("B")
    )
)
```

A node is represented by `NodeRef` (a `std::shared_ptr<Node>`). You almost never
touch `Node` directly — you build nodes with builders and shape them with mods.

!!! tip "There is no HTML here"
    `col` is not `<div style="flex-direction:column">`. It's a *description* of
    a vertical box. waya's DOM backend happens to render it as a flex `<div>`
    today, but that's an implementation detail you never depend on.

## 2. Everything you do to a node is a `Mod`, applied with `|`

A **`Mod`** is a small value that modifies a node: a colour, a size, a layout
rule, a hover behaviour, an event handler — all the same type. You apply mods
with the pipe operator:

```cpp
text("Save")
    | fg(0xffffff)         // colour
    | bg(0x6366f1)         // background
    | pad_x(18) | pad_y(11)// padding
    | round(11)            // corner radius
    | pointer              // cursor
    | tap(Save{})          // on click, send Save{}
```

Because a mod is just a value, you can **name** it, **store** it, **pass** it
around, and **combine** two mods into one:

```cpp
Mod primary = bg(0x6366f1) | fg(0xffffff) | round(11) | pad_x(18) | pad_y(11);

text("OK")     | primary | tap(Ok{})
text("Cancel") | primary | tap(Cancel{})
```

Mods compose left to right; later mods win on conflict. This single rule —
*node `|` mod → node* — replaces the entire CSS cascade, selector system, and
event-attribute API.

## 3. A component is a function that returns a node

There is no `Component` base class, no macro, no registration, no lifecycle.
A component is **a function that returns a `NodeRef`**:

```cpp
NodeRef card(std::string title, std::string body) {
    return col(
        text(title) | font(20) | bold,
        text(body)  | fg(0x94a3b8)
    ) | gap(8) | pad(20) | round(16) | bg(0x1e293b);
}

// use it:
card("Hello", "world") | on(Hover, elevation(3))
```

Components take arguments, return nodes, and compose by nesting — exactly like
functions, because they *are* functions. You can still chain mods onto the
result of a component call, because it returns a node like any other.

## 4. The app is a pure loop: Model → view → Msg → update → Model

waya uses **the Elm Architecture**. Your app is four pieces:

```
        ┌──────────────────────────────────────────┐
        │                                          │
        ▼                                          │
   ┌─────────┐   view(Model)   ┌──────────┐        │
   │  Model  │ ──────────────▶ │ Surface  │        │
   └─────────┘                 └──────────┘        │
        ▲                           │              │
        │                     user interacts       │
        │                           │              │
        │                           ▼              │
        │  update(Model, Msg)  ┌──────────┐        │
        └───────────────────── │   Msg    │ ───────┘
                               └──────────┘
```

- **`Model`** — a struct holding all your state.
- **`view(const Model&) → NodeRef`** — pure: renders the model to a surface.
- **`Msg`** — a `std::variant` of everything that can happen.
- **`update(Model, Msg) → Model`** — pure: computes the next state.

The runtime closes the loop. It renders `view(model)`, serves it, and waits.
When the user taps something wired with `tap(SomeMsg{})`, the runtime calls
`update(model, SomeMsg{})`, then `view(new_model)`, diffs the surfaces, and
streams the delta. Your code never mentions the socket, the diff, or the DOM.

### Why "pure" matters

Because `update` and `view` are pure functions, you can test your entire
application logic with plain equality — no server, no browser, no DOM, no
mocks:

```cpp
Counter::Model m{ .n = 0 };
auto m2 = Counter::update(m, Counter::Inc{});
assert(m2.n == 1);                          // logic test, no I/O
```

This is the payoff of the architecture: the hard part of a UI (state
transitions) becomes ordinary, testable C++.

## Where do side effects go?

A pure `update` can't perform I/O — but it *can* return a **description** of
I/O to perform. That description is a **`Cmd`**. To fetch data, set a timer, or
navigate, `update` returns `(Model, Cmd<Msg>)` instead of just `Model`:

```cpp
static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
    if (std::holds_alternative<Load>(msg))
        return { m, Cmd<Msg>::fetch("/data.json", Loaded{}) };  // async, described
    return { m, Cmd<Msg>::none() };
}
```

The runtime performs the effect and feeds the result back as a message. Your
`update` stays pure. Standing effects (a clock tick, a pub/sub subscription)
are declared by a `subscribe(const Model&) → Sub<Msg>` function. See
[Effects & Subscriptions](08-effects.md).

## Putting it together: the `Program`

A waya app is a struct satisfying the `SurfaceProgram` concept. The minimum:

```cpp
struct App {
    struct Model { /* ... */ };
    using Msg = std::variant</* ... */>;

    static Model  init();                          // starting state
    static Model  update(Model m, Msg msg);        // next state (pure)
    static NodeRef view(const Model& m);           // render (pure)
};
```

Optionally add:

- `static std::pair<Model, Cmd<Msg>> update(...)` — to run effects.
- `static Sub<Msg> subscribe(const Model&)` — for standing effects.
- `static Meta meta(const Model&)` — for per-route SEO metadata.

That's the whole model. Every chapter that follows is just *detail* on these
four ideas: the [vocabulary](03-vocabulary.md) of nodes, the
[mods](04-styling.md) that shape them, the [layout](05-layout.md) containers,
and the [runtime](06-runtime.md) that drives the loop.
