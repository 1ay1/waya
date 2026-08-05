# waya — Build Plan

Sequencing for the architecture in [DESIGN.md](DESIGN.md). Every phase ends in
something runnable and demoable; no phase is pure infrastructure.

**Guiding rule:** prove the risky mechanism before building the comfortable
scaffolding around it. The two things that can kill this project are compile
times and error-message quality, so both get measured from Phase 0 and gate
every subsequent phase.

---

## Phase 0 — Spike: prove the DSL ✅ **DONE**

**Goal:** answer "does the type-state HTML content model actually work in C++26,
and what does it cost?" before writing a framework around it.

Delivered in `spike/` — two spikes now. Run `./spike/run_spike.sh` (content
model) and `./spike/run_style.sh` (styling + Elm). GCC 16, `-std=c++26`.

**Spike #1 — content model** (`waya_dsl.hpp`, `test_spike.cpp`):

- [x] `Tag` enum + `Traits<T>` for a 29-element subset
- [x] `Cat` bitmask + `PermittedChild` concept
- [x] `ElemNode<Tag, ElemCfg, Children...>` + `operator|` for `cls`/`id`/`href`
- [x] `render()` / `render_document()` — **constexpr**, so pages fold to `.rodata`
- [x] Automatic, unskippable HTML escaping
- [x] Golden error tests — 14 invalid-HTML cases, each asserted to fail
- [x] Compile-time benchmark at 176 / 701 / 1163 elements
- [x] P2741 computed diagnostics naming both elements + linking the spec

**Spike #2 — styling + Elm architecture** (`waya_style.hpp`, `waya_emit.hpp`,
`test_style.cpp`) — answers "styling like maya, no CSS in the DSL":

- [x] `Sty` — a complete web style vocabulary as a structural NTTP (the maya
      `CTStyle`+`FlexStyle` analogue, not limited to 8 terminal fields)
- [x] Styles piped with `|`, merged compile-time, right-operand-wins
- [x] Type-state: `gap`/`justify`/`align` require a flex container (compile
      error otherwise) — maya's border-colour rule, transposed
- [x] The renderer **owns output**: interns identical `Sty` to one atomic class
      and emits one deduplicated stylesheet — no inline styles, no `.css` file
- [x] A full Elm loop (`Model`/`Msg`/`init`/`update`/`view`) drives the tree

**Exit gates — all met:**

| Gate | Target | Measured |
|---|---|---|
| 500-element page compile time | < 2000 ms | **466 ms** (701 elems); 739 ms at 1163 |
| Diagnostic length | ≤ 5 lines | **5–6 lines, exactly 1 error** |
| Invalid HTML rejected | all | **14 / 14** |
| Byte-exact HTML output | yes | **`static_assert`-verified at compile time** |

```
$ ./spike/run_spike.sh
  ...
  passed: 21   failed: 0
```

**Two findings that changed the design** (full write-up in DESIGN.md §10.1):

1. Keep NTTP configs tiny — attribute values inside the config type inflated
   every diagnostic by 25 lines. Values now travel as members, flags in the type.
2. Never gate a factory with `requires` if you want a readable error. Accept the
   child, then assert on a plain `bool` inside a helper *type*. This is the
   difference between 37 lines and 6.

Both are now permanent rules for Phases 1+.

---

## Phase 1 — The node layer and full SSR (3–4 weeks)

**Goal:** a complete, correct, fast HTML renderer. Static-site quality.

- [ ] All ~110 HTML5 elements, traits generated from the WHATWG spec table by a
      script (`tools/gen_elements.py` → `element_traits.inc`) — hand-writing
      them guarantees drift
- [ ] Per-element attribute allowlists + required-attribute checks (`alt`, `name`)
- [ ] `Node = variant<Elem, Text, Raw, Fragment, NodeRef, Island>`
- [x] **Styling (DESIGN.md §5.5)** — the `Sty` vocabulary (colour, box model,
      flex, grid, typography, radius, shadow, transitions, pseudo-classes,
      media queries), the `|` pipe with merge, the flex/grid type-state gates,
      the interning `StyleSheet` (one deduplicated stylesheet), and the
      universal `prop<>`/`var_<>`/`on<>`/`at<>` channel (any CSS, one pipe)
- [x] **Layout components** — `row`/`col`/`stack`/`cluster`/`center`/`grid`/
      `grid_auto`/`sidebar`/`spacer`/`divider`: any layout in one call,
      responsive by default, still fully pipeable (test_layout, 22 assertions)
