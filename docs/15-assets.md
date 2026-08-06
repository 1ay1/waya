# Assets & Global CSS

Most styling in waya hangs off a node — a colour, a size, a hover rule, all
applied with `|`. But some things are inherently **document-scoped**: a web
font must be declared once, a custom `@keyframes` can't live on a single node,
`:root` design tokens are global by definition. The **asset registry** is the
core's seam for exactly these.

It's what makes the core *closed* under "build anything": paired with the
per-node escape hatches (`css`, `attr`, `var`, `as`, `markup`), there is no
piece of a web document waya can't express from your own code.

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

// Register once (at startup, or the first time a component is used):
assets().font_face("Inter", "/fonts/Inter.woff2");          // @font-face
assets().root_var("--brand", "#6366f1");                    // :root token
assets().keyframes("wobble",                                // @keyframes
    "0%{transform:none}50%{transform:rotate(6deg)}100%{transform:none}");
assets().css("::selection{background:#6366f1;color:#fff}"); // any global rule
assets().head("<link rel=\"icon\" href=\"/favicon.svg\">"); // raw <head> markup
```

The live runtime folds everything registered into every served page's `<head>`.
There is nothing else to wire up.

## The API

`assets()` returns the process-global registry. All methods dedupe, so
registering the same asset from a component used 100 times emits it once.

| Method | Emits | Deduped by |
|--------|-------|------------|
| `keyframes(name, spec)` | `@keyframes name{spec}` | name (first wins) |
| `font_face(family, src, weight="400", style="normal")` | `@font-face{…}` | family+src+weight+style |
| `root_var(name, value)` | a `:root{--name:value}` entry | name (last wins) |
| `css(rule)` | any global CSS rule verbatim | exact text |
| `head(html)` | raw markup in `<head>` | exact text |

`root_var` is **last-write-wins**, so an app can override a component library's
default token. `keyframes` is **first-write-wins**, so a component's canonical
animation is stable no matter how many times it's used.

## Animating with a custom keyframe

`custom_animation` registers a keyframe **and** applies it in one call — the
ergonomic bridge, so a component ships its own animation without you touching
anything global:

```cpp
box() | custom_animation("wobble",
    "0%{transform:none}50%{transform:rotate(6deg)}100%{transform:none}",
    /*ms=*/600, /*ease=*/"ease", /*fill=*/"both", /*iter=*/"infinite");
```

This is the seam that makes the built-in motion vocabulary open-ended: anything
you can write in CSS keyframes, you can animate. (The built-in entrances —
`fade_in`, `pop_in`, `slide_in`, `spin`, `pulse`, … — use the same mechanism
against a small keyframe library the shell always ships.)

## Reduced motion is respected for free

The shell emits a `@media (prefers-reduced-motion: reduce)` rule that neutral­
ises animations and transitions for users who ask for it. Your custom keyframes
and animations are covered automatically — accessibility, by default.

## Design tokens end to end

`root_var` and the [theme token system](04-styling.md) are two views of the same
`:root` custom properties. Use `theme(t)` for a full palette bound to the
`--wa-*` names the token mods read; use `root_var` for one-off tokens you
reference yourself:

```cpp
assets().root_var("--radius-lg", "20px");
card | css("border-radius", "var(--radius-lg)");
```
