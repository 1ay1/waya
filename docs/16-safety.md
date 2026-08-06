# Correctness & Safety

waya's founding bet is that the server language should make an entire class of
web bugs *impossible*, not merely discouraged. Three mechanisms deliver that:
structural validation, context-aware escaping, and URL sanitisation. They cost
nothing in production and catch the mistakes that actually ship.

## Structural validation

A surface is a runtime tree, so waya validates it against the parts of the
WHATWG content model that bite. In a **debug build** the live runtime checks the
tree on every render and prints any violation to stderr — so a malformed UI
fails loudly the first time you see it, not silently in front of a user. In a
release build the walk is skipped entirely (zero cost).

You can also run it yourself, which is ideal in tests:

```cpp
#include <waya/surface/validate.hpp>

assert(verify(view(model)));          // true when structurally sound
std::string report = explain(view(model));   // human-readable violations
auto vs = check(*view(model));        // std::vector<Violation> for programmatic use
```

The rules:

| Rule | Catches |
|------|---------|
| `form-control-name` | an `input`/`select`/`textarea` in a `form` with no `name` — its value would never reach `update()` |
| `nested-interactive` | a tap target / button / link inside another one (invalid HTML; the browser splits your DOM) |
| `void-element` | children on a leaf primitive (`image`/`input`/`checkbox`/`radio`/`path`) |
| `img-alt` | an image with no `alt` text (accessibility) |
| `orphan-option` | a stray `<option>` not built through `select(...)` |

These are the "wait, it caught that?" moments — the mistakes every web dev makes
on autopilot, surfaced immediately with a message that tells you the fix.

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

The one explicit opt-out is `markup(html)`, which emits raw trusted HTML. It's
named to signal danger — only pass content you control.

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
