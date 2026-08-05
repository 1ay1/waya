# waya — Design

> A C++26 server-side web framework with a type-state compile-time HTML DSL,
> a static/dynamic template diff engine, and an Elm-style runtime.
>
> maya renders a **cell grid** to a terminal. waya renders a **DOM tree** to a browser.
> Same philosophy, different substrate.

---

## 0. The thesis

Web development has settled into an uncomfortable equilibrium. The server is
where the data lives, the client is where the user lives, and the industry's
answer has been to ship a compiler, a virtual machine, and a megabyte of
framework across the gap so that both halves can pretend to be one program.
Every mainstream answer — React Server Components, LiveView, Livewire, Hotwire,
htmx — is a different negotiation of the same treaty.

waya makes a specific bet: **the treaty is fine, but the language on the server
side is wrong.** Elixir gave LiveView soft-realtime processes. Rust gave Leptos
fine-grained reactivity and a borrow checker. But nobody has brought the one
thing C++26 uniquely offers — *a type system powerful enough to make an entire
class of web bugs fail to compile* — to the server-rendering problem.

The pitch to a web developer is not "C++ is faster." They have heard that and
correctly concluded that they do not care enough to trade `npm install` for
CMake. The pitch is:

> **Your template will not compile if the HTML is wrong.**
> Not "will warn." Not "will fail a linter." Will not compile.
>
> A `<td>` outside a `<tr>`, a `<div>` inside a `<p>`, an `href` on a `<span>`,
> an unclosed tag, an unescaped interpolation into an `onclick` attribute, a
> form input with no `name`, an `<img>` with no `alt` — every one of these is a
> type error, reported at the call site, before your program exists.

That is a guarantee no JS/TS framework can make, because JSX types check
*props*, not *content models*. `<p><div/></p>` is valid TSX and invalid HTML;
the browser silently reparses it into something you did not write and your
hydration mismatches at 3am. In waya it is a compile error with a message that
names the offending pair.

This is the maya insight, transposed. maya's headline is "you cannot set a
border color without a border." That is a small, almost cute guarantee. But it
demonstrates the mechanism, and the mechanism generalises to something with
real economic value on the web: **the HTML5 content model, encoded in the type
system, enforced at zero runtime cost.**

---

## 1. What we are porting from maya

I read maya's source before designing this. The mechanisms worth carrying over,
and what each becomes on the web:

| maya (terminal) | waya (web) | Why it transfers |
|---|---|---|
| `t<"Hi"> \| Bold \| border_<Round>` — type-state DSL over `BoxCfg` NTTP | `p_<"Hi"> \| cls<"lead"> \| id_<"x">` — type-state DSL over `ElemCfg` NTTP | Same `operator\|` + `requires` machinery, richer invariants |
| `requires (Cfg.has_border)` gates `bcol` | `requires (Content<Parent>::allows<Child>)` gates nesting | The exact same trick, applied to the HTML content model |
| `Element = variant<Box, Text, List, ListRef, Component>` | `Node = variant<Elem, Text, Raw, Fragment, NodeRef, Island>` | Plain-data tree, `std::visit`, no virtuals |
| Flexbox layout solver → cell grid | *No layout engine.* CSS is the layout engine | The browser already has a better one than we could write |
| SIMD cell diff → minimal ANSI writes | Static/dynamic template split → minimal JSON patch | Same goal (minimal bytes on a slow wire), different unit |
| `CacheId` content-hash memoisation | `CacheId` on subtrees + HTTP `ETag` | Directly reusable, same code |
| `Signal`/`Computed`/`Effect` | Same, but server-side, per-session | Directly reusable |
| `Program{init,update,view,subscribe}` + `Cmd`/`Sub` | Same, but `Msg` arrives over WebSocket | Directly reusable |
| Witness Chain (typed proof objects) | Witness Chain for escaping + patch soundness | The doctrine is the most valuable thing maya has |
| `run<P>()` on a terminal | `serve<P>()` on a socket | Same shape |

And the things we deliberately **do not** port: the flexbox solver, the cell
grid, SIMD, ANSI serialisation, unicode width tables. The browser owns layout
and painting. Trying to out-CSS CSS is how framework authors lose.

---

