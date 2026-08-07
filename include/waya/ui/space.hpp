#pragma once
/// \file ui/space.hpp
/// Spacing-scale SHORTHANDS — so an app reads on a consistent 4px rhythm without
/// spelling out `pad(sp(4))` every time. The scale itself is `sp(step)` (in the
/// core, `sp(4)` == 16px); this adds the terse mods that apply it:
///
///   box() | p(4)     // padding: sp(4) = 16px on all sides
///   row() | gx(3)    // gap: sp(3) = 12px between children
///   col() | py(6)    // vertical padding: sp(6) = 24px
///   text() | mt(2)   // margin-top: sp(2) = 8px
///   box() | px_(5)   // inline padding: sp(5) = 20px (px_ so it can't clash
///
/// The scale (4px steps): 1=4 2=8 3=12 4=16 5=20 6=24 8=32 10=40 12=48 16=64.
/// These are thin wrappers over the core `pad/gap/margin` mods — reach for the
/// raw pixel versions (`pad(14)`) any time you need an off-scale value; the
/// scale is the convenient DEFAULT, never a cage.

#include "../surface/node.hpp"
#include "../surface/sugar.hpp"   // sp(int) -> float, the 4px scale

namespace waya::ui {

using namespace waya::surface;

// ── scale-based spacing mods (thin wrappers on pad/gap/margin) ──────────────
inline Mod p(int step)   { return pad(sp(step)); }            ///< padding, all sides
inline Mod px_(int step) { return pad_x(sp(step)); }          ///< padding, inline (l+r)
inline Mod py(int step)  { return pad_y(sp(step)); }          ///< padding, block (t+b)
inline Mod gx(int step)  { return gap(sp(step)); }            ///< gap between flex children
inline Mod ma(int step)  { return margin(sp(step)); }         ///< margin, all sides (ma, not m, to avoid shadowing a Model named m)
inline Mod mt(int step)  { return detail::raw_css("margin-top",    detail::numstr(sp(step)) + "px"); }
inline Mod mb(int step)  { return detail::raw_css("margin-bottom", detail::numstr(sp(step)) + "px"); }

/// `stack_v(step, children…)` — a column with a scale gap: `col(cs...) | gx(step)`.
template <typename... Cs> NodeRef stack_v(int step, Cs... cs){ return col(std::move(cs)...) | gx(step); }
/// `stack_h(step, children…)` — a row with a scale gap.
template <typename... Cs> NodeRef stack_h(int step, Cs... cs){ return row(std::move(cs)...) | gx(step); }

} // namespace waya::ui
