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
/// `fluid(minLen, idealVw, maxLen)` — a general clamp() length for any prop.
inline std::string fluid(std::string min_, std::string ideal, std::string max_){
    return "clamp(" + min_ + "," + ideal + "," + max_ + ")";
}

namespace detail { inline void collect(std::vector<NodeRef>&){}
template<typename...R> void collect(std::vector<NodeRef>& v, NodeRef n, R...r){ v.push_back(std::move(n)); collect(v,std::move(r)...); } }

// ── Filling the space ─ the mods every app needs to build a full-height shell ─
// The #1 layout confusion: how to make a region GROW to fill leftover space and
// SCROLL internally instead of pushing the page taller. These name it directly.

/// `grows` / `grows(n)` — take the leftover space on the parent's MAIN axis
/// (SwiftUI's fill). In a `col` it fills height, in a `row` it fills width.
/// Clearer than `flex_1`; the everyday "this pane should expand".
inline const Mod grows = grow(1);

/// `stretches` — stretch to fill the parent's CROSS axis (align-self: stretch).
inline const Mod stretches = detail::raw_css("align-self", "stretch");

/// `flex_col` / `flex_row` — turn a node into a flex container that DISTRIBUTES
/// space to its children AND lets a scrolling child be bounded (min-height/
/// min-width:0). This is the missing setup that makes a full-height app layout
/// 'just work': `flex_col` on the root, `grows` on the region that expands.
inline const Mod flex_col = sty([](Style& s){
    s.flow = Flow::col;
    s.extra.emplace_back("display", "flex");
    s.extra.emplace_back("flex-direction", "column");
    s.extra.emplace_back("min-height", "0");
});
inline const Mod flex_row = sty([](Style& s){
    s.flow = Flow::row;
    s.extra.emplace_back("display", "flex");
    s.extra.emplace_back("flex-direction", "row");
    s.extra.emplace_back("min-width", "0");
});

/// `vscroll()` — grow to fill and scroll VERTICALLY inside (a list, a chat log).
/// (Alias-with-intent of the existing scroll_fill(); pairs with flex_col parent.)
inline Mod vscroll(){ return sty([](Style& s){
    s.has_grow = true; s.grow = 1;
    s.extra.emplace_back("min-height", "0");
    s.extra.emplace_back("overflow-y", "auto");
    s.extra.emplace_back("-webkit-overflow-scrolling", "touch"); }); }

/// `hscroll()` — grow to fill and scroll HORIZONTALLY inside (a board, a shelf).
inline Mod hscroll(){ return sty([](Style& s){
    s.has_grow = true; s.grow = 1;
    s.extra.emplace_back("min-width", "0");
    s.extra.emplace_back("overflow-x", "auto");
    s.extra.emplace_back("-webkit-overflow-scrolling", "touch"); }); }

/// `viewport(children…)` — a root that fills EXACTLY the viewport height and is a
/// height-distributing flex column: header/toolbar stay put, a `grows`/`vscroll`
/// region fills the rest, nothing scrolls the page. The one-call app frame.
template <typename... Cs> NodeRef viewport(Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto n = box(); n->kids = std::move(k); n->style.flow = Flow::col;
    n->style.extra.emplace_back("height", "100dvh");
    n->style.extra.emplace_back("display", "flex");
    n->style.extra.emplace_back("flex-direction", "column");
    n->style.extra.emplace_back("overflow", "hidden");
    finalize(*n); return n;
}

// ── Centering ─────────────────────────────────────────────────────────────
/// `center_y` — centre children on the VERTICAL axis only (justify a col / align
/// a row). `dead_center` — perfectly centre on BOTH axes (a spinner, a splash).
inline const Mod center_y = sty([](Style& s){
    if(s.flow==Flow::none) s.flow=Flow::col;
    if(s.flow==Flow::col) s.justify=Justify::center; else s.align=Align::center; });
inline const Mod dead_center = sty([](Style& s){
    s.extra.emplace_back("display","grid"); s.extra.emplace_back("place-items","center"); });

// ── Split panes ──────────────────────────────────────────────────────────
/// `split(a, b)` — two panes side by side, each taking half (or `ratio` of the
/// width to `a`). Stacks to a column below `stack_below` total width. Great for
/// an editor+preview, a list+detail, a form+summary.
inline NodeRef split(NodeRef a, NodeRef b, float ratio = 0.5f, Len stack_below = rem(48)){
    int pa = (int)(ratio*100.f + 0.5f); if(pa<5) pa=5; if(pa>95) pa=95;
    std::string th = std::to_string((int)stack_below.value) + (stack_below.unit==Unit::rem?"rem":"px");
    // Each pane grows/shrinks around its share, but its flex-basis flips to the
    // Every-Layout switcher value so both wrap to a column below the threshold.
    // (grow, shrink, then the switcher basis — one `flex` shorthand, no conflict.)
    a->style.extra.emplace_back("min-width", "0");
    b->style.extra.emplace_back("min-width", "0");
    a->style.extra.emplace_back("flex", std::to_string(pa) + " 1 calc((" + th + " - 100%) * 999)");
    b->style.extra.emplace_back("flex", std::to_string(100-pa) + " 1 calc((" + th + " - 100%) * 999)");
    finalize(*a); finalize(*b);
    auto n = box(std::move(a), std::move(b));
    n->style.flow = Flow::row; n->style.wrap = Wrap::wrap; n->style.gap = px(16);
    finalize(*n); return n;
}

