<h1 align="center">waya</h1>

<p align="center">
  A <b>C++26 server-side web framework</b>. Describe a <b>surface</b> with a tiny<br>
  vocabulary; waya renders it to the browser and streams only the delta on every interaction.
</p>

<p align="center">
  <b><a href="https://1ay1.github.io/waya/">📖 Documentation</a></b> ·
  <a href="https://1ay1.github.io/waya/01-getting-started/">Getting Started</a> ·
  <a href="https://1ay1.github.io/waya/11-api-reference/">API Reference</a> ·
  <a href="examples/">Examples</a>
</p>

<p align="center">
  <a href="https://github.com/1ay1/waya/actions/workflows/ci.yml"><img src="https://github.com/1ay1/waya/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/1ay1/waya/actions/workflows/docs.yml"><img src="https://github.com/1ay1/waya/actions/workflows/docs.yml/badge.svg" alt="Docs"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-26-blue" alt="C++26">
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT">
</p>

---

You write three pure functions — the initial state, how a message changes it,
and how it looks — and waya turns that into a real, running website: it renders
the UI to HTML + CSS, serves it, and keeps every browser in sync over a
WebSocket, sending only the parts of the page that changed.

**No HTML. No CSS. No DOM. No JavaScript. No client-side state.** You describe
*what* the screen should look like as a function of your data; waya owns *how*
it gets there and stays there.

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };                 // 1. your state
    struct Inc {}; struct Dec {};                // 2. what can happen
    using Msg = std::variant<Inc, Dec>;

    static Model init() { return {}; }           // 3. the starting state

    static Model update(Model m, Msg msg) {      // 4. next state (pure — testable with ==)
        std::visit(overload{
            [&](Inc){ m.n++; },
            [&](Dec){ m.n--; },
        }, msg);
        return m;
    }

    static NodeRef view(const Model& m) {        // 5. how it looks (pure)
        return col(
            text(m.n) | font(64) | weight(Weight::black),
            row(
                text("−") | pad(14) | round(12) | bg(0x1e293b) | pointer | tap(Dec{}),
                text("+") | pad(14) | round(12) | bg(0x6366f1) | pointer | tap(Inc{})
            ) | gap(12)
        ) | gap(24) | pad(48) | center;
    }
};

int main() { return live<Counter>({ .port = 8080 }); }
```

That is a complete, running web app. Click `+` and the number updates — with a
**handful of bytes** over the wire, no re-render, and no framework in the
browser.

## Why waya

- **One vocabulary.** Four primitives — `box`, `text`, `image`, `path` — plus a
  text `input` family, and a complete set of chaining **modifiers** applied with
  `|`. No CSS, no DOM APIs.
- **Everything is a node; everything you do to one is a `Mod`.** A colour, a
  layout, a hover state, an event handler — all the same kind of value. A
  "component" is just a function that returns a node.
- **The Elm Architecture.** A pure `Model`, a pure `update`, a pure `view`. Side
  effects are *described* as data (`Cmd`) and performed by the runtime, so your
  logic is testable with `==` and has no I/O.
- **SSR by construction.** Every route server-renders real, crawlable HTML —
  great SEO out of the box, with correct `<title>`, Open Graph, and JSON-LD.
- **Sync by delta.** The surface is retained; each frame is diffed against the
  last and only the changed nodes are streamed as a compact binary frame.
- **Responsive without breakpoints.** Layout primitives (`grid`, `cluster`,
  `switcher`, `sidebar`) adapt to available space on their own.
- **Real-time & multiplayer.** Timers, async work, and pub/sub broadcast are
  first-class — two open tabs can update each other instantly.

## Install

waya is **header-only**. Add it and link the system threads library:

```cmake
add_subdirectory(third_party/waya)          # or FetchContent
target_link_libraries(my_app PRIVATE waya::waya)
target_compile_features(my_app PRIVATE cxx_std_26)
```

Requires a **C++26** compiler (GCC 15+ is the reference toolchain) and
**CMake 3.28+**. See the
[Getting Started guide](https://1ay1.github.io/waya/01-getting-started/) for the
full walkthrough.

## Run the examples

The repo ships a single `waya` command — like `npm` or `cargo` — that wraps
build, run, scaffold, and live-reload. It works on Linux, macOS, the BSDs, and
Windows (Git-Bash / WSL, or `waya.cmd` from cmd/PowerShell):

```sh
./waya run splash     # build & serve       → http://localhost:8080
./waya list           # every example target
./waya dev            # watch + rebuild + live-reload
./waya doctor         # check your toolchain
```

Symlink it onto your PATH (`ln -s "$PWD/waya" ~/.local/bin/waya`) and drop the
`./`. Prefer raw cmake? That still works:

```sh
cmake -S . -B build && cmake --build build -j
./build/splash    # an animated landing page          → http://localhost:8080
./build/studio    # live theme switching
./build/orbit     # live generative art (a subscription ticks it)
./build/pulse     # a real-time collaborative dashboard (open two tabs)
./build/blog      # "hypertext" — a full content site: router, search, SEO, TOC
./build/showcase  # the grand tour: routed screens, ?query filters, a validated
                  #   form, async fetch with HTTP-status handling, aria-live
