# Correctness & Safety

waya's founding bet is that the server language should make an entire class of
web bugs *impossible*, not merely discouraged. Three mechanisms deliver that:
structural validation, context-aware escaping, and URL sanitisation. They cost
nothing in production and catch the mistakes that actually ship.

## Structural validation

A surface is a runtime tree, so waya validates it against the parts of the
WHATWG content model — and the waya invariants — that bite. The guarantee is
enforced at **three escalating tiers**, so you choose how loud a violation is:

1. **Debug — log.** In a debug build the live runtime checks every render and
   prints any violation to stderr. A malformed UI fails loudly the first time
   you see it, not silently in front of a user. Release builds skip the walk
   entirely (zero cost).
2. **Strict — abort.** Build with `-DWAYA_STRICT=ON` (or `#define WAYA_STRICT`)
   and the runtime *refuses to render* an invalid tree: it prints every
   violation and aborts before the tree is ever diffed or sent. This is the
   switch that makes “impossible UIs can’t ship” literal rather than advisory —
   turn it on in CI and staging.
3. **Compile — reject.** For a component whose shape is *fixed* (no data-driven
   branching), `WAYA_STATIC_CHECK(cond)` turns a decidable invariant into a
   hard compile error, so a malformed shell never even links.

You can also run the validator yourself, which is ideal in tests:

```cpp
#include <waya/surface/validate.hpp>

assert(verify(view(model)));                 // true when structurally sound
assert_valid(view(model));                   // aborts with a report on any violation
std::string report = explain(view(model));   // human-readable violations
auto vs = check(view(model));                // std::vector<Violation> for tooling

// compile-time invariant for a fixed-shape factory:
NodeRef icon_button(){
    WAYA_STATIC_CHECK(!waya::surface::detail::is_void(Kind::button));
    return button("x") | tap(Close{});
}
```

The rules:

| Rule | Catches |
|------|---------|
| `form-control-name` | an `input`/`select`/`textarea`/`checkbox`/`radio` in a `form` with no `name` — its value would never reach `update()` |
| `nested-interactive` | a tap target / button / link inside another one (invalid HTML; the browser splits your DOM) |
| `void-element` | children on a leaf primitive (`image`/`input`/`checkbox`/`radio`/`path`) |
| `img-alt` | an image with no `alt` text (accessibility) |
| `orphan-option` | a stray `<option>` not built through `select(...)` |
| `empty-select` | a `select` with no options — an unusable, unchoosable dropdown |
| `duplicate-key` | two sibling nodes sharing a `key` — silently corrupts the keyed-list move-diff |
| `dead-handler` | an event handler wired to no real `Msg` — a click that does nothing |
| `markup-unsafe` | `markup(...)` whose raw HTML carries a `<script>` / `javascript:` / `on*=` handler — almost always user input in the trusted channel |

These are the "wait, it caught that?" moments — the mistakes every web dev makes
on autopilot, surfaced immediately with a message that tells you the fix. The
`duplicate-key` and `dead-handler` rules in particular catch *silent* bugs: the
UI renders, but interactions are quietly lost — exactly the failures the surface
model exists to prevent.

## Context-aware escaping

The DOM backend escapes every value for the context it lands in — you never
hand-escape anything:

- **Text content** escapes `& < >`.
- **Attribute values** additionally escape `"` and `'`, so a value containing a
  quote can never break out of the attribute and inject markup. This covers
  `attr(...)`, `name(...)`, `src`, `placeholder`, `value`, `<option>` values —
  every attribute channel.

```cpp
text("x") | attr("title", user_supplied)   // safe: quotes become &quot;
```

The one explicit opt-out is `markup(html)`, which emits raw trusted HTML — only
pass content you control. If you need to render rich HTML from an *untrusted*
source (markdown output, CMS/user content), reach for `sanitized_html(html)`
instead: it strips `<script>`/`<style>`/`<iframe>` blocks, inline `on*=` event
handlers, and `javascript:` URLs, so formatting survives but active content
can't. And the `markup-unsafe` validator rule (above) catches the case where
active content slipped into `markup()` by accident.

```cpp
markup(trusted_svg)                 // you control this — fine
sanitized_html(rendered_markdown)   // untrusted — scripts/handlers stripped
```

## URL sanitisation

A `javascript:` (or `data:`/`vbscript:`) URL in an `href` or `src` runs code
even after HTML escaping. Use `href(url)` / `link_to(label, url)` for links built
from data you don't fully control — they neutralise dangerous schemes to `#`:

```cpp
link_to("Profile", user.website)      // javascript:… becomes #
text("Docs") | href("/docs")          // safe, renders a real <a>
safe_url(untrusted)                    // the primitive, if you need it directly
```

## What waya does *not* claim

The safety guarantee is precise and honest. waya makes **HTML structure,
escaping, and patch/protocol soundness** correct by construction. It does not
claim "C++ is memory-safe"; that comes from the usual discipline — value
semantics, no raw owning pointers, and CI under ASan/UBSan/TSan.
