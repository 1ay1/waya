<h1 align="center">waya</h1>

<p align="center">
  A C++26 server-side web framework with a type-state compile-time HTML DSL,<br>
  a static/dynamic template diff engine, and an Elm-style runtime.
</p>

<p align="center">
  <a href="DESIGN.md">Design</a> ·
  <a href="PLAN.md">Plan</a> ·
  <a href="spike/">Spike</a>
</p>

<p align="center">
  <a href="https://github.com/1ay1/waya/actions/workflows/ci.yml"><img src="https://github.com/1ay1/waya/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-26-blue" alt="C++26">
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT">
</p>

---

> **Status: Phase 0 complete.** The core mechanism is proven and both project
> kill-risks are retired with measurements. See [PLAN.md](PLAN.md).

## The idea

Every web stack forces you to think in the browser's primitives — the box
model, the cascade, the DOM, event handlers, hydration. waya's bet is that
**those should be an implementation detail you never touch.** You describe a
**surface** with a tiny vocabulary; waya owns *how* it renders — HTML, CSS,
canvas, whatever fits — and keeps it in sync over the network by streaming only
what changed. Powerful enough to draw anything, simple enough to learn in a
minute, and never tied to a substrate. See **[SURFACE.md](SURFACE.md)** — it's
the framework's one and only API (`include/waya/surface/`, run
`./build/app`): a live app whose `view()` is pure `box`/`text`/`path`
(a chart is one node), rendered to the browser and kept in sync by streaming
only the delta on each tap. Not one line of the app mentions HTML, CSS, a div,
flex, onclick, or a canvas.

## How you build a waya app

One vocabulary. You describe a **surface** — the visual state of your app — with
four primitives and a few chaining attributes. waya renders it (as HTML today,
as anything tomorrow) and keeps the browser in sync by streaming only what
changed. There is no HTML, no CSS, no DOM, no event wiring in your code.

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };
    using Msg = int;
    enum { Inc, Dec, Reset };

    static Model init()               { return {}; }
    static Model update(Model m, Msg msg) {
        if (msg == Inc)   m.n++;
        if (msg == Dec)   m.n--;
        if (msg == Reset) m.n = 0;
        return m;
    }

    static NodeRef view(const Model& m) {
        auto btn = [](std::string s, int msg, std::uint32_t c) {
            return text(std::move(s)) | fg(0xffffff) | pad(12) | bg(c) | round_(12) | tap(msg);
        };
        return col({
            text(m.n) | fg(0x818cf8) | size(72) | bold,
            row({ btn("-", Dec, 0x334155), btn("reset", Reset, 0x1e293b), btn("+", Inc, 0x6366f1) }) | gap(10),
        }) | gap(24) | pad(48) | bg(0x0b1020);
    }
};