```

### Live reload

`waya dev` (or `scripts/dev.sh`) watches the source, rebuilds on save, and the
browser reloads itself. A failed build keeps the last good server up.

```sh
waya dev splash                # edit examples/splash.cpp, save, watch it update
waya dev counter --port 9000   # pick a target, port, --no-open, etc.
```

## Documentation

The full manual lives at **[1ay1.github.io/waya](https://1ay1.github.io/waya/)** —
a from-scratch tutorial, deep API reference, recipes, and internals:

- **[Introduction](https://1ay1.github.io/waya/00-introduction/)** — what waya is and the core idea.
- **[Getting Started](https://1ay1.github.io/waya/01-getting-started/)** — install, build, and run your first app.
- **[The Mental Model](https://1ay1.github.io/waya/02-mental-model/)** — surfaces, nodes, mods, and the Elm loop.
- **[The Vocabulary](https://1ay1.github.io/waya/03-vocabulary/)** · **[Styling](https://1ay1.github.io/waya/04-styling/)** · **[Layout](https://1ay1.github.io/waya/05-layout/)**
- **[The Runtime](https://1ay1.github.io/waya/06-runtime/)** · **[Events](https://1ay1.github.io/waya/07-events/)** · **[Effects](https://1ay1.github.io/waya/08-effects/)**
- **[Routing & SEO](https://1ay1.github.io/waya/09-routing-seo/)** · **[Scaling](https://1ay1.github.io/waya/10-scaling/)**
- **[API Reference](https://1ay1.github.io/waya/11-api-reference/)** — every function, mod, and type.
- **[Recipes](https://1ay1.github.io/waya/12-recipes/)** · **[Examples](https://1ay1.github.io/waya/13-examples/)** · **[Glossary](https://1ay1.github.io/waya/glossary/)**

Design & architecture notes live under [`docs/design/`](docs/design/).

Build the docs locally:

```sh
pip install -r docs/requirements.txt
mkdocs serve      # live preview at http://127.0.0.1:8000
```

## Under the hood

The machinery beneath the surface, all tested:

- **Diff + wire** (`include/waya/surface/`): the surface is retained and diffed;
  the minimal op set (`set_text`/`set_paint`/`set_path`/`replace`/`insert`/
  `remove`/`move`) is streamed as a compact binary frame, coalesced into one
  paint per frame. Applying a patch reproduces the rendered surface (tested).
- **Persistent WebSocket, per-session state** (`include/waya/net/ws.hpp`): a
  dependency-free RFC 6455 implementation. Each browser gets its own `Model`;
  thread-per-connection so one open client never blocks another.
- **The DOM backend** (`include/waya/surface/dom.hpp`): renders a surface to
  HTML with interned (deduplicated) CSS classes. It's internal — you never see
  it. Swap the backend and your app is unchanged.

See the [internals](https://1ay1.github.io/waya/internals/architecture/) docs
for the full picture and the [wire protocol](https://1ay1.github.io/waya/internals/wire-protocol/).

## Performance

After the first paint, an interaction streams **only the nodes that changed** —
not a re-render. Measured on a live dashboard (`bench/frames.cpp`,
`cmake -DWAYA_BUILD_BENCH=ON`):

| Interaction | 10 rows | 100 rows | 1000 rows |
|---|--:|--:|--:|
| Full first paint | 7.3 KB | 49 KB | 470 KB |
| Highlight a row (a click) | **279 B** | **279 B** | **279 B** |
| Counter tick (one text node) | **22 B** | **22 B** | **22 B** |
| Diff + encode time | ~1 µs | ~1 µs | ~1 µs |

The delta is proportional to **what changed**, not to page size — so a one-field
update on a 1000-row table is the same 22 bytes as on a 10-row one, computed in
about a microsecond (the subtree-hash fast path skips every unchanged branch).
At 1000 rows a click sends **~1700× less** than a full render.

## Quality

- **Tests:** `ctest --test-dir build` — the suite is green on every push (CI).
- **Testable by construction:** apps are pure, so the whole thing drives in a
  plain unit test — no browser, no socket. `test::harness<App>()` runs
  `init`/`update`/`view` and lets you assert on state, the rendered tree, and
  even effects (which are plain values). Its sibling `debug::timeline<App>()`
  adds **time-travel**: record every message, step back/forward, jump to any
  point, diff what each message changed on screen, and pinpoint the exact step
  a bug appeared — all from the pure message log.
- **Correct HTML by construction:** debug builds validate the surface against
  the WHATWG content model on every render (unnamed form controls, nested
  interactive nodes, void elements with children, missing alt text all get
  caught loudly). Attribute values are context-escaped and URLs scheme-sanitised.
- **Warnings:** the whole tree builds `-Werror` clean (`cmake -DWAYA_WERROR=ON`).
- **Sanitizers:** all tests pass under ASan+UBSan (`cmake -DWAYA_SANITIZE=ON`);
  the concurrent live runtime is TSan-clean under load.

## License

MIT.