## 2. Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│  User code                                                            │
│    struct Dashboard { Model; Msg; init; update; view; subscribe; }     │
│    view() -> Node   using namespace waya::dsl;                        │
├──────────────────────────────────────────────────────────────────────┤
│  DSL layer            waya/dsl.hpp                                     │
│    div_<>, p_<>, tr_<>, input_<> ... — one type per HTML element       │
│    ElemCfg NTTP carries id/class/attrs/flags                          │
│    Content-model concepts gate composition          COMPILE TIME      │
├──────────────────────────────────────────────────────────────────────┤
│  Node layer           waya/node/                                       │
│    Node = variant<Elem, Text, Raw, Fragment, NodeRef, Island>          │
│    Attrs (interned), CacheId (content hash)                            │
├──────────────────────────────────────────────────────────────────────┤
│  Template layer       waya/render/template.hpp        ★ THE CORE      │
│    A view() is split into STATICS (never change, hashed once) and      │
│    DYNAMICS (the holes). First render = full HTML. Every render after  │
│    = only the changed holes, as a sparse tree-path-keyed patch.        │
├──────────────────────────────────────────────────────────────────────┤
│  Render pipeline      waya/render/                                     │
│    render_full()  -> HTML bytes  (SSR, first paint, crawlers)          │
│    render_diff()  -> Patch       (WebSocket, subsequent)               │
│    Escaping is type-enforced by context (HTML/attr/URL/JS/CSS)         │
├──────────────────────────────────────────────────────────────────────┤
│  Runtime              waya/app/                                        │
│    Session = one connected client = one Model + one signal graph       │
│    Elm loop: event -> Msg -> update -> view -> diff -> patch           │
│    Cmd<Msg> (effects as data), Sub<Msg> (timers, pubsub, streams)      │
├──────────────────────────────────────────────────────────────────────┤
│  Transport            waya/net/                                        │
│    HTTP/1.1 + HTTP/2, WebSocket, SSE. io_uring / epoll / kqueue / IOCP │
│    Router with typed path params: route<"/user/{id:int}">              │
├──────────────────────────────────────────────────────────────────────┤
│  Client runtime       waya.js  (~6 KB, hand-written, no build step)    │
│    Applies patches, forwards DOM events, morphs, restores focus        │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. The DSL — impossible HTML doesn't compile

### 3.1 What it looks like

```cpp
#include <waya/waya.hpp>
using namespace waya;
using namespace waya::dsl;

Node view(const Model& m) {
    return div_(
        h1_<"Dashboard"> | cls<"title">,
        when(m.loading,
            div_<"Loading…"> | cls<"spinner">,
            table_(
                thead_(tr_(th_<"Name">, th_<"Status">, th_<"Latency">)),
                tbody_(
                    each(m.rows, [](const Row& r) {
                        return tr_(
                            td_(text(r.name)),
                            td_(text(r.status)) | cls_if(r.down, "err"),
                            td_(text(r.ms, "ms"))
                        ) | key(r.id);          // keyed for stable diffing
                    })
                )
            ) | cls<"grid">
        ),
        button_<"Refresh"> | on_click<Refresh{}> | cls<"btn">
    ) | cls<"page">;
}
```

Note what is absent: no closing tags to mismatch, no string concatenation, no
manual escaping, no `dangerouslySetInnerHTML`, no template-language grammar to
learn. It is C++ expressions all the way down, and `view` is a pure function.

### 3.2 The mechanism (ported directly from maya)

maya encodes element state in a structural NTTP and gates transitions with
`requires`. From `reference/maya/include/maya/dsl.hpp:543`:

```cpp
// TYPE-STATE: border color requires border — applying bcol without border is
// a compile error. The requires clause enforces the state machine transition.
template <FlexDirection Dir, BoxCfg Cfg, typename... Cs, uint8_t R, uint8_t G, uint8_t B>
    requires (Cfg.has_border)
constexpr auto operator|(BoxNode<Dir, Cfg, Cs...> n, BColTag<R, G, B>) { ... }
```

waya uses the identical shape, but the predicate consults the **HTML content
model** instead of a border flag:

```cpp
// Every HTML element is a distinct type carrying a compile-time config.
template <Tag T, ElemCfg Cfg, typename... Children>
struct ElemNode {
    std::tuple<Children...> children;
    [[nodiscard]] Node build() const;
};

// The HTML5 content categories, as a bitset NTTP.
enum class Category : uint32_t {
    Metadata = 1u<<0, Flow = 1u<<1, Sectioning = 1u<<2, Heading = 1u<<3,
    Phrasing = 1u<<4, Embedded = 1u<<5, Interactive = 1u<<6, Palpable = 1u<<7,
    ScriptSupporting = 1u<<8,
};

// One trait per element, generated from the WHATWG spec table.
template <Tag T> struct ElementTraits;

template <> struct ElementTraits<Tag::p> {
    static constexpr auto categories   = Category::Flow | Category::Palpable;
    static constexpr auto permitted    = Category::Phrasing;   // content model
    static constexpr bool void_element = false;
    static constexpr auto attrs        = GlobalAttrs;
};

template <> struct ElementTraits<Tag::td> {
    static constexpr auto categories   = Category::None;       // no category!
    static constexpr auto permitted    = Category::Flow;
    static constexpr auto parents_only = tags<Tag::tr>;         // strict parent
    static constexpr auto attrs        = GlobalAttrs | attrs<"colspan","rowspan","headers">;
};

// The gate. This single concept is the whole guarantee.
template <Tag Parent, typename Child>
concept PermittedChild =
       (ElementTraits<Parent>::permitted & categories_of<Child>) != Category::None
    && parent_ok<Parent, Child>;
```

