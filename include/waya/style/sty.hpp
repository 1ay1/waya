#pragma once
/// \file sty.hpp
/// `Sty` — the compile-time style value carried on every node.
///
/// The maya `CTStyle`+`FlexStyle` analogue, but with a *complete* web
/// vocabulary rather than a terminal cell's eight fields. Composed with `|`,
/// merged deterministically (right wins), resolved at compile time. The
/// renderer serialises this to CSS — the author never writes CSS. See
/// DESIGN.md §5.5.
///
/// Phase 1 carries a representative-but-growing slice. Adding a property is:
/// one field + one has-flag here, one line in the CSS serialiser, and (if it's
/// container-only) a note in the flex/grid type-state gate.

#include "length.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace waya::style {

enum class Display : std::uint8_t { unset, block, inline_, inline_block, flex, grid, none };
enum class Dir     : std::uint8_t { unset, row, col, row_rev, col_rev };
enum class Wrap    : std::uint8_t { unset, nowrap, wrap, wrap_rev };
enum class Justify : std::uint8_t { unset, start, center, end, between, around, evenly };
enum class Align   : std::uint8_t { unset, start, center, end, stretch, baseline };
enum class Weight  : std::uint8_t { unset, w100, w200, w300, normal, w500, w600, bold, w800, w900 };
enum class TextAlign : std::uint8_t { unset, left, center, right, justify };
enum class Position  : std::uint8_t { unset, static_, relative, absolute, fixed, sticky };
enum class Cursor    : std::uint8_t { unset, pointer, default_, text, wait, not_allowed };

/// The style value. All-scalar/enum so it stays a small, printable NTTP
/// (DESIGN §10.1 rule: nothing large in a structural template parameter).
struct Sty {
    Display display   = Display::unset;
    Position position = Position::unset;

    // colour — sentinels behind has-flags (0x000000 is a legal colour)
    bool has_fg = false;      std::uint32_t fg = 0;
    bool has_bg = false;      std::uint32_t bg = 0;

    // box model
    bool has_pad = false;     Len pad{};
    bool has_pad_x = false;   Len pad_x{};
    bool has_pad_y = false;   Len pad_y{};
    bool has_margin = false;  Len margin{};
    bool has_w = false;       Len w{};
    bool has_h = false;       Len h{};
    bool has_max_w = false;   Len max_w{};
    bool has_min_w = false;   Len min_w{};
    bool has_radius = false;  Len radius{};
    bool has_border_w = false;Len border_w{};
    bool has_border_c = false;std::uint32_t border_c = 0;

    // flex / grid container
    Dir     direction = Dir::unset;
    Wrap    wrap      = Wrap::unset;
    Justify justify   = Justify::unset;
    Align   align     = Align::unset;
    bool    has_gap   = false; Len gap{};

    // flex item
    bool has_grow   = false; int grow = 0;
    bool has_shrink = false; int shrink = 1;

    // typography
    Weight    weight     = Weight::unset;
    bool      has_size   = false; Len size{};
    bool      has_lh     = false; Len line_height{};  // rem/em/number-as-em
    bool      italic     = false;
    bool      underline  = false;
    TextAlign text_align = TextAlign::unset;

    // effects / misc
    bool   has_shadow  = false;
    bool   has_opacity = false; int opacity_pct = 100;
    Cursor cursor      = Cursor::unset;

    // General channel — the "not limiting" guarantee. ANY CSS property/value,
    // pseudo-classes, and media queries the typed fields don't cover live here
    // as raw declarations. Populated by `prop<>`, `on<>`, `at<>`. Because this
    // is a runtime member (not part of the NTTP), it costs nothing in
    // diagnostics and imposes no ceiling on what you can express.
    std::vector<std::pair<std::string, std::string>> extra;   ///< (property, value)
    std::vector<std::pair<std::string, std::string>> states;  ///< (":hover{...}", body) etc.

    bool operator==(const Sty& o) const = default;

    /// Is this node a flex or grid container? Gates gap/justify/align.
    [[nodiscard]] bool is_container() const {
        return display == Display::flex || display == Display::grid
            || direction != Dir::unset;
    }
    /// Is this the default (unstyled) value? Then we emit no class at all.
    [[nodiscard]] bool empty() const { return *this == Sty{}; }
};

/// Merge: `b` overlays `a`, field by field. Right operand wins. Deterministic —
/// the maya `Style::merge` contract, so `| bold | weight(normal)` is `normal`.
constexpr Sty merge(Sty a, const Sty& b) {
    if (b.display  != Display::unset)  a.display  = b.display;
    if (b.position != Position::unset) a.position = b.position;
    if (b.has_fg)       { a.has_fg = true;       a.fg = b.fg; }
    if (b.has_bg)       { a.has_bg = true;       a.bg = b.bg; }
    if (b.has_pad)      { a.has_pad = true;      a.pad = b.pad; }
    if (b.has_pad_x)    { a.has_pad_x = true;    a.pad_x = b.pad_x; }
    if (b.has_pad_y)    { a.has_pad_y = true;    a.pad_y = b.pad_y; }
    if (b.has_margin)   { a.has_margin = true;   a.margin = b.margin; }
    if (b.has_w)        { a.has_w = true;        a.w = b.w; }
    if (b.has_h)        { a.has_h = true;        a.h = b.h; }
    if (b.has_max_w)    { a.has_max_w = true;    a.max_w = b.max_w; }
    if (b.has_min_w)    { a.has_min_w = true;    a.min_w = b.min_w; }
    if (b.has_radius)   { a.has_radius = true;   a.radius = b.radius; }
    if (b.has_border_w) { a.has_border_w = true; a.border_w = b.border_w; }
    if (b.has_border_c) { a.has_border_c = true; a.border_c = b.border_c; }
    if (b.direction != Dir::unset)     a.direction = b.direction;
    if (b.wrap      != Wrap::unset)    a.wrap = b.wrap;
    if (b.justify   != Justify::unset) a.justify = b.justify;
    if (b.align     != Align::unset)   a.align = b.align;
    if (b.has_gap)      { a.has_gap = true;      a.gap = b.gap; }
    if (b.has_grow)     { a.has_grow = true;     a.grow = b.grow; }
    if (b.has_shrink)   { a.has_shrink = true;   a.shrink = b.shrink; }
    if (b.weight != Weight::unset)     a.weight = b.weight;
    if (b.has_size)     { a.has_size = true;     a.size = b.size; }
    if (b.has_lh)       { a.has_lh = true;       a.line_height = b.line_height; }
    if (b.italic)       a.italic = true;
    if (b.underline)    a.underline = true;
    if (b.text_align != TextAlign::unset) a.text_align = b.text_align;
    if (b.has_shadow)   a.has_shadow = true;
    if (b.has_opacity)  { a.has_opacity = true;  a.opacity_pct = b.opacity_pct; }
    if (b.cursor != Cursor::unset)     a.cursor = b.cursor;
    for (const auto& e : b.extra)  a.extra.push_back(e);
    for (const auto& s : b.states) a.states.push_back(s);
    return a;
}

} // namespace waya::style
