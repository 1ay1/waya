# The Typed Dialect — maya, for the web

The browser is a terminal. maya proved you can build a whole TUI from a tiny,
composable, *type-safe* vocabulary where nonsensical styles don't compile. The
typed dialect (`<waya/surface/typed.hpp>`, namespace `waya::tui`) brings that
same guarantee to web UI: **a layout mistake is a compile error, not a silent
no-op.**

It's opt-in and layers on the same runtime — same `Node`, same diff, same
`live()`. You just get compile-time gates, typed colours, and length literals.

```cpp
#include <waya/surface/live.hpp>
#include <waya/surface/typed.hpp>

using namespace waya::tui;                // Row/Col/Box/Text + gated mods
using namespace waya::color;              // typed colours: indigo, rgba(...)
using namespace waya::surface::literals;  // 16_px, 1.5_rem

static NodeRef view(const Model& m) {
    return Col(
        Text("waya") | fg(muted) | tracking(6.f) | uppercase,
        Text(m.n) | font(84_px) | bold | tabular_nums,
        Row(btn("-"), btn("+")) | gap(12) | justify_center
    ) | gap(28) | align_center | pad(48_px) | bg(rgba(11,16,32,1));
}
```

## Type-state layout gates

`gap`, `justify`, `align`, `wrap` only mean anything on a **flex or grid
container**. In plain CSS (and in the untyped `waya::surface` vocabulary) putting
them on the wrong element is a silent no-op — the #1 class of real CSS bug. In
the typed dialect it's a compile error with a one-line message:

```cpp
Text("hi") | gap(12)            // ✗ compile error:
// waya: this style needs a different layout context.
// gap/justify/align/wrap need a flex or grid container (Row/Col/Grid) ...

Row(a, b) | gap(12)             // ✓ Row is a flex context
Grid(a) | gap(8) | justify_center   // ✓ Grid is a grid context
```

How it works (the type theory): a builder returns a `Styled<Ctx>` — a `NodeRef`
tagged with a **phantom context type** (`ctx::Flex`, `ctx::Grid`, `ctx::Block`,
`ctx::Inline`) describing the layout context it establishes. A gated mod carries
the requirement it needs (`req::Container`). The `|` operator is constrained:
when the context satisfies the requirement it applies; when it doesn't, a
`static_assert` fires the message. The tag is phantom — **zero runtime cost**,
erased to a bare `NodeRef` the moment you hand it to the runtime.

The typed builders:

| Builder | Context | Gates that apply |
|---------|---------|------------------|
| `Row(...)` `Col(...)` `Stack(...)` | `Flex` | `gap`, `justify_*`, `align_*`, `wrap_*` |
| `Grid(...)` | `Grid` | `gap`, `justify_*`, `align_*` |
| `Box(...)` | `Block` | box-model mods; **not** container mods |
| `Text(...)` | `Inline` | typography; **not** container mods |

Alignment is expressed as **named values** (`justify_center`, `justify_between`,
`align_center`, `align_stretch`, `wrap_on`…) rather than `justify(Enum)` — reads
like maya's named style values and keeps the gate clean.

!!! note "Why grow/shrink aren't gated"
    `grow`/`shrink` are flex-*item* properties: they depend on the *parent* being
    a container, which a child's own type can't see. CSS ignores `flex-grow` on a
    non-item harmlessly, so gating on the child would be wrong. They apply in any
    context.

## Typed colours

Colours are values, not magic hex. `<waya/color.hpp>`:

```cpp
using namespace waya::color;

fg(indigo)                  // a named palette colour
bg(rgb(0x141b2e))           // from hex
bg(rgba(0, 0, 0, 0.4f))     // with alpha (rides the css channel automatically)
fg(hsl(210, 0.9f, 0.5f))    // HSL, constexpr
fg(indigo.lighten(0.2f))    // tint/shade/mix, all constexpr
```

`fg`/`bg` accept both a `Color` and a bare `std::uint32_t` (back-compat). An
opaque `Color` uses the fast interned paint path; a translucent one preserves
alpha via the style channel. Palette: `ink`, `muted`, `indigo`, `violet`,
`cyan`, `sky`, `emerald`, `amber`, `rose`, `red`, plus the `slate*` ramp.

## Length literals

```cpp
using namespace waya::surface::literals;

pad(16_px)  round(12_px)  w(50_pct)  h(100_vh)  font(1.25_rem)  gap_rem(1.5)
```

`12_px`, `1.5_rem`, `50_pct`, `100_vw`, `100_vh`, `1_fr` — a size is
self-documenting at the call site, no `px(...)` wrapper.

## The complete vocabulary (so you never touch `css()`)

The named mods cover the everyday cases the raw `css()` escape hatch used to
leak into: `min_w`/`max_w`/`min_h`/`max_h`, `leading`/`tracking`,
`truncate`/`line_clamp(n)`, `uppercase`/`lowercase`/`capitalize`,
`tabular_nums`, `no_select`/`no_pointer`, `aspect`, and the full box/flex set.
`css("prop","value")` remains the universal escape hatch — but for a normal
view you shouldn't need it. When you do, it's still interned and diffed like any
other mod; nothing is off-limits.

## Mixing typed and untyped

`Styled<Ctx>` converts implicitly to `NodeRef`, so typed and untyped trees
compose freely — a typed `Row` can contain an untyped `card(...)`, and a typed
node drops straight into `live<App>()`. Use the dialect where you want the
guarantees; reach for plain `waya::surface` where you don't. Same runtime either
way.