and the child-accepting factory refuses to instantiate otherwise:

```cpp
template <Tag T, ElemCfg Cfg = {}, typename... Cs>
    requires (!ElementTraits<T>::void_element)
          && (PermittedChild<T, Cs> && ...)
constexpr auto elem(Cs... cs) {
    return ElemNode<T, Cfg, Cs...>{{std::move(cs)...}};
}
```

The error message matters as much as the check. We surface it through a
dedicated diagnostic type so the compiler prints something a web developer can
act on:

```cpp
template <Tag Parent, Tag Child>
struct invalid_nesting {
    static_assert(dependent_false<Parent>,
        "waya: <" + name_of<Child>() + "> is not permitted inside <"
                  + name_of<Parent>() + ">. "
        "The HTML5 content model for <" + name_of<Parent>() + "> permits: "
                  + categories_of<Parent>() + ". "
        "See https://html.spec.whatwg.org/#the-" + name_of<Parent>() + "-element");
};
```

C++26's `static_assert` with a *constant-expression message* (P2741) makes this
possible — the diagnostic is computed, not a fixed literal. This is the single
highest-leverage C++26 feature for this project.

### 3.3 The full invariant list

Each of these is a compile error, not a runtime check:

| Invariant | Mechanism |
|---|---|
| `<div>` inside `<p>` | content-model concept |
| `<td>` outside `<tr>` | `parents_only` trait |
| `<li>` outside `<ul>`/`<ol>`/`<menu>` | `parents_only` |
| Children given to a void element (`<img>`, `<br>`) | `requires (!void_element)` on `elem()` |
| `href` on `<span>`, `src` on `<div>` | per-element permitted attribute set |
| Interactive content nested in `<a>` or `<button>` | `Category::Interactive` exclusion |
| `<img>` with no `alt` | required-attribute check at `.build()` |
| `<input>`/`<select>` inside a form with no `name` | required-attribute check |
| `<label>` with neither `for` nor a wrapped control | cross-child structural check |
| Two `<h1>`…`<h6>` heading-order violations | opt-in a11y lint tier |
| Duplicate `id` in one subtree | `consteval` id-set accumulation |
| Unescaped interpolation into `onclick=` / `href="javascript:"` | escaping-context type (§5) |
| A `Signal<T>` read outside a reactive scope | `Effect` scope token |

The last three are the ones that actually bite in production, and no template
language on the market catches them.

### 3.4 Escape hatch, on purpose

A framework that only allows valid HTML and offers no exit is a framework that
loses to reality on day three. waya's exit is explicit and greppable:

```cpp
raw_html(trusted_string)   // bypasses the content model AND escaping
unsafe::any_child          // bypasses only the content model
```

Both are named so that `grep -r 'raw_html\|unsafe::'` is a complete audit of
your escaping surface. maya has the same instinct (the raw `Canvas` API next to
the safe DSL).

---

## 4. The rendering core — static/dynamic splitting

This is the piece that makes waya competitive rather than merely elegant, and
it is a direct translation of maya's cell diff into the DOM domain.

### 4.1 The problem

maya's terminal is a slow wire; writing the whole screen every frame flickers
and burns bandwidth, so maya packs cells into 64-bit words and SIMD-compares
frames to emit only changed cells. The web's wire is slower still. Sending the
whole page on every state change is what made 2005-era server rendering feel
bad, and it is why LiveView's central innovation is not "server rendering" but
**the static/dynamic split**.

### 4.2 The insight, stolen from LiveView and improved by C++

A template is a sequence of literal chunks interleaved with holes:

```
"<div class='row'><span>"  ⟨name⟩  "</span><b>"  ⟨count⟩  "</b></div>"
 └────── static 0 ──────┘           └── static 1 ──┘         └ static 2 ┘
```

The statics **never change for the lifetime of the program**. So:

- **First render** — send statics + dynamics, assembled into full HTML.
  Also send the statics *once*, keyed by template id, so the client caches them.
- **Every render after** — send only the dynamics that actually differ:
  `{"3": {"1": "42"}}` meaning "template 3, hole 1, new value 42."

LiveView computes this split with an Elixir macro at compile time. waya
computes it with `constexpr` evaluation at compile time — and gets something
Elixir cannot: the statics are `constexpr std::string_view`s baked into
`.rodata`, and the template id is a *type*, so a wrong-template patch is
unrepresentable rather than merely unlikely.

