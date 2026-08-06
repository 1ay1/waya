# The Runtime — the Elm Architecture

waya runs your app with **the Elm Architecture**: a pure `Model`, a pure
`update`, a pure `view`, driven by a runtime that owns all the I/O. This
chapter is the reference for the `Program` type and how the loop executes.

## The `Program` type

A waya app is a struct providing associated types and static functions. The
minimum, formalised by the `SurfaceProgram` concept:

```cpp
struct App {
    struct Model { /* your entire state */ };
    using Msg = std::variant</* message structs */>;

    static Model   init();                      // starting state
    static Model   update(Model m, Msg msg);    // next state (pure)
    static NodeRef view(const Model& m);        // render (pure)
};
```

Assert it early for clean errors:

```cpp
static_assert(SurfaceProgram<App>);
```

The concept minimally requires the `Model` and `Msg` types and a
`view(const Model&) -> NodeRef`. The runtime additionally calls `init` and one
of the `update` overloads below.

## `Model` — your state

A plain struct (or any type) holding everything your UI depends on. Keep it a
value type; the runtime copies it through the loop.

```cpp
struct Model {
    std::vector<Todo> todos;
    std::string draft;
    Filter filter = Filter::All;
};
```

## `Msg` — what can happen

A `std::variant` of small structs, one per event. This is the *only* channel
through which state changes.

```cpp
struct Add {};
struct Toggle { int id; };
struct SetDraft { std::string text; };
using Msg = std::variant<Add, Toggle, SetDraft>;
```

Each message can carry a payload (`Toggle` carries an `id`; `SetDraft` carries
text). Messages are matched in `update` with `std::visit` + `overload`.

## `init` — the first state

```cpp
static Model init();
```

Returns the model the app starts with. Called once per session (each browser
connection gets its own model).

```cpp
static Model init() { return { .todos = seed(), .filter = Filter::All }; }
```

## `update` — the state transition

`update` is pure: `(Model, Msg) -> Model`. It never performs I/O. Two overloads
are recognised; define whichever you need:

```cpp
// 1. Plain — for apps with no side effects.
static Model update(Model m, Msg msg);

// 2. With effects — return the next state AND a command to run.
static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg);
```

Use `std::visit` + `overload` to switch on the variant:

```cpp
static Model update(Model m, Msg msg) {
    std::visit(overload{
        [&](Add){ m.todos.push_back({ next_id(), m.draft }); m.draft.clear(); },
        [&](const Toggle& t){ for (auto& x : m.todos) if (x.id==t.id) x.done = !x.done; },
        [&](const SetDraft& s){ m.draft = s.text; },
    }, msg);
    return m;
}
```

`overload{...}` is a helper waya provides (via `<waya/surface/live.hpp>`) that
turns a set of lambdas into one callable for `std::visit`.

!!! tip "Keep `update` pure"
    No file reads, no network, no clocks, no globals inside `update`. If you
    need those, return a `Cmd` (overload 2) and let the runtime perform it.
    Purity is what makes `update` unit-testable with `==`.

## `view` — render the state

```cpp
static NodeRef view(const Model& m);
```

Pure: the same model always yields the same surface. Build the surface with the
[vocabulary](03-vocabulary.md) and [mods](04-styling.md), wiring interactions
with `tap`, `on_input`, etc.

```cpp
static NodeRef view(const Model& m) {
    return col(
        input(m.draft) | on_input([](std::string v){ return SetDraft{v}; }) | on_enter(Add{}),
        col_(each(m.todos, [](const Todo& t){ return todo_row(t); }))
    ) | gap(12) | pad(24);
}
```

## Optional: `subscribe` — standing effects

For effects that run *as long as the model says so* — a clock tick, a live
subscription — define `subscribe`:

```cpp
static Sub<Msg> subscribe(const Model& m);
```

```cpp
static Sub<Msg> subscribe(const Model& m) {
    return m.running ? Sub<Msg>::every(1000, Tick{}) : Sub<Msg>::none();
}
```

The runtime reconciles subscriptions each frame: start `Tick` when `running`
becomes true, stop it when it becomes false. See
[Effects & Subscriptions](08-effects.md).

## Optional: `meta` — per-route SEO

```cpp
static Meta meta(const Model& m);
```

Return the `<title>`, description, Open Graph, Twitter card, and JSON-LD for the
current model/route. See [Routing & SEO](09-routing-seo.md).

## The loop, step by step

1. A browser connects. The runtime calls `init()` to make that session's model.
2. It computes `view(model)` and **server-renders** the correct route to HTML
   (with `meta(model)` in the `<head>`), and sends it. The page is complete and
   crawlable before any JavaScript runs.
3. A tiny fixed client script opens a WebSocket.
4. The user interacts. A node wired with `tap(SomeMsg{})` (or `on_input`, etc.)
   sends a compact token over the socket.
5. The runtime maps the token back to your typed `Msg` and calls
   `update(model, msg)` (or the `(Model, Cmd)` overload).
6. If a `Cmd` was returned, the runtime performs it on a worker thread and
   feeds any resulting `Msg` back into step 5.
7. It computes `view(new_model)`, **diffs** it against the previous surface, and
   streams back only the changed nodes as a binary frame.
8. The client applies the patch. Repeat from step 4.

Your code appears only in steps 1, 2, 5, 6, 7 (the pure functions). The socket,
the SSR, the diff, and the patch are the runtime's job.

## Running it: `live`

```cpp
template <typename P> requires SurfaceProgram<P>
int live(LiveConfig cfg = {});
```

Starts the server and blocks until Ctrl-C. Installs a SIGINT handler for clean
shutdown. Returns `0` normally, `1` on bind failure.

```cpp
int main() {
    return live<App>({ .port = 8080, .page_bg = 0x0b1020, .title = "My App" });
}
```

### `LiveConfig`

| Field | Type | Default | Meaning |
|---|---|---|---|
| `port` | `int` | `8080` | Listen port. |
| `host` | `const char*` | `"0.0.0.0"` | Bind address; default = all interfaces (LAN-reachable). Use `"127.0.0.1"` for loopback-only. |
| `open` | `bool` | `true` | Auto-open the browser at startup. |
| `page_bg` | `std::uint32_t` | `0x0b1020` | Background painted behind the app (matches overscroll, safe areas, first paint; drives mobile theme-color). |
| `title` | `const char*` | `"waya"` | Default `<title>` when `Meta.title` is empty. |

### Environment variables

| Variable | Effect |
|---|---|
| `WAYA_PORT` | Override the listen port. |
| `WAYA_HOST` | Override the bind address. |
| `WAYA_NO_OPEN` | If set, don't auto-open the browser. |

### Sessions & concurrency

Each connection runs on its own thread and has its **own model** — so one
slow/blocked client can never stall another, and per-user state is isolated by
construction. Shared state between sessions is done explicitly with pub/sub
broadcast (see [Effects → Broadcast](08-effects.md#broadcast-multiplayer)).

### Optional gzip

Compile with `-DWAYA_GZIP` and link zlib to have the runtime gzip the initial
HTML for clients that advertise `Accept-Encoding: gzip`. Delta frames are
already tiny and are sent uncompressed.

---

Next: [Events & Inputs](07-events.md) — taps, text fields, keys, forms, and
drag & drop.
