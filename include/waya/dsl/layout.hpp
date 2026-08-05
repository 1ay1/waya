#pragma once
/// \file layout.hpp
/// Layout components — maya's box model for the web. One call, responsive by
/// default, and fully composable with the style pipes (never a dead end).
///
/// The goal (per the project's north star): ANY layout should be trivial. maya
/// gives you `row({...})`, `col({...})`, `sidebar(a, b, 42)`, `grid(...)` as
/// first-class components so you never hand-count spans or repeat flex pipes.
/// waya mirrors that; CSS does the actual responsive work (flex-wrap, grid
/// auto-fit), so these stay thin, correct, and composable.
///
///   row(a, b, c)                 // side by side, wraps as it narrows
///   col(header, body, footer)    // stacked, each fills the width
///   center(content)              // centred both axes
///   cluster(tag1, tag2, tag3)    // wrap-flow of gapped items
///   grid(cards) | cols(3)        // or auto-fit: grid_auto(cards, 240_px)
///   sidebar(nav, main_, 260_px)  // fixed rail + fluid main, stacks when narrow
///
/// Every one returns a normal element, so you keep piping:
///   row(a, b) | gap(24_px) | pad(16_px) | bg(0x111827)

#include "../dsl/element.hpp"
#include "../style/tokens.hpp"

#include <string>
#include <utility>

namespace waya::dsl {

// ── row / col / stack — the core box model ──────────────────────────────────

/// Horizontal flex row that wraps as it narrows. Default gap; override freely.
template <typename... Cs>
auto row(Cs... cs) {
    return div_(std::move(cs)...)
        | style::flex(style::Dir::row) | style::wrap() | style::gap(style::px(12));
}

/// Vertical stack; each child fills the width. Default gap; override freely.
template <typename... Cs>
auto col(Cs... cs) {
    return div_(std::move(cs)...)
        | style::flex(style::Dir::col) | style::gap(style::px(12));
}

/// Vertical stack with NO default gap (control spacing per child).
template <typename... Cs>
auto stack(Cs... cs) {
    return div_(std::move(cs)...) | style::flex(style::Dir::col);
}

/// A tight, wrapping horizontal cluster (tags, chips, buttons), items centred.
template <typename... Cs>
auto cluster(Cs... cs) {
    return div_(std::move(cs)...)
        | style::flex(style::Dir::row) | style::wrap()
        | style::gap(style::px(8)) | style::items(style::Align::center);
}

/// Centre content on both axes.
template <typename... Cs>
auto center(Cs... cs) {
    return div_(std::move(cs)...)
        | style::flex(style::Dir::row)
        | style::justify(style::Justify::center)
        | style::items(style::Align::center);
}

// ── grid ───────────────────────────────────────────────────────────────

/// A CSS grid. Combine with `| cols(3)` for a fixed grid, or use `grid_auto`
/// below for a responsive one. Default gap; override freely.
template <typename... Cs>
auto grid(Cs... cs) {
    return div_(std::move(cs)...) | style::gridbox | style::gap(style::px(16));
}

/// A responsive grid: as many equal columns as fit at >= `min_col` wide, then
/// it re-flows by itself — no breakpoints. `grid_auto(cards, 240_px)`.
template <typename... Cs>
auto grid_auto(style::Len min_col, Cs... cs) {
    return div_(std::move(cs)...) | style::gridbox | style::autofit(min_col)
                                  | style::gap(style::px(16));
}

// ── sidebar ────────────────────────────────────────────────────────────

/// A fixed-width rail beside a fluid main area that stacks when there isn't
/// room — the classic sidebar, one call (Every Layout's "sidebar"). `side_w`
/// is the rail's ideal width; `main_` grows to fill the rest and wraps under
/// the rail on narrow screens.
template <typename Side, typename Main>
auto sidebar(Side side, Main main_, style::Len side_w = style::px(260)) {
    return div_(
        div_(std::move(side))
            | style::prop_dyn("flex", "1 1 " +
                [](style::Len l){ return std::to_string((int)l.value) +
                    (l.unit == style::Unit::rem ? "rem" : "px"); }(side_w)),
        div_(std::move(main_)) | style::prop<"flex", "999 1 60%">
    ) | style::flex(style::Dir::row) | style::wrap() | style::gap(style::px(16));
}

// ── spacer / divider ────────────────────────────────────────────────────

/// A flexible spacer that pushes siblings apart (maya's `space`). Put it
/// between two items in a `row` to shove them to opposite ends.
inline auto spacer() { return div_() | style::grow(1); }

/// A thin horizontal rule that inherits the current border colour. Style it
/// like anything else: `divider() | prop<"border-color", "#334155">`.
inline auto divider() {
    return hr_() | style::prop<"border", "none">
                 | style::prop<"border-top", "1px solid currentColor">
                 | style::prop<"opacity", "0.15">
                 | style::width(style::pct(100));
}

} // namespace waya::dsl