```cpp
// Produced by consteval analysis of the DSL tree — zero runtime cost.
template <TemplateId Id>
struct Template {
    static constexpr std::array<std::string_view, N> statics = { ... };
    static constexpr std::array<HoleDesc, M>        holes    = { ... };
    static constexpr uint64_t                       fingerprint = fnv1a(statics);
};
```

### 4.3 Nested templates and the comprehension optimisation

The split composes. A component is a nested template, so a change deep inside a
list row emits a patch addressed by tree path (`"0.3.1.2"`) and nothing else
moves. For lists (`each(...)`), waya emits a **comprehension**: the statics are
sent once for the whole list, not once per row — a 1000-row table costs one
copy of the row markup plus 1000 tuples of dynamics. This is exactly LiveView's
`%Comprehension{}` and it is the difference between a 2 MB and a 40 KB payload.

### 4.4 Where waya beats LiveView

1. **Memoisation by content hash.** maya already has this — `CacheId` in
   `reference/maya/include/maya/render/cache_id.hpp`, a typed content hash
   built by `CacheIdBuilder{}.add(...).build()`, deliberately *not* a
   hand-written string key (the file's comment documents the ghost-render bugs
   that hand-written keys caused). waya reuses it verbatim: a subtree whose
   `CacheId` is unchanged is skipped before it is ever rendered, so the diff
   cost is proportional to what changed, not to page size.

2. **Zero-allocation render path.** Statics are `string_view` into `.rodata`.
   Dynamics render into a per-session arena that is reset, not freed. A steady-
   state frame in maya performs zero heap allocations; waya holds the same bar.
   This is what a GC'd runtime structurally cannot do, and it is the difference
   between 50 k and 500 k concurrent sessions on one box.

3. **The patch is typed.** A `Patch` carries its `TemplateId` and hole indices
   are `constexpr`-bounded, so "server and client disagree about the shape of
   the template" is caught by the fingerprint on connect, not by a corrupted
   DOM.

---

## 5. Escaping as a type, not a function call

XSS is the web's buffer overflow: a *context confusion* bug. A string safe in
HTML text is unsafe in an attribute, which is unsafe in a URL, which is unsafe
in JS. Every framework's answer is "we escape by default," which is true right
up until the value lands in `href` or `onclick`.

waya makes the context part of the type:

```cpp
template <EscapeContext Ctx> struct Escaped { std::string_view s; };

using HtmlText = Escaped<EscapeContext::HtmlText>;
using AttrVal  = Escaped<EscapeContext::Attribute>;
using UrlVal   = Escaped<EscapeContext::Url>;
using JsVal    = Escaped<EscapeContext::Script>;
using CssVal   = Escaped<EscapeContext::Style>;

// An attribute slot accepts only AttrVal. Interpolating a raw std::string
// is a compile error; interpolating HtmlText is a compile error.
// The conversion runs the correct escaper — there is no way to skip it.
template <> struct AttrSlot<"href"> { using accepts = UrlVal; };
```

`href="javascript:alert(1)"` fails because `UrlVal`'s `consteval` constructor
rejects non-allowlisted schemes when the value is a literal, and its runtime
constructor rejects them at conversion. The result: **there is no code path
from a user string into the document that does not pass through the right
escaper**, and that fact is enforced by the compiler rather than by review.

This is precisely maya's Witness Chain doctrine (`docs/internals/witness-chain.md`)
applied to injection: construct a move-only proof object at the one place the
property can be verified, and make every consumer demand the proof.

---

## 5.5 Styling — owned by waya, expressed in the DSL, never CSS

This is the section the terminal heritage matters most for, and the one place a
naive port would ruin the framework. **The requirement (from the project owner):
no CSS in the DSL, and "the rendering and everything about style should be
maya-style too" — waya owns styling the way maya owns it.**

### 5.5.1 Why "just copy maya's `CTStyle`" is a trap

maya's style struct has exactly eight fields — `fg`, `bg`, `bold`, `dim`,
`italic`, `underline`, `strike`, `inverse` — because **a terminal cell only
*has* those properties.** maya owns the renderer, so `Fg<100,180,255>` becomes
an ANSI SGR byte sequence that maya writes itself. There is no CSS, no cascade,
no layout engine external to maya — maya's `FlexStyle` *is* a flexbox
reimplementation, because a terminal has none.

A `<div>` has ~400 style properties. If waya hardcodes maya's eight, every real
user hits the wall on day one (no flex, no grid, no radius, no shadow, no
media queries, no `:hover`) and reaches for a `raw_css()` escape hatch — and
now the framework has shipped the worst of both worlds. **"Like maya" cannot
mean "maya's eight fields." It must mean maya's four *properties of the
approach*:**

