#pragma once
/// \file layout.hpp
/// waya's layout system \u2014 powerful, correct, and INTRINSICALLY RESPONSIVE.
///
/// Two ideas, both proven:
///   \u2022 SwiftUI's sizing intent: a node either HUGS its content or FILLS the
///     available space. `fill`, `hug`, `spacer()` say which \u2014 directly.
///   \u2022 Every Layout's primitives (every-layout.dev): composable containers that
///     adapt to available space by THEMSELVES, with no media-query breakpoints.
///     A `cluster` wraps when it must; a `switcher` flips row\u2192column below a
///     threshold; `grid` fits as many columns as room allows. Truly responsive.
///
/// Everything here is just a node with the right mods, so it composes with the
/// whole vocabulary. Layout is not a special system \u2014 it's more nodes.

#include "node.hpp"

#include <string>
#include <vector>

namespace waya::surface {

// ── Sizing intent (SwiftUI-style) ───────────────────────────────────
/// `flexible` — take all available space on the main axis (SwiftUI's "fill"; a
/// Spacer that also holds content). Content-sized is the default. (`fill` is a
/// Len for widths: `w(fill)` = 100%; `hug` is a Len meaning content-sized.)
inline const Mod flexible = grow(1);
/// A flexible spacer that pushes siblings apart. `row(a, spacer(), b)`.
inline NodeRef spacer(){ auto n = box(); n->style.has_grow=true; n->style.grow=1; finalize(*n); return n; }

// ── Fluid type & space (clamp \u2014 responsive without breakpoints) ─────────────
/// `fluid_font(min,max)` \u2014 font-size that scales with the viewport between two
/// bounds. `text("Hi") | fluid_font(24, 48)` grows on big screens, shrinks on
/// small \u2014 no media queries.
inline Mod fluid_font(float min_px, float max_px){
    return detail::raw_css("font-size", "clamp(" + detail::numstr(min_px) + "px,"
        + detail::numstr(min_px/16.f) + "rem + 2vw," + detail::numstr(max_px) + "px)");
}
/// `fluid(minLen, idealVw, maxLen)` \u2014 a general clamp() length for any prop.
inline std::string fluid(std::string min_, std::string ideal, std::string max_){
    return "clamp(" + min_ + "," + ideal + "," + max_ + ")";
}

// ═══════════════════════════════════════════════════════════════════════════
//  Every Layout primitives \u2014 responsive by construction, no breakpoints.
// ═══════════════════════════════════════════════════════════════════════════

namespace detail { inline void collect(std::vector<NodeRef>&){}
template<typename...R> void collect(std::vector<NodeRef>& v, NodeRef n, R...r){ v.push_back(std::move(n)); collect(v,std::move(r)...); } }

// STACK is `col` (already defined) — vertical flow with consistent spacing.

/// CLUSTER \u2014 items flow horizontally and WRAP onto new lines as space runs out.
/// Tags, chips, button groups, nav items. Intrinsically responsive: it wraps by
/// itself, no breakpoints. (Every Layout's Cluster.)
template <typename... Cs> NodeRef cluster(Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto n = box(); n->kids = std::move(k);
    n->style.flow = Flow::row; n->style.wrap = Wrap::wrap;
    n->style.align = Align::center; n->style.gap = px(12);
    finalize(*n); return n;
}

/// CENTER \u2014 horizontally centre a column of content with a comfortable max
/// width; it shrinks to fit on narrow screens. (Every Layout's Center.)
template <typename... Cs> NodeRef center_col(Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto n = box(); n->kids = std::move(k); n->style.flow = Flow::col;
    n->style.max_w = px(760); n->style.extra.emplace_back("margin-inline","auto");
    finalize(*n); return n;
}

/// SIDEBAR \u2014 a fixed-ish rail beside a fluid main area. When there isn't room,
/// they STACK by themselves (no breakpoint). `sidebar(nav, main, 16rem)`.
/// (Every Layout's Sidebar, via flex-wrap + flex-basis.)
inline NodeRef sidebar(NodeRef side, NodeRef main_, Len side_w = rem(16)){
    // side: FIXED width — don't grow (flex-grow:0), basis = side_w. A growing
    // side rail would eat the free space and defeat the point of a sidebar.
    side->style.has_grow=false; side->style.has_shrink=true; side->style.shrink=1;
    side->style.extra.emplace_back("flex-grow", "0");
    side->style.extra.emplace_back("flex-basis", [&]{ std::string o; switch(side_w.unit){
        case Unit::px:o=std::to_string((int)side_w.value)+"px";break; case Unit::rem:o=std::to_string((int)side_w.value)+"rem";break; default:o="16rem"; } return o; }());
    finalize(*side);
    main_->style.has_grow=true; main_->style.grow=999;
    main_->style.extra.emplace_back("flex-basis","60%");
    main_->style.min_w = px(0);   // allow shrink → triggers the wrap
    finalize(*main_);
    auto n = box(); n->kids = {side, main_};
    n->style.flow = Flow::row; n->style.wrap = Wrap::wrap; n->style.gap = px(16);
    finalize(*n); return n;
}

/// SWITCHER \u2014 lay children in a row, but FLIP to a column when the container is
/// narrower than `threshold`. Container-based, no media query. (Every Layout's
/// Switcher, via flex-basis + min().)
template <typename... Cs> NodeRef switcher(Len threshold, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    std::string th = std::to_string((int)threshold.value) + (threshold.unit==Unit::rem?"rem":"px");
    for (auto& c : k){ c->style.has_grow=true; c->style.grow=1;
        c->style.extra.emplace_back("flex-basis", "calc((" + th + " - 100%) * 999)");
        finalize(*c); }
    auto n = box(); n->kids = std::move(k);
    n->style.flow = Flow::row; n->style.wrap = Wrap::wrap; n->style.gap = px(16);
    finalize(*n); return n;
}

/// GRID \u2014 as many equal columns as fit at >= `min_col` wide, re-flowing by
/// itself. The one-liner responsive card grid. (auto-fit + minmax.)
template <typename... Cs> NodeRef grid(Len min_col, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    std::string mc = std::to_string((int)min_col.value) + (min_col.unit==Unit::rem?"rem":"px");
    auto n = box(); n->kids = std::move(k);
    n->style.extra.emplace_back("display","grid");
    n->style.extra.emplace_back("grid-template-columns","repeat(auto-fit,minmax(min(" + mc + ",100%),1fr))");
    n->style.gap = px(16);
    finalize(*n); return n;
}

/// HERO — fill the viewport height and centre the content vertically. Great for
/// landing/hero sections. (Every Layout's Cover.)
template <typename... Cs> NodeRef hero(Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto n = box(); n->kids = std::move(k); n->style.flow = Flow::col;
    n->style.justify = Justify::center; n->style.min_h = Len{100, Unit::vh};
    finalize(*n); return n;
}

// ── fixed grids ─ the primitive that makes TABLES + card grids ALIGN ────────
// Faking columns with a per-row `row(grow(1)…)` never aligns — each row splits
// its own free space. A real grid gives every cell a SHARED column track, so
// headers and values line up down the page. Cells are the grid's direct kids.
//
// The grid MODS (grid_cols/grid_rows/grid_areas/auto_grid/col_span/row_span/
// area) live in node.hpp as first-class Flow::grid vocabulary. `columns()` below
// is the convenience BUILDER for the most common case: a fixed n-column table.

/// `columns(n, cells…)` — a fixed n-column grid box; the cells align down the
/// page. `columns(3, header0,header1,header2, a0,a1,a2, b0,b1,b2)` is a table.
template <typename... Cs> NodeRef columns(int n, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto box_ = box(); box_->kids = std::move(k);
    box_->style.flow = Flow::grid;
    box_->style.extra.emplace_back("grid-template-columns","repeat("+std::to_string(n)+",minmax(0,1fr))");
    finalize(*box_); return box_;
}

} // namespace waya::surface