// ── Aspect-ratio media boxes ──────────────────────────────────────────────
/// `ratio_box(w, h, child)` — a container locked to a `w:h` aspect ratio that
/// clips its child to fit (a video embed, a cover image, a map). The child fills
/// it. `video_box(child)` is the 16:9 shorthand.
inline NodeRef ratio_box(float w, float h, NodeRef child){
    auto n = box(std::move(child));
    n->style.extra.emplace_back("aspect-ratio", detail::numstr(w) + " / " + detail::numstr(h));
    n->style.extra.emplace_back("width", "100%");
    n->style.extra.emplace_back("overflow", "hidden");
    n->style.extra.emplace_back("position", "relative");
    finalize(*n); return n;
}
inline NodeRef video_box(NodeRef child){ return ratio_box(16, 9, std::move(child)); }
inline NodeRef square_box(NodeRef child){ return ratio_box(1, 1, std::move(child)); }

// ── Masonry ───────────────────────────────────────────────────────────────
/// `masonry_(min_col, items, gap_px)` — a Pinterest-style column-packed layout:
/// items flow into as many columns as fit at >= `min_col` wide, each column
/// packed independently (no row alignment) so variable-height cards tile with no
/// gaps. Uses CSS multicol — widely supported, no JS. Order is column-major.
inline NodeRef masonry_(Len min_col, std::vector<NodeRef> items, float gap_px = 16){
    std::string mc = std::to_string((int)min_col.value) + (min_col.unit==Unit::rem?"rem":"px");
    for(auto& it : items){
        it->style.extra.emplace_back("break-inside", "avoid");
        it->style.extra.emplace_back("margin-bottom", detail::numstr(gap_px) + "px");
        it->style.extra.emplace_back("width", "100%");
        finalize(*it);
    }
    auto n = box(); n->kids = std::move(items);
    n->style.extra.emplace_back("column-width", mc);
    n->style.extra.emplace_back("column-gap", detail::numstr(gap_px) + "px");
    finalize(*n); return n;
}
template <typename... Cs> NodeRef masonry(Len min_col, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    return masonry_(min_col, std::move(k));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Every Layout primitives — responsive by construction, no breakpoints.
// ═══════════════════════════════════════════════════════════════════════════

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

/// BOARD / HSCROLL — a horizontally-scrolling row of fixed-width items (a kanban,
/// a carousel, a shelf of cards) that STAYS RESPONSIVE: it scrolls INSIDE itself
/// and never widens the page, its items shrink on small screens (clamp), and it
/// snaps item-to-item. The correct way to do "columns side by side" without the
/// classic "4 fixed columns overflow the phone" bug.
///
///   board(rem(18), colA, colB, colC, colD)   // desktop: all four; phone: swipe
///
/// `item_w` is the ideal item width; each item becomes
/// `flex: 0 0 clamp(min(item_w,86vw), item_w, item_w)` so it never exceeds the
/// viewport. Give items your own content; this handles the track.
/// `board_(item_w, items)` — the vector form (build items in a loop).
inline NodeRef board_(Len item_w, std::vector<NodeRef> k){
    std::string w = std::to_string((int)item_w.value) + (item_w.unit==Unit::rem?"rem":"px");
    for (auto& c : k){
        c->style.extra.emplace_back("flex", "0 0 clamp(min(" + w + ", 86vw), " + w + ", " + w + ")");
        c->style.extra.emplace_back("scroll-snap-align", "start");
        finalize(*c);
    }
    auto n = box(); n->kids = std::move(k);
    n->style.flow = Flow::row; n->style.gap = px(16);
    n->style.extra.emplace_back("overflow-x", "auto");
    n->style.extra.emplace_back("scroll-snap-type", "x proximity");
    n->style.extra.emplace_back("-webkit-overflow-scrolling", "touch");
    n->style.extra.emplace_back("align-items", "flex-start");
    n->style.extra.emplace_back("width", "100%");
    finalize(*n); return n;
}
template <typename... Cs> NodeRef board(Len item_w, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    return board_(item_w, std::move(k));
}

// ── fixed grids ─ the primitive that makes TABLES + card grids ALIGN ────────
// Faking columns with a per-row `row(grow(1)…)` never aligns — each row splits
// its own free space. A real grid gives every cell a SHARED column track, so
// headers and values line up down the page. Cells are the grid's direct kids.
//
// The grid MODS (grid_cols/grid_rows/grid_areas/auto_grid/col_span/row_span/
// area) live in node.hpp as first-class Flow::grid vocabulary. `columns()` below
// is the convenience BUILDER for the most common case: a fixed n-column table.

/// `columns(n, cells…)` — an n-column grid that is RESPONSIVE BY DEFAULT: it
/// shows up to `n` equal columns, and drops to fewer (…down to 1) all by itself
/// as the container narrows — no media query, no breakpoint. The trick is a
/// per-column floor: each track wants at least `min` wide, so once `n` of them
/// won't fit, `auto-fit` wraps. Pass `min` to tune the collapse point
/// (default 12rem ≈ a comfortable card).
///
///   columns(4, a, b, c, d)            // 4-up on desktop, 2-up on a tablet, 1 on phone
///   columns(3, a, b, c, min: rem(16)) // collapses sooner
template <typename... Cs> NodeRef columns(int n, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto box_ = box(); box_->kids = std::move(k);
    box_->style.flow = Flow::grid;
    // auto-fit + a min() floor => up to n columns, wrapping down on its own.
    // 100%/(n) - a hair keeps exactly n at full width, then peels off columns
    // as space shrinks. max(<min>, …) stops it going below a readable width.
    const std::string per = "calc(100% / " + std::to_string(n) + " - 0.01px)";
    box_->style.extra.emplace_back("grid-template-columns",
        "repeat(auto-fit, minmax(max(12rem, " + per + "), 1fr))");
    finalize(*box_); return box_;
}

/// `columns_min(n, min, cells…)` — like `columns` but with an explicit collapse
/// floor (each column stays >= `min` before the grid wraps to fewer).
template <typename... Cs> NodeRef columns_min(int n, Len min, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    auto box_ = box(); box_->kids = std::move(k);
    box_->style.flow = Flow::grid;
    std::string mn = std::to_string((int)min.value) + (min.unit==Unit::rem?"rem":"px");
    const std::string per = "calc(100% / " + std::to_string(n) + " - 0.01px)";
    box_->style.extra.emplace_back("grid-template-columns",
        "repeat(auto-fit, minmax(max(" + mn + ", " + per + "), 1fr))");
    finalize(*box_); return box_;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Whole-page grid layouts — one call = one dashboard shape. (For the app
//  ROOT wrapper see page()/app_shell()/centered() in sugar.hpp.)
// ═══════════════════════════════════════════════════════════════════════════

/// `dashboard(min_col, panels…)` — the panel grid: as many equal columns as fit
/// at >= `min_col` wide, every row STRETCHED so panels sharing a row are the
/// same height (charts/cards line up). The monitoring/instrument shape in one
/// call — like `grid(min_col, …)` but height-aligned.
///
///   dashboard(rem(26), cpuPanel, memPanel, netPanel, diskPanel)
template <typename... Cs> NodeRef dashboard(Len min_col, Cs... cs){
    std::vector<NodeRef> k; detail::collect(k, std::move(cs)...);
    std::string mc = std::to_string((int)min_col.value) + (min_col.unit==Unit::rem?"rem":"px");
    auto n = box(); n->kids = std::move(k);
    n->style.flow = Flow::grid;
    n->style.extra.emplace_back("grid-template-columns","repeat(auto-fit,minmax(min(" + mc + ",100%),1fr))");
    n->style.extra.emplace_back("align-items","stretch");
    n->style.gap = px(18);
    finalize(*n); return n;
}

/// `holy_grail(header, nav, main, aside, footer)` — the classic five-region
/// page band: full-width header + footer, and a middle row of nav | main |
/// aside that WRAPS to a single column on narrow screens by itself (no
/// breakpoint). `main` takes the leftover width; the rails are fixed-ish.
/// Pass `nullptr` for any region you don't need. Wrap the result in
/// page()/app_shell() for the coloured root.
inline NodeRef holy_grail(NodeRef header, NodeRef nav, NodeRef main_,
                          NodeRef aside, NodeRef footer,
                          Len rail = rem(14)){
    auto rail_s = std::to_string((int)rail.value) + (rail.unit==Unit::rem?"rem":"px");
    auto railed = [&](NodeRef r){
        if(!r) return;
        r->style.extra.emplace_back("flex","0 1 " + rail_s);
        r->style.min_w = px(0); finalize(*r);
    };
    railed(nav); railed(aside);
    if(main_){ main_->style.has_grow=true; main_->style.grow=999;
               main_->style.extra.emplace_back("flex-basis","60%");
               main_->style.min_w = px(0); finalize(*main_); }
    auto band = box();
    if(nav)   band->kids.push_back(nav);
    if(main_) band->kids.push_back(main_);
    if(aside) band->kids.push_back(aside);
    band->style.flow = Flow::row; band->style.wrap = Wrap::wrap;
    band->style.gap = px(16); band->style.has_grow=true; band->style.grow=1;
    finalize(*band);
    auto n = col();
    if(header) n->kids.push_back(header);
    n->kids.push_back(band);
    if(footer) n->kids.push_back(footer);
    n->style.gap = px(16); n->style.min_h = Len{100, Unit::vh};
    finalize(*n); return n;
}

} // namespace waya::surface