1. Style is a value you pipe with `|` — no separate `.css` file, no selector.
2. Composed and resolved at **compile time**, zero runtime cost.
3. **Type-state safe** — nonsensical styles do not compile.
4. **Deterministic merge** — right operand wins (maya's `Style::merge`).

### 5.5.2 The mechanism: CSS is waya's SGR

Here is the exact structural correspondence, traced from maya's real code
(`reference/maya/include/maya/element/box.hpp`):

```
maya:  view() → tree of BoxElement{ FlexStyle layout; Style visual; }
              → renderer walks tree, runs flex solver, PAINTS CELLS
              → ANSI/SGR bytes → terminal

waya:  view() → tree of Elem{ Sty style; }
              → renderer walks tree, INTERNS each Sty
              → atomic class names + one generated stylesheet → browser
```

The substitution is at the very last step and only there: a browser's only
programmable surface is CSS, so waya's renderer serialises each node's `Sty`
into CSS. **CSS is to waya what ANSI/SGR is to maya: a private output encoding
the renderer emits, not a language the author touches.** You never write a
selector, never open a `.css` file, never fight a cascade. Exactly as a maya
user never hand-writes `\x1b[1;38;2;100;180;255m`.

### 5.5.3 What the author writes

```cpp
using namespace waya::style;

div_(
    h1_(text("Dashboard")) | fg<0x3B82F6> | bold | size<28_px>,
    row_(
        button_(text("−")) | pad<8_px> | bg<0x1E293B> | rounded<6_px>,
        button_(text("+")) | pad<8_px> | bg<0x1E293B> | rounded<6_px>
    ) | gap<12_px>
)
| col_ | gap<16_px> | pad<24_px> | bg<0x0F172A> | rounded<12_px>
```

No CSS. Styles pipe with `|`, merge left-to-right, resolve at compile time —
maya's ergonomics precisely. The vocabulary spans **every** category (colour,
full box model, flex, grid, typography, radius, shadow, opacity, transitions,
pseudo-classes, media queries), not eight fields — so it does not limit you.

### 5.5.4 Type-state: maya's border rule, transposed onto the box model

Because the style is a typed value, waya catches CSS bugs that *no stylesheet
and no Tailwind can*:

```cpp
p_(...) | gap<8_px>                 // ERROR: gap requires a flex/grid container
row_(...) | gap<8_px>              // OK   — row_ makes it a flex container
span_(...) | width<100_px>         // ERROR: width is ignored on inline elements
... | justify<Center>              // ERROR without a flex context
... | size<-4_px>                  // ERROR: negative length
... | fg<"bleu">                   // ERROR: not a colour
```

`gap` without a flex/grid container is the single most common real CSS bug, and
waya makes it a **compile error**. The mechanism is identical to the spike's
content-model gate and to maya's `requires (Cfg.has_border)` guarding
border colour: `gap`/`justify`/`align` read `Sty::is_flex_ctx()` in a consteval
check, and a non-flex style fails a `static_assert` with a waya-authored
sentence. **Proven working** — see §5.5.6.

### 5.5.5 The renderer owns output — compile-time atomic-CSS interning

The naive translation (inline `style="…"` per element) would bloat HTML and
break the §4 diff engine. So waya does exactly what maya does with its
**`StylePool`** (maya interns every `Style` to a `uint16_t` so cells stay 8
bytes for SIMD diffing): waya interns every unique `Sty` reachable from
`view()` to a stable, content-hashed class name.

- Identical styles anywhere in the tree collapse to **one class** — `pad<16_px>`
  used 500 times is *one* rule `.wa-7c2{padding:16px}`, not 500 inline copies.
- The framework emits **one deduplicated stylesheet** for the whole page.
- The stylesheet is a compile-time constant (`.rodata`), caches forever behind
  an `ETag`, and — crucially — class names are **statics**, so the §4
  static/dynamic split is preserved: changing a value re-points a class and the
  60-byte patch stays 60 bytes.

This is **atomic CSS generated at compile time** — the Tailwind model, but
type-checked, with zero build step, zero PostCSS, zero purge pass.

### 5.5.6 Proven, not promised

A second spike (`spike/waya_style.hpp` + `waya_emit.hpp` + `test_style.cpp`,
run by `./spike/run_style.sh`) demonstrates the whole chain on GCC 16,
**6/6 green**:

```
$ ./spike/run_style.sh
  PASS  compiles, Elm loop runs, all assertions hold
  PASS  renderer emitted ZERO inline styles (atomic classes only)
  PASS  identical styles interned to a single class/rule
  PASS  gap without flex (1 error, 4 lines)
  PASS  justify without flex (1 error, 4 lines)
  PASS  gap WITH flex compiles (the gate lets valid styles through)
  passed: 6   failed: 0
```

Actual output from the spike's styled counter app (author wrote zero CSS):

