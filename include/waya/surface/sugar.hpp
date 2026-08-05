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

// ── Combinators ─────────────────────────────────────────────────────────────

/// `when(cond, node)` \u2014 the node, or an empty (zero-size) box when false. So
/// `col(header, when(loading, spinner()), body)` just works.
inline NodeRef when(bool cond, NodeRef node){
    return cond ? node : box();   // empty box renders as an empty <div>
}
/// `when(cond, a, b)` \u2014 a or b.
inline NodeRef when(bool cond, NodeRef a, NodeRef b){ return cond ? a : b; }

/// `each(range, fn)` \u2014 map a range to a list of nodes, spliced into a parent.
/// `col(each(items, [](auto& x){ return row(text(x.name)); }))`.
template <typename Range, typename Fn>
std::vector<NodeRef> each(const Range& range, Fn fn){
    std::vector<NodeRef> out;
    for (const auto& item : range) out.push_back(fn(item));
    return out;
}

// A box/row/col that takes a vector<NodeRef> (so `each` composes directly).
inline NodeRef box_(std::vector<NodeRef> kids){ auto n=std::make_shared<Node>(); n->kind=Kind::box; n->kids=std::move(kids); finalize(*n); return n; }
inline NodeRef row_(std::vector<NodeRef> kids){ auto n=box_(std::move(kids)); n->style.flow=Flow::row; finalize(*n); return n; }
inline NodeRef col_(std::vector<NodeRef> kids){ auto n=box_(std::move(kids)); n->style.flow=Flow::col; finalize(*n); return n; }

} // namespace waya::surface