- [x] **Runtime-data combinators** — `each`/`each_indexed`/`when`/`dyn`/`raw`,
      type-erased but content-model-checked (test_dynamic, 17 assertions)
- [x] **Dev server + live reload** — `waya::serve()`, `scripts/dev.sh`
- [x] **Generic attributes + events** — `attr<"name","value">`, `flag<>`,
      `attr_dyn`/`flag_if`, `on_<>`/`on_click<>`/`on_input<>` — any attribute,
      boolean attr, data-*, ARIA, DOM event; values escaped (test_attrs, 23)
- [ ] Escaping-context types (`HtmlText`/`AttrVal`/`UrlVal`/`JsVal`/`CssVal`)
      with `consteval` URL-scheme validation
- [ ] Attribute interning (maya's `StylePool` pattern → `AttrPool`)
- [ ] `key()`, `cls_if()` polish; `unsafe::` for content-model bypass
- [ ] `tools/gen_elements.py` → all ~110 elements + attribute allowlists
- [ ] Arena allocator for the render path; **zero allocations in steady state**
- [ ] Fuzz the renderer (output must always be well-formed, always escaped)
- [ ] `CacheId` ported from maya, subtree memoisation

**Demo:** a static site generator. Build a docs site with it, host it, dogfood.
**Gate:** renders a 1 MB page faster than a comparable Go/Rust templating engine.

---

## Phase 2 — HTTP server and Tier-1 hypermedia (4–5 weeks)

**Goal:** real apps with forms and partial updates. No WebSocket yet.

- [ ] Reactor: io_uring (Linux) / kqueue (BSD, macOS) / IOCP (Windows), epoll fallback
- [ ] HTTP/1.1 + HTTP/2, keep-alive, chunked, compression (gzip/brotli/zstd)
- [ ] `route<"/user/{id:int}/posts/{slug}">` — typed path params, compile-time
      validated against the handler signature
- [ ] Form/query decoding into structs (reflection path + macro fallback)
- [ ] Cookies, sessions, CSRF, security headers
- [ ] **Streaming SSR**: `co_yield` chunks, out-of-order shell-then-slots
- [ ] Tier-1 attributes (`get`/`post`/`swap`/`target`/`trigger`) + the client
      shim that implements them (~2 KB)
- [ ] Static asset serving with content-hashed URLs and `ETag`

**Demo:** a CRUD app (todo/blog) with validated forms, no custom JS.
**Gate:** > 200 k req/s plaintext on a laptop; TechEmpower-comparable numbers.

---

## Phase 3 — The diff engine (4–6 weeks) ★ the hard part

**Goal:** the static/dynamic split. This is the phase that makes waya more than
"a fast templating library."

- [ ] `consteval` analysis of a DSL tree → `Template<Id>{statics, holes}`
- [ ] Template registry + fingerprints; client-side statics cache
- [ ] `Patch` type: sparse, tree-path-keyed, `TemplateId`-tagged
- [ ] Nested templates (components as sub-templates)
- [ ] **Comprehensions**: list statics sent once, not per row
- [ ] Keyed list reconciliation (insert/remove/move without re-sending rows)
- [ ] Integrate `CacheId` — skip unchanged subtrees before rendering them
- [ ] Property-test: `apply(patch, old_html) == render_full(new_state)` for
      randomly generated state transitions. This is the correctness backbone;
      fuzz it hard.
- [ ] Witness types for patch soundness (maya's Witness Chain doctrine)

**Gate:** a 1000-row table changing one cell emits **< 100 bytes**.

---

## Phase 4 — Tier-2 live sessions (5–6 weeks)

**Goal:** LiveView-class interactivity.

- [ ] WebSocket (RFC 6455) + SSE fallback
- [ ] `Session`: `Model` + arena + signal graph, thread-pinned, coroutine-driven
- [ ] Elm loop: `Msg` → `update` → `view` → diff → patch
- [ ] `Cmd<Msg>` ported + web effects (`navigate`, `push_state`, `set_cookie`,
      `broadcast`, `fetch`); `Sub<Msg>` with reconciliation
- [ ] Signals ported from maya (batching, reentrancy-safe notify frames)
- [ ] `waya.js` client (~6 KB, no build step): patch application, DOM morphing,
      event forwarding, focus/scroll/IME preservation, reconnect with backoff
- [ ] Pub/sub for multi-session broadcast; presence tracking
- [ ] Optimistic UI + latency compensation
- [ ] Islands: `island<C>(props)` with server-rendered first paint and
      pass-through children (the donut)
- [ ] File uploads with progress over the live socket

**Demo:** real-time collaborative dashboard + chat.
**Gate:** 100 k concurrent sessions on one 8-core box; p99 update latency < 5 ms.

---

## Phase 5 — Developer experience (4–5 weeks)

**Goal:** the phase that decides adoption. Do not treat it as polish.

- [ ] `waya new` / `waya dev` / `waya build` / `waya deploy` CLI
- [ ] **Hot reload < 200 ms** — split view TUs, stable runtime .so, `Model`
      preserved across reloads. Measure it in CI; regressions fail the build.
- [ ] Error-message audit: every invariant in DESIGN §3.3 gets a golden test
- [ ] `find_package(waya)`, vendored deps, single-command build
- [ ] Debug overlay: template ids, patch sizes, render timings, session state
- [ ] Docs site (built with waya), full API reference, 20+ runnable examples
- [ ] Test kit: `render_test()`, `simulate<Program>(msgs...)`, snapshot testing
- [ ] LSP hints / editor integration where feasible

---

## Phase 6 — Batteries (ongoing)

- [ ] Markdown + TeX math (port maya's engine; swap the cell backend for DOM)
- [ ] DB layer: connection pooling, typed queries, migrations, change streams
      feeding `Sub<Msg>`
- [ ] Auth scaffolding (sessions, OAuth, WebAuthn)
- [ ] Reflection-driven admin CRUD from a plain struct
- [ ] i18n/l10n, timezone-correct formatting
- [ ] Asset pipeline; Tailwind-compatible class handling
- [ ] Component library (accessible by construction — ARIA enforced by types)
- [ ] Observability: structured logs, OpenTelemetry, per-session profiling
- [ ] Deployment: static binary, container images, graceful restart

---

## Timeline

| Phase | Focus | Duration | Cumulative |
|---|---|---|---|
| 0 | DSL spike | ✅ done | — |
| 1 | Node layer + SSR | 3–4 wk | 4 wk |
| 2 | HTTP + hypermedia | 4–5 wk | 9 wk |
| 3 | Diff engine | 4–6 wk | 15 wk |
| 4 | Live sessions | 5–6 wk | 21 wk |
| 5 | Developer experience | 4–5 wk | 26 wk |
| 6 | Batteries | ongoing | — |

≈ **7 months to 1.0-beta** for one focused engineer. Phases 1–2 and 3–4 admit
parallelism if there are more.

---

## Repository layout

```
waya/
├── include/waya/
│   ├── waya.hpp              # umbrella public header (maya's convention)
│   ├── dsl.hpp               # the type-state DSL
│   ├── html/                 # element traits, content model, attributes
│   │   └── element_traits.inc    # GENERATED from the WHATWG spec
│   ├── node/                 # Node variant, attrs, cache_id
│   ├── escape/               # escaping-context types
│   ├── render/               # full render, template split, diff, patch
│   ├── app/                  # Program, Session, Cmd, Sub, signals
│   ├── net/                  # http, ws, router, reactor
│   └── widget/               # markdown, forms, components
├── src/                      # non-header-only implementation
├── client/waya.ts            # the ~6 KB client runtime
├── tools/gen_elements.py     # spec table → element_traits.inc
├── examples/                 # 20+ runnable apps
├── tests/                    # unit, golden-error, property, fuzz
├── bench/                    # compile-time + runtime benchmarks
└── reference/maya/           # the TUI framework we are porting from
```

---

## What to build next, concretely

Phase 0 is done and both kill-risks are retired. The next commit should start
`tools/gen_elements.py`, because hand-writing 110 element traits guarantees
drift from the spec, and the generator is what turns the spike's 29-element
subset into a real framework.

Order of work for Phase 1:

1. `tools/gen_elements.py` → `element_traits.inc` (all ~110 elements)
2. Promote `spike/waya_dsl.hpp` into `include/waya/{html,dsl}/`, keeping the
   two design rules from §Phase 0
3. The `Node` variant + type-erased runtime tree (the spike is purely static;
   real apps need `each`/`when` over runtime data)
4. Escaping-context types (DESIGN.md §5)
5. Fuzz the renderer

The spike stays in the tree as a regression test and as the reference for what
"good diagnostics" means.