```html
<div class="wa-578439"><h1 class="wa-490593">…</h1>
  <div class="wa-28d967">
    <button class="wa-0b325d">−</button>
    <button class="wa-0b325d">+</button>   <!-- SAME class: interned -->
  </div></div>
```
```css
.wa-578439{display:flex;flex-direction:column;background:#0f172a;padding:24px;gap:16px;border-radius:12px}
.wa-490593{color:#3b82f6;font-size:28px;font-weight:700}
.wa-28d967{display:flex;flex-direction:row;gap:12px}
.wa-0b325d{background:#1e293b;padding:8px;border-radius:6px}
```

The two buttons carry structurally-equal `Sty` values and therefore **share one
class** — the interning is real, and it is the same trick as maya's StylePool.

### 5.5.7 The one honest tradeoff (and the seam that absorbs it)

CSS is a moving target — new properties ship in browsers constantly, so waya's
*named* vocabulary (`fg`, `pad`, `flex`, …) will always trail the spec by a
little. That is fine, because the named tokens are only **sugar**: the
foundation is a **universal channel** that makes *any* CSS a clean pipe, so
nothing is ever off-limits. This is the "general enough like maya" guarantee —
maya gives you a complete low-level vocabulary and never picks styles for you;
waya does the same for the web.

```cpp
// Any property, even ones waya has never heard of — typed, interned, diffed:
... | prop<"backdrop-filter", "blur(8px)">
... | prop<"grid-template-columns", "repeat(3, 1fr)">
... | var_<"--brand", "#3b82f6"> | prop<"color", "var(--brand)">
... | prop_dyn("width", std::to_string(w) + "px")   // runtime value

// States and responsive are values too — not a separate, weaker API:
... | on<Hover>(bg(0x2563eb))                       // .wa-x:hover{...}
... | on<Focus>(prop<"outline", "2px solid #93c5fd">)
... | at<Md>(pad(16_px))                            // @media (min-width:768px)

// And the existing CSS ecosystem still transfers verbatim:
... | css_class<"legacy-thing">                     // a Tailwind / design-system class
```

Crucially, everything the universal channel produces is **still interned and
diffed** — two buttons with the same base *and* the same `:hover` share one
class and one rule set (tested). The named tokens exist only so the common 90%
reads beautifully; the universal channel guarantees the other 10% is never a
wall. This mirrors maya exactly: `Bold` is sugar over `Style{.bold=true}`, and
you can always drop to the raw `Style`. **Proven working** —
`tests/test_style_general.cpp`, 14/14, covers arbitrary props, custom
properties, pseudo-classes, media queries, grid, and interning-with-states.

---

## 6. Interactivity — the three-tier ladder

The biggest strategic error a new web framework can make is forcing one
interactivity model. waya offers three, in the same DSL, chosen per component.

**Tier 0 — static.** `render_full()` to bytes. Blog, docs, marketing. No JS at
all, not even the runtime. Output is byte-identical across runs (important for
CDN caching and for tests).

**Tier 1 — hypermedia (htmx-style).** Attributes drive partial GETs/POSTs; the
server returns HTML fragments. No session state, horizontally scalable behind a
dumb load balancer, degrades to working forms with JS off.

```cpp
button_<"Load more"> | get<"/items?page=2"> | swap<Append, "#list">
```

**Tier 2 — live (LiveView-style).** A stateful WebSocket session with the Elm
loop and the diff engine. For dashboards, chat, collaborative editing,
real-time streams.

```cpp
struct Chat {
    struct Model { std::vector<Line> lines; std::string draft; };
    using Msg = std::variant<Typed, Sent, Received>;
    static auto update(Model, Msg) -> std::pair<Model, Cmd<Msg>>;
    static Node view(const Model&);
    static auto subscribe(const Model&) -> Sub<Msg> {
        return topic<Msg>("room:lobby", [](Line l){ return Received{l}; });
    }
};
serve<Chat>({.route = "/chat", .port = 8080});
```

**Islands.** Tier-2 components can be embedded in a Tier-0/1 page — the donut
model. `island<Cart>(props)` renders server-side for first paint, then upgrades
to a live socket, while everything around it stays inert HTML. Children pass
through the hole untouched.

The ladder means adoption is incremental: a team can start at Tier 0 for a docs
site and reach Tier 2 for one widget without rewriting.

---

## 7. The runtime — Elm, ported

Directly reused from maya, because the design is already right. A `Program` is:

```cpp
init()               -> pair<Model, Cmd<Msg>>
update(Model, Msg)   -> pair<Model, Cmd<Msg>>
view(const Model&)   -> Node
subscribe(const Model&) -> Sub<Msg>
```

`update` is pure and therefore testable with `operator==` and no server:

```cpp
auto [m, cmd] = Chat::update(Model{.draft="hi"}, Sent{});
EXPECT_EQ(m.lines.size(), 1);
EXPECT_EQ(cmd, Cmd<Msg>::broadcast("room:lobby", ...));
```