int main() { return live<Counter>({.port = 8080}); }   // ./build/counter
```

That's the whole thing — the Elm shape (`Model` / `Msg` / `init` / `update` /
`view`), and a `view` that describes a surface. `update` is a pure function you
can test with `==`, no browser.

### The vocabulary (all of it)

| primitive | what it is |
|---|---|
| `box(children)` | a rectangle that holds things — the universal container |
| `text(string)` | a run of characters |
| `image(src)` | a bitmap |
| `path(points)` | any 2-D shape — a chart, an icon, a custom widget, all one node |

Plus `row` / `col` / `stack` (a `box` with a direction) and chaining attributes:
`fg` `bg` `size` `bold` `round_` `pad` `gap` `grow` `tap(msg)` `key(...)`. That's
it. A **5,000-point chart is one `path` node**; you never write a loop, a
`<canvas>`, or a `ctx`.

### In sync by delta

waya keeps the previous surface and diffs the next against it — only the changed
nodes travel. Tapping `+` on the counter sends **one op**:

```
tap +  →  [[0,"0","1"]]         (a set_text at the number — a couple dozen bytes)
```

The subtree content-hash lets the diff skip everything unchanged in O(1), so a
dashboard with a big list stays cheap. The connection is a dumb byte pipe; waya
is the smart end.

### Substrate-free — and never limiting

Because your `view` never names HTML, waya owns *how* it renders. Today that's a
DOM backend (a spike also proves a canvas backend from the same surface,
unchanged). Tomorrow it could be a native window, a PDF, an image — your app is
defined by *what it shows*, not the technology that shows it. And nothing is out
of reach: when the named primitives don't cover something, you describe the
shape with `path`, which stays substrate-agnostic (SVG on the DOM backend, draw
ops on the canvas backend).

See **[SURFACE.md](SURFACE.md)** for the full model, and
[`examples/`](examples/) for `hello`, `counter`, and `app` — every one a surface
program, run with `waya::surface::live`.

<details>
<summary>Under the hood: the DOM backend, and its own guarantees</summary>

The surface's DOM backend is built on a type-state HTML DSL that makes invalid
HTML a *compile* error — a `<td>` outside a `<tr>`, a `<div>` inside a `<p>`, an
unescaped interpolation — and interns styles to atomic classes. That machinery
lives in `include/waya/{html,dsl,style,render}/` and is exercised by the test
suite, but it is **implementation detail**: waya users write the surface
vocabulary above, never `div_`/`p_`/`| cls`. If you never write HTML, you can't
write invalid HTML.
</details>

## Under the hood

The machinery beneath the surface, all tested:

- **Diff + wire** (`include/waya/surface/`): the surface is retained and diffed
  with a subtree content-hash (skips unchanged branches in O(1)); the minimal
  op set (`set_text`/`set_paint`/`set_path`/`replace`/`insert`/`remove`) is
  streamed as compact JSON. Soundness is tested — applying a patch reproduces
  the rendered surface.
- **Persistent WebSocket, per-session state** (`include/waya/net/ws.hpp`): a
  dependency-free RFC 6455 implementation (tested against the spec's reference
  vectors). Each browser gets its own `Model`; thread-per-connection so one open
  client never blocks another.
- **Subtree memoisation** (maya's `CacheId`): `each_keyed` re-runs a row's view
  only when its key changes — unchanged rows aren't rebuilt.
- **The DOM backend** is a type-state HTML DSL that makes invalid HTML a compile
  error and interns styles to atomic classes. It's internal; you never see it.

## Measured

The two risks that could have killed the project were discharged with numbers,
not optimism: a 1163-element compile in **739 ms** (gate < 2 s), and framework
diagnostics reduced to **one error in 5–6 lines**. Details in
[DESIGN.md §10](DESIGN.md). Test suite: **32/32 green**, CI on every push.

## Run it

```sh
cmake -S . -B build && cmake --build build -j
./build/counter                # http://localhost:8080 — click the buttons
./build/app                    # a live dashboard with a chart
./build/hello                  # a landing page
```

### Live reload

`scripts/dev.sh` watches the source, rebuilds on save, and the browser updates
itself — the app's WebSocket client reconnects to the rebuilt server and reloads.
A failed build keeps the last good server up, so your browser never hangs.

```sh
scripts/dev.sh counter         # edit examples/counter.cpp, save, watch it update
scripts/dev.sh app build       # a specific target / build dir
```

Install `inotify-tools` for instant reloads; otherwise it polls once a second.

## Repository

- `DESIGN.md` — architecture, philosophy, risk assessment
- `PLAN.md` — phased build plan with gates
- `spike/` — the Phase 0 proof: DSL, tests, runner
- `reference/maya/` — the TUI framework being ported from (git-ignored; clone [1ay1/maya](https://github.com/1ay1/maya) here to follow the source citations)

## Requirements

GCC 15+ with `-std=c++26` (uses P2741 computed `static_assert` messages). The
dev server (`waya/net/serve.hpp`) is POSIX-only for now.

```sh
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

## License

MIT.
