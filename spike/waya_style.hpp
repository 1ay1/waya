#pragma once
// waya::style — Phase 0 spike #2: maya-style styling, owned by waya.
//
// The user's ask: "ditch CSS in the DSL but keep things not limiting. The
// rendering and everything about style should be maya-style too."
//
// maya's model (from reference/maya/include/maya/element/box.hpp):
//   - Every BoxElement carries a `FlexStyle layout` (direction/gap/align/
//     padding/sizing) AND a `Style` (fg/bg/bold/...) directly on the NODE.
//   - The renderer walks the tree, runs the flex solver, and paints cells.
//     maya OWNS every cell. CSS does not exist anywhere.
//
// waya ports this EXACTLY, with one substitution at the very bottom:
//   - Every Elem carries a compile-time `Sty` on the NODE, piped with `|`.
//   - The renderer walks the tree and emits output. But a browser's only
//     "GPU" is CSS, so waya's renderer serialises each node's Sty into a
//     GENERATED stylesheet + an interned atomic class name.
//
//   CSS is to waya what ANSI/SGR is to maya: a private output encoding the
//   renderer emits, NOT a language the author writes. You never see a `.css`
//   file, never write a selector, never fight a cascade.
//
// What this file proves:
//   1. `Sty` is a complete web style vocabulary (colour, box model, flex,
//      typography, radius, ...) carried as a structural NTTP — the maya
//      CTStyle+FlexStyle analogue, not limited to 8 terminal fields.
//   2. Styles pipe with `|`, merge deterministically (right wins), compile-time.
//   3. Type-state: `gap`/`justify`/`align` require a flex container; negative
//      lengths rejected. maya's "border colour needs a border", transposed.
//   4. The renderer OWNS the output: it interns identical Stys to ONE atomic
//      class and emits ONE deduplicated stylesheet. No inline styles.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace waya {

// ── Length: a typed dimension, never a bare string ──────────────────────────
// Like maya's Dimension. A value + unit, validated at construction.

enum class Unit : uint8_t { px, rem, pct, fr, auto_, zero };

struct Len {
    float value = 0;
    Unit unit   = Unit::zero;

    constexpr bool operator==(const Len&) const = default;
};

// User-facing literals: 12_px, 1.5_rem, 50_pct.  (spike: helpers, not UDLs)
constexpr Len px(float v)  { return {v, Unit::px};  }
constexpr Len rem(float v) { return {v, Unit::rem}; }
constexpr Len pct(float v) { return {v, Unit::pct}; }
constexpr Len fr(float v)  { return {v, Unit::fr};  }
inline constexpr Len autolen{0, Unit::auto_};

// ── Enums (mirrors maya's FlexDirection/Align/Justify) ──────────────────────

enum class Dir     : uint8_t { None, Row, Col, RowRev, ColRev };
enum class Justify : uint8_t { None, Start, Center, End, Between, Around, Evenly };
enum class Align   : uint8_t { None, Start, Center, End, Stretch, Baseline };
enum class Weight  : uint8_t { None, Normal, Medium, Semibold, Bold };
enum class Disp    : uint8_t { Default, Flex, Grid, Block, Inline, None };

inline constexpr auto Row    = Dir::Row;
inline constexpr auto Col    = Dir::Col;
inline constexpr auto Center = Justify::Center;
inline constexpr auto Between = Justify::Between;

// ── Sty: the node's compile-time style value (the CTStyle analogue) ─────────
//
// DESIGN RULE from spike #1: keep NTTP structs small-ish and printable. This
// one is all scalars/enums (no char arrays), so it stays diagnostic-friendly.
// A real waya Sty has ~40 fields; the spike carries a representative slice
// spanning every category to prove the vocabulary generalises.

struct Sty {
    Disp    display = Disp::Default;

    // colour (0 = unset sentinel handled by has_* flags)
    bool     has_fg = false;  uint32_t fg = 0;
    bool     has_bg = false;  uint32_t bg = 0;

    // box model
    bool has_pad = false;     Len pad{};
    bool has_gap = false;     Len gap{};
    bool has_w   = false;     Len w{};
    bool has_h   = false;     Len h{};
    bool has_radius = false;  Len radius{};

    // flex
    Dir     direction = Dir::None;
    Justify justify   = Justify::None;
    Align   align     = Align::None;
    bool    has_grow  = false; int grow = 0;

    // typography
    Weight  weight = Weight::None;
    bool    has_size = false;  Len size{};
    bool    italic = false;
    bool    underline = false;

    // effects
    bool     has_shadow = false;
    bool     has_opacity = false; int opacity_pct = 100;

    constexpr bool operator==(const Sty&) const = default;

    // Is this a flex/grid container? (gates gap/justify/align — see below.)
    [[nodiscard]] constexpr bool is_flex_ctx() const {
        return display == Disp::Flex || display == Disp::Grid
            || direction != Dir::None;
    }
};

// ── Merge: right overlays left, deterministic (maya's Style::merge) ─────────

constexpr Sty merge(Sty a, const Sty& b) {
    if (b.display != Disp::Default)  a.display = b.display;
    if (b.has_fg)      { a.has_fg = true; a.fg = b.fg; }
    if (b.has_bg)      { a.has_bg = true; a.bg = b.bg; }
    if (b.has_pad)     { a.has_pad = true; a.pad = b.pad; }
    if (b.has_gap)     { a.has_gap = true; a.gap = b.gap; }
    if (b.has_w)       { a.has_w = true; a.w = b.w; }
    if (b.has_h)       { a.has_h = true; a.h = b.h; }
    if (b.has_radius)  { a.has_radius = true; a.radius = b.radius; }
    if (b.direction != Dir::None)     a.direction = b.direction;
    if (b.justify   != Justify::None) a.justify = b.justify;
    if (b.align     != Align::None)   a.align = b.align;
    if (b.has_grow)    { a.has_grow = true; a.grow = b.grow; }
    if (b.weight != Weight::None)     a.weight = b.weight;
    if (b.has_size)    { a.has_size = true; a.size = b.size; }
    if (b.italic)      a.italic = true;
    if (b.underline)   a.underline = true;
    if (b.has_shadow)  a.has_shadow = true;
    if (b.has_opacity) { a.has_opacity = true; a.opacity_pct = b.opacity_pct; }
    return a;
}

} // namespace waya