`Cmd<Msg>` is a variant of effect descriptions (`none`, `quit`, `batch`,
`after`, `task`, plus web-specific `navigate`, `push_state`, `set_cookie`,
`broadcast`, `fetch`). `Sub<Msg>` declares event sources (timers, pub/sub
topics, file watches, DB change streams) and is reconciled each frame — a
subscription that disappears from the returned `Sub` is torn down
automatically, which is how you avoid the leak that every hand-rolled
WebSocket app eventually has.

Signals (`Signal<T>`/`Computed<T>`/`Effect`) are available for fine-grained
updates inside a session, reusing maya's implementation including its batching
and its reentrancy-safe notification frames.

**Session model.** One connected client = one `Session` = one `Model` + one
arena + one signal graph, pinned to a thread (share-nothing, like BEAM
processes, but without the copying). Sessions are `co_await`-driven C++26
coroutines on an io_uring/epoll reactor. A crashed session takes down only
itself and reconnects with state rebuilt from `init()` plus the URL — the
LiveView recovery model.

---

## 8. Ergonomics — the actual adoption problem

Everything above is worthless if `npm create` has no equivalent. Explicit
non-negotiables:

- **`waya new app && waya dev`** — starts a hot-reloading dev server. No CMake
  authoring required to begin. The CLI vendors a preconfigured build.
- **Hot reload under 200 ms.** This is the make-or-break number. Achieved by
  compiling only `view()` TUs, keeping the runtime in a stable shared library,
  and preserving `Model` across reloads. If a change takes 8 seconds, no web
  developer will stay, regardless of guarantees.
- **Header-only core, single `find_package(waya)`.** Vendored deps, no system
  package hunting.
- **Errors that read like TypeScript's, not like Boost's.** Concept names are
  user-facing copy. Budget real time here; this is the single biggest
  determinant of whether the type-state DSL feels like a superpower or a
  hazing ritual.
- **Batteries:** router with typed params, form decoding into structs (via
  C++26 reflection — a `struct LoginForm` becomes a validated parser for free),
  sessions/cookies/CSRF, auth scaffolding, DB access, i18n, asset pipeline with
  content-hashed URLs, structured logging, OpenTelemetry.
- **Markdown + math for free.** maya already ships a Markdown engine and a TeX
  typesetter; the parser/AST is reusable and only the backend changes (cells →
  DOM). Docs sites work on day one.

---

## 9. C++26 features this design depends on

| Feature | Paper | Used for |
|---|---|---|
| Static reflection | P2996 | Form/JSON decoding into structs, DB row mapping, auto-derived `Msg` serialisation, admin scaffolding |
| `static_assert` with computed message | P2741 | The readable content-model diagnostics of §3.2 |
| Structural NTTPs / class types as NTTP | C++20, leaned on hard | `ElemCfg`, `Str<N>` literals as template params |
| Expansion statements | P1306 | Iterating children/attrs at compile time without recursion |
| `constexpr` containers | P0784 et al | Building the statics table at compile time |
| Coroutines + senders/receivers | P2300 | The async reactor, `Cmd::task` |
| Deducing `this` | P0847 | Fluent builder chains without CRTP |
| Pack indexing | P2662 | Child-tuple manipulation |

Reflection is the one that changes the *scope* of the project. With it, waya
can generate a validated form parser, a JSON codec, and an admin CRUD UI from a
plain struct — the productivity story that makes Rails-refugees look twice.
Compiler support (Clang/EDG forks today, shipping through 2026) is the main
schedule risk, so the design keeps reflection behind a feature flag with a
macro-based fallback for the first release.

---

## 10. Honest risk assessment

I would rather write this down now than discover it in month nine.

> **Update — risks 1 and 2 are now measured, not guessed.** A working spike
> lives in `spike/` (GCC 16, `-std=c++26`). `./spike/run_spike.sh` reports
> **21/21 passing**: 14 invalid-HTML cases correctly rejected, a 1163-element
> page compiling in **739 ms**, and every diagnostic reduced to **one error in
> ≤ 6 lines**. Details in §10.1.

1. **Compile times.** A deeply templated DSL over hundreds of element types can
   explode. Mitigations: element types are declared, not instantiated, until
   used; the content model is a bitmask compare, not a type-list search;
   `extern template` for the runtime; the "one TU per view" hot-reload
   architecture. **Gate: a 500-line page must compile in under 2 seconds.**
   ✅ **Measured: 701 elements in 466 ms, 1163 elements in 739 ms** — roughly
   0.63 ms/element, scaling linearly, ~2.7× inside the gate.

2. **Error message quality.** Type-state DSLs are famous for 400-line
   diagnostics. **Gate: every invariant has a golden-file test asserting the
   exact message.** ✅ **Measured: 1 error, 5–6 lines, naming both elements and
   linking the spec.** Getting there required two non-obvious tricks (§10.1) —
   this was the single most valuable thing the spike taught.

