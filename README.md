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
minute, and never tied to a substrate. See **[SURFACE.md](SURFACE.md)** (proven
in `spike/surface/`, 15/15: one `view()`, a DOM backend AND a canvas backend,
unchanged).

The first shipped layer is the **DOM backend** of that model — and it carries a
guarantee no JS framework can:

[maya](https://github.com/1ay1/maya) is a C++26 TUI framework that renders a **cell grid** to
a terminal, and whose headline guarantee is that *impossible states don't
compile* — you cannot set a border colour without first declaring a border.

**waya ports that philosophy to the web.** Same type-state machinery, same
Elm-style runtime, same effects-as-data — but it renders a **DOM tree** to a
browser, and the guarantee becomes something with real economic value:

> **Your template will not compile if the HTML is wrong.**

A `<td>` outside a `<tr>`, a `<div>` inside a `<p>`, an `href` on a `<span>`, an
unescaped interpolation into `onclick` — every one is a compile error, reported
at the call site, before your program exists. No JSX-based framework can make
that claim: TSX type-checks *props*, not *content models*. `<p><div/></p>` is
valid TSX and invalid HTML, and the browser silently reparses it into something
you did not write.

## It already works

```cpp
#include <waya_dsl.hpp>
using namespace waya;
using namespace waya::dsl;

// Rendered entirely at COMPILE TIME — this page costs zero work at runtime.
constexpr auto page = html_(
    head_(title_(text("Hi"))),
    body_(p_(text("hello")))
);

static_assert(render_document(page) ==
    "<!DOCTYPE html><html><head><title>Hi</title></head>"
    "<body><p>hello</p></body></html>");
```

and the invalid cases do not build:

```cpp
p_(div_())                      // error: waya: <div> is not permitted inside <p>.
                                //   The HTML5 content model for <p> permits phrasing
                                //   content (text, <span>, <a>, <img>, …).
                                //   See https://html.spec.whatwg.org/#the-p-element

div_(td_())                     // error: <td> is not permitted inside <div>
br_(text("x"))                  // error: <br> is a void element and cannot have children
span_(text("s")) | href<"/u">   // error: the 'href' attribute is not valid on <span>
```

Escaping is automatic and cannot be skipped:

```cpp
render(p_(text("<script>alert('xss')</script>")))
// → "<p>&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;</p>"
```

## Styling like maya — no CSS in the DSL

maya owns every cell it paints; CSS is nowhere. waya keeps that: **you style in
the DSL by piping values, never in a `.css` file.** CSS is to waya what ANSI is
to maya — a private output encoding the renderer emits, not a language you touch.

```cpp
div_(
    h1_(text("Dashboard")) | fg<0x3B82F6> | bold | size<28_px>,
    row_(btn("−"), btn("+")) | gap<12_px>
)
| col_ | gap<16_px> | pad<24_px> | bg<0x0F172A> | rounded<12_px>
```

The renderer **interns** identical styles to one atomic class and ships one
deduplicated stylesheet — the same trick as maya's `StylePool`. And because the
style is a typed value, it catches CSS bugs no stylesheet can:

```cpp
p_(...)   | gap<8_px>       // error: gap requires a flex/grid container
row_(...) | gap<8_px>       // ok  — row_ is a flex container
... | size<-4_px>            // error: negative length
```

**Not limiting — *any* CSS is one clean pipe.** The named tokens (`fg`, `pad`,
`flex`, …) are just sugar. Anything CSS can do — including properties waya has
never heard of, pseudo-classes, and media queries — is the same clean pipe, and
is still interned and diffed:

```cpp
div_(...) | prop<"backdrop-filter", "blur(8px)">
          | prop<"grid-template-columns", "repeat(3, 1fr)">
          | var_<"--brand", "#3b82f6">
          | on<Hover>(bg(0x2563eb))         // .wa-x:hover { … }
          | at<Md>(pad(16_px))              // @media (min-width:768px)
```

### Layout, one call

Common layouts are components, not repeated pipes — responsive by default, and
still fully composable (keep piping styles onto them):

```cpp
row(a, b, c)                  // side by side, wraps as it narrows
col(header, body, footer)     // stacked, each fills the width
row(logo, spacer(), nav)      // spacer() shoves siblings to opposite ends
cluster(tag1, tag2, tag3)     // wrapping chips, vertically centred
grid_auto(240_px, /*cards*/)  // as many columns as fit, re-flows itself
grid(a, b, c) | cols(3)       // or a fixed N-column grid
sidebar(nav, main_, 260_px)   // fixed rail + fluid main, stacks when narrow
center(content)               // centred both axes
row(a, b) | gap(24_px) | bg(0x111827)   // still pipeable
```

### Attributes & events — also one clean pipe

The same idea as styling: `id`/`class`/`href` are fast paths, but **any**
attribute, boolean attribute, `data-*`, ARIA, or DOM event is the same pipe —
and values are auto-escaped:

```cpp
input_() | attr<"type", "email"> | attr<"name", "email"> | flag<"required">
button_(text("Go")) | on_click<"submit()"> | flag_if("disabled", !valid)
div_(...) | attr<"data-id", "42"> | attr<"role", "tabpanel"> | attr<"aria-label", "Details">
input_() | attr_dyn("value", model.query)   // runtime value
```

A whole sign-up form is a handful of pipes — see [`examples/form.cpp`](examples/form.cpp)
(`./build/form`).

### Live apps — the Elm architecture

For stateful UI, waya ports maya's `Program`: a pure `Model` + `Msg` + `update`
+ `view`. Click a button → `Msg` → `update` runs on the server → the page
re-renders. No reload, no hand-written JS.

```cpp
struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {};
    using Msg = std::variant<Inc, Dec>;

    static Model init() { return {}; }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Inc){ return std::pair{Model{m.n+1}, Cmd<Msg>::none()}; },
            [&](Dec){ return std::pair{Model{m.n-1}, Cmd<Msg>::none()}; },
        }, msg);
    }
    static auto view(const Model& m) {
        return col(
            h1_(text(std::to_string(m.n))),
            row(button_(text("−")) | on_msg(Msg{Dec{}}),
                button_(text("+")) | on_msg(Msg{Inc{}}))
        );
    }
};
int main() { return waya::live<Counter>(); }   // ./build/counter
```

`update` is a **pure function** — test it with `==`, no server (see
[`tests/test_program.cpp`](tests/test_program.cpp)). Effects are data (`Cmd`);
the runtime performs them.

**The browser window is the terminal.** maya keeps the previous cell grid and
sends only the cells that changed; waya keeps the previous DOM tree and sends
only the nodes that changed. Clicking `+` on the counter sends a **15-byte
patch** — `[[0,"0.0","1"]]`, one `set_text` op — not the 2.6 KB page:

```
click +  →  [[0,"0.0","1"]]        (15 bytes on the wire, ~175× smaller)
```

The diff (`include/waya/render/diff.hpp`) walks the old and new render in
lockstep and emits the minimal op set (`set_text`/`set_attr`/`remove_attr`/
`replace`/`insert`/`remove`) addressed by node path; the ~30-line client walks
to the node and mutates it. Diff soundness — `apply(diff(a,b), a) == b` — is
tested in [`tests/test_diff.cpp`](tests/test_diff.cpp).

**Persistent WebSocket + per-session state.** `waya::live_ws<P>()` keeps the
connection open: the browser opens a WebSocket, sends a message id on click, and
the server pushes the JSON patch straight back — no request per click. Each
browser gets its own `Model` + memo cache + prev-tree, so clients are
independent. RFC 6455 handshake + frame codec live in
[`include/waya/net/ws.hpp`](include/waya/net/ws.hpp) (dependency-free, tested
against the spec's reference vectors).

**Subtree memoisation — unchanged rows aren't rebuilt.** maya caches components
by a content-hash `CacheId`; waya does the same. `each_keyed(rows, key_fn,
view_fn)` runs a row's `view_fn` only when its key changes:

```cpp
tbody_(each_keyed(rows,
    [](const Row& r){ return cache_id("row", r.id, r.ms, r.up); },   // key
    [](const Row& r){ return tr_(td_(text(r.name)), …); }))           // view
```

On a frame where nothing changed, **zero** row callbacks run; change one row and
**one** runs (tested: [`tests/test_memo.cpp`](tests/test_memo.cpp)).

**One applier, one source of truth — the live UI can't drift.** The server's
`diff`, the runtime's own tree-tracking, and the browser's JS are all the SAME
patch semantics. `waya::vdom::apply` is the single canonical applier; the
runtime keeps its `prev` tree by applying its own patches to it (so the server
always diffs against exactly what the client's DOM is), and
`verify_roundtrip<P>(msgs)` proves it for any Program with no browser:

```cpp
// executable form of "if it compiles, the live UI is consistent"
assert(verify_roundtrip<Counter>({Inc{}, Inc{}, Dec{}, Reset{}}));
```

After every message it asserts `apply(prev, diff(prev,next)) == next`. Tested
across text/attr/insert/remove/replace ops and a 200-step walk
([`tests/test_live_sound.cpp`](tests/test_live_sound.cpp)).

Proven working — `./spike/run_style.sh` (6/6) plus `test_style_general` (14/14,
arbitrary props, custom properties, pseudo-classes, media queries, grid,
interning-with-states). Full rationale in [DESIGN.md §5.5](DESIGN.md).

## Measured, not promised

```
$ ./spike/run_spike.sh

=== 1. Positive tests ===                    PASS
=== 2. Negative tests (must NOT compile) ===  14 / 14 rejected
=== 3. Compile-time gate ===                  701 elements : 466 ms   (gate: < 2000 ms)
                                             1163 elements : 739 ms
=== 4. Error message quality ===              1 error, 5–6 lines      (gate: ≤ 8 lines)

 passed: 21   failed: 0

$ ./spike/run_style.sh                        passed: 6   failed: 0
```

The two risks that could have killed the project — **compile times** and
**diagnostic quality** — are now discharged with numbers rather than optimism.

## What's planned

Three tiers of interactivity in one DSL, chosen per component:

- **Tier 0 — static.** Compile-time rendered pages. No JS at all.
- **Tier 1 — hypermedia.** htmx-style partial updates, no session state.
- **Tier 2 — live.** LiveView-style stateful WebSocket sessions with an Elm
  loop and a static/dynamic diff engine that sends ~60-byte patches.

Plus islands, streaming SSR, typed routing, reflection-driven form decoding, and
a Witness-Chain approach to escaping soundness. Full architecture in
[DESIGN.md](DESIGN.md); sequencing in [PLAN.md](PLAN.md).

## See it in the browser

The examples ship with a tiny built-in dev server, so you can view a page
without writing any HTTP glue:

```sh
cmake -S . -B build && cmake --build build -j
./build/hello                 # serves http://localhost:8080 and opens your browser
```

That's it — the `hello` example renders a styled page with a live, dynamically
generated services table. Pass `--print` instead to dump the HTML to stdout
(for piping or diffing):

```sh
./build/hello --print > page.html
```

### Live reload

For the edit loop, `scripts/dev.sh` watches the source tree, rebuilds on save,
and restarts the server — and the page **refreshes itself** a moment later (the
dev server injects a tiny live-reload client, no browser extension needed):

```sh
scripts/dev.sh                 # runs the `hello` example by default
scripts/dev.sh counter         # a specific example (Model/Msg/update/view)
scripts/dev.sh mypage build    # custom target + build dir
```

Edit a `.cpp`/`.hpp`, save, and the browser updates itself — for a `live_ws`
app the WebSocket client reconnects to the rebuilt server and reloads; for a
plain SSR page the injected heartbeat refreshes it. Try `scripts/dev.sh counter`
and click the buttons.

Install `inotify-tools` for instant reloads; otherwise it polls once a second.

Serving your own page is one call:

```cpp
#include <waya/waya.hpp>
#include <waya/net/serve.hpp>
using namespace waya::dsl;

int main() {
    return waya::serve([] {
        return html_(head_(title_(text("hi"))),
                     body_(h1_(text("Hello from waya"))));
    });
}
```

> The dev server is a minimal blocking HTTP/1.1 loop (POSIX) meant for preview.
> The production reactor — io_uring/epoll, keep-alive, HTTP/2, routing — is the
> `net/` layer scheduled for [Phase 2](PLAN.md).

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
