#pragma once
/// \file sugar.hpp
/// Delightful conveniences over the surface vocabulary. Optional \u2014 everything
/// here is just shorthand for things you can already write. Include it for a
/// nicer authoring experience: named colours, a spacing scale, and the `when` /
/// `each` combinators for conditional and list content.

#include "node.hpp"

#include <functional>
#include <vector>

namespace waya::surface {

// ── A pleasant default palette (slate/indigo/cyan family) ───────────────────
// Named so you write `fg(ink)` not `fg(0xe2e8f0)`. Override freely with any hex.
namespace color {
inline constexpr std::uint32_t ink      = 0xe2e8f0;  // primary text
inline constexpr std::uint32_t muted    = 0x94a3b8;  // secondary text
inline constexpr std::uint32_t faint    = 0x64748b;  // tertiary
inline constexpr std::uint32_t bg0       = 0x0b1020;  // page
inline constexpr std::uint32_t bg1       = 0x141b2e;  // surface
inline constexpr std::uint32_t bg2       = 0x1e293b;  // raised
inline constexpr std::uint32_t line      = 0x334155;  // border
inline constexpr std::uint32_t brand     = 0x6366f1;  // indigo
inline constexpr std::uint32_t brand2    = 0x22d3ee;  // cyan
inline constexpr std::uint32_t good      = 0x34d399;  // green
inline constexpr std::uint32_t warn      = 0xf59e0b;
inline constexpr std::uint32_t bad       = 0xf87171;
inline constexpr std::uint32_t white     = 0xffffff;
} // namespace color

// ── Spacing scale (4px grid) so gaps/pads stay consistent ───────────────────
// `pad(sp(4))` == 16px. Small numbers, harmonious spacing.
inline float sp(int step){ return step * 4.f; }

// ── Combinators ─────────────────────────────────────────────────────────

/// `when(cond, node)` — the node, or an empty (zero-size) box when false. So
/// `col(header, when(loading, spinner()), body)` just works.
inline NodeRef when(bool cond, NodeRef node){
    return cond ? node : box();   // empty box renders as an empty <div>
}
/// `when(cond, a, b)` — a or b.
inline NodeRef when(bool cond, NodeRef a, NodeRef b){ return cond ? a : b; }
/// `when(cond, […]{ return node; })` — lazy: builds the node only if shown.
template <typename Fn> requires std::is_invocable_r_v<NodeRef, Fn>
NodeRef when(bool cond, Fn build){ return cond ? build() : box(); }

/// `show(cond, node)` — alias for when; reads well for visibility toggles.
inline NodeRef show(bool cond, NodeRef node){ return when(cond, std::move(node)); }

/// `each(range, fn)` — map a range to a list of nodes, spliced into a parent.
/// `col(each(items, [](auto& x){ return row(text(x.name)); }))`.
template <typename Range, typename Fn>
std::vector<NodeRef> each(const Range& range, Fn fn){
    std::vector<NodeRef> out;
    for (const auto& item : range) out.push_back(fn(item));
    return out;
}
/// `each` with an index: `each(items, [](auto& x, size_t i){ … })`.
template <typename Range, typename Fn>
    requires requires(Fn f, const typename Range::value_type& v, std::size_t i){ f(v, i); }
std::vector<NodeRef> each_i(const Range& range, Fn fn){
    std::vector<NodeRef> out; std::size_t i = 0;
    for (const auto& item : range) out.push_back(fn(item, i++));
    return out;
}
/// `each_keyed(range, key_fn, view_fn)` — map a range to KEYED nodes, so the diff
/// reconciles by identity (moves, not re-renders). key_fn returns a string.
template <typename Range, typename KeyFn, typename Fn>
std::vector<NodeRef> each_keyed(const Range& range, KeyFn key_fn, Fn view_fn){
    std::vector<NodeRef> out;
    for (const auto& item : range){
        auto node = view_fn(item);
        node->key = key_fn(item);
        finalize(*node);
        out.push_back(std::move(node));
    }
    return out;
}

// A box/row/col that takes a vector<NodeRef> (so `each` composes directly).
inline NodeRef box_(std::vector<NodeRef> kids){ auto n=std::make_shared<Node>(); n->kind=Kind::box; n->kids=std::move(kids); finalize(*n); return n; }
inline NodeRef row_(std::vector<NodeRef> kids){ auto n=box_(std::move(kids)); n->style.flow=Flow::row; finalize(*n); return n; }
inline NodeRef col_(std::vector<NodeRef> kids){ auto n=box_(std::move(kids)); n->style.flow=Flow::col; finalize(*n); return n; }

/// `fragment(nodes)` — splice a vector of nodes into a parent without a wrapper
/// box. A `display:contents` div: it lays out as if its children were direct
/// children of the grandparent. Useful for `col( header, fragment(each(…)) )`.
inline NodeRef fragment(std::vector<NodeRef> kids){
    auto n = box_(std::move(kids));
    n->style.extra.emplace_back("display", "contents");
    finalize(*n);
    return n;
}

// ── Overlays: portals for modals / menus / toasts ─────────────────────────
// The surface has no separate render root, so a portal is expressed as a fixed,
// full-viewport layer stacked above everything (high z-index). It stays in the
// tree (so the diff still owns it) but escapes the normal layout flow — exactly
// what a modal/menu needs. Content is centered by default; pass mods to place it.

/// `overlay(content)` — a fixed full-screen layer above the page (z 1000),
/// content centered. Add `tap(Close)` for a click-away backdrop.
inline NodeRef overlay(NodeRef content){
    auto n = box(std::move(content));
    auto& s = n->style;
    s.pos = Pos::fixed;
    s.top = {0,Unit::px}; s.left = {0,Unit::px}; s.right = {0,Unit::px}; s.bottom = {0,Unit::px};
    s.has_z = true; s.z = 1000;
    s.flow = Flow::col; s.justify = Justify::center; s.align = Align::center;
    s.extra.emplace_back("background", "rgba(0,0,0,.55)");
    s.extra.emplace_back("backdrop-filter", "blur(2px)");
    finalize(*n);
    return n;
}
/// `modal(cond, content)` — the overlay only when `cond`; empty otherwise.
inline NodeRef modal(bool open, NodeRef content){
    return open ? overlay(std::move(content)) : box();
}
/// `toast_layer(nodes)` — a fixed, non-interactive top-right stack for toasts.
inline NodeRef toast_layer(std::vector<NodeRef> toasts){
    auto n = col_(std::move(toasts));
    auto& s = n->style;
    s.pos = Pos::fixed;
    s.top = {sp(4),Unit::px}; s.right = {sp(4),Unit::px};
    s.has_z = true; s.z = 1100; s.gap = {sp(2),Unit::px};
    s.extra.emplace_back("pointer-events", "none");
    finalize(*n);
    return n;
}

// ── Responsive app shells ────────────────────────────────────────
// The framework already makes every node fit its container (no overflow); these
// give you a correct full-viewport page in one call, so "responsive" is the
// path of least resistance rather than something you assemble by hand.

/// `page(bg_color, content…)` — the app root: fills the whole viewport, paints
/// its background edge-to-edge (no gutters), and pads fluidly (clamp). Drop your
/// UI inside; it adapts to any screen with zero media queries.
template <typename... Cs> NodeRef page(std::uint32_t bg_color, Cs... cs){
    auto n = col(std::move(cs)...);
    n->style.has_grow = true; n->style.grow = 1;
    n->style.has_bg = true; n->style.bg = bg_color;
    n->style.extra.emplace_back("min-height", "100vh");
    n->style.extra.emplace_back("padding", "clamp(0px, 3vw, 2.5rem)");
    finalize(*n);
    return n;
}

/// `centered(max_rem, content)` — a column capped to `max_rem` wide and centred,
/// growing to fill available height. The classic "readable centred content"
/// container (chat, article, form). Fluidly full-width below the cap.
inline NodeRef centered(float max_rem, NodeRef content){
    auto n = col(std::move(content));
    n->style.has_grow = true; n->style.grow = 1;
    n->style.extra.emplace_back("width", "100%");
    n->style.extra.emplace_back("max-width", std::to_string((int)max_rem) + "rem");
    n->style.extra.emplace_back("margin-inline", "auto");
    finalize(*n);
    return n;
}

} // namespace waya::surface