3. **"Why would I write C++ for this?"** The answer must be a *demo*, not an
   argument: a live dashboard with 100 k concurrent WebSocket sessions on one
   $40 VPS, next to the same app in LiveView and Next.js, with a memory graph.
   Plus the XSS/invalid-HTML compile errors shown as a 30-second GIF. Ship the
   benchmark with the announcement or do not announce.

4. **Ecosystem.** No auth libraries, no ORM, no component libraries. Mitigation:
   ship a first-party batteries set (§8) and a Tailwind-compatible class story
   so the existing CSS ecosystem transfers unchanged.

5. **Memory safety.** Pitching "safe by construction" in a language with UAF is
   a fair critique. Mitigation: the safety claim is *scoped* and precise — HTML
   correctness, escaping, and protocol soundness are type-enforced; memory
   safety comes from arenas, value semantics, no raw owning pointers in the
   API, and CI under ASan/UBSan/TSan with fuzzing on every parser. Say exactly
   this, never "C++ is safe."

6. **Scope.** This document describes a very large system. PLAN.md sequences it
   so that something demoable exists early and each phase is independently
   useful.

### 10.1 What the spike proved (and what it changed)

The spike (`spike/waya_dsl.hpp`, ~430 lines) implements a 29-element subset
with the full content model, attribute pipes, escaping, and a constexpr
renderer. Three findings changed the design:

**Finding 1 — whole documents render at compile time.** This works today:

```cpp
constexpr auto page = html_(head_(title_(text("Hi"))), body_(p_(text("hello"))));
static_assert(render_document(page) ==
    "<!DOCTYPE html><html><head><title>Hi</title></head>"
    "<body><p>hello</p></body></html>");
```

A fully static page costs **zero work at runtime** — it is a `.rodata` string.
That is a stronger claim than "fast SSR" and it falls out of the design for
free. It also validates the §4 static/dynamic split: if the whole tree folds at
compile time, so does the statics table.

**Finding 2 — keep the NTTP config tiny, or diagnostics explode.** The first
draft of `ElemCfg` held the attribute values inline as `std::array<char,256>`.
GCC prints the full NTTP value in *every* instantiation frame, so a one-line
error arrived wrapped in 31 lines of `std::array<char, 256>()`. Moving values
out of the type (config keeps `bool` flags; values ride as `string_view`
members pointing at the `Str<S>` template parameter's static storage) cut the
noise immediately. **Design rule: nothing goes in an NTTP unless a `requires`
clause needs to read it.**

**Finding 3 — assert on a `bool` inside a type, never on a concept in a
function.** Two mechanisms that look equivalent are not:

```cpp
// 37 lines: overload resolution failure + substitution dump
template <typename... Cs> requires (PermittedChild<T, Cs> && ...)
constexpr auto p_(Cs...);

// 15 lines: GCC appends the concept-satisfaction derivation
static_assert(PermittedChild<T, Child>, nesting_error<T, Child>());

// 6 lines: one sentence, ours
template <Tag P, typename C, bool Ok = PermittedChild<P, C>>
struct CheckChild { static_assert(Ok, nesting_error<P, C>()); ... };
static_assert((CheckChild<T, Cs>::value && ...));
```

The factory must **accept** any child and diagnose in the body — a `requires`
clause makes the function invisible to overload resolution, and the compiler
then explains its whole search instead of our message. Asserting on a plain
`bool` (not the concept) suppresses the satisfaction derivation; triggering via
*type instantiation* rather than a constexpr call suppresses the
"in constexpr expansion of…" frames that would otherwise name every node in
the tree.

This has a shipping consequence: waya's CMake preset must set
`-ftemplate-backtrace-limit=1` and `-fno-diagnostics-show-caret`. The
framework authors its own errors; the compiler's backtrace is noise.

**What the spike did not cover** (deferred to Phase 1+): the remaining ~80
elements, ARIA/a11y invariants, the escaping-context types of §5, the
static/dynamic template split of §4, and anything on the network. The two
things it *did* cover are the two that could have killed the project.


---

## 11. What "impressive" means here

The framework should be judged on whether a web developer, shown a 40-line
example, says *"wait, it caught that?"* Three moments to engineer for:

1. They nest a `<div>` inside a `<p>` out of habit and get a one-line error
   quoting the WHATWG spec.
2. They interpolate a user string into `href` and the compiler stops them.
3. They open devtools, click around a live dashboard, and see 60-byte
   WebSocket frames where React would have sent a re-render.

Those three moments are the product. Everything in §11's plan exists to make
them possible.

---

*Build sequencing and milestones: see [PLAN.md](PLAN.md).*
