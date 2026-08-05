#pragma once
/// \file tokens.hpp
/// The user-facing style vocabulary: small tag values piped onto nodes with `|`.
///
/// Each token exposes `apply(Sty) -> Sty`. The `|` overload (in dsl) applies the
/// token to a node's style. Container-only tokens (gap/justify/align) carry a
/// compile-time gate so they only apply once the node is a flex/grid container —
/// maya's "border colour needs a border", transposed onto the box model.

#include "sty.hpp"
#include "../core/diagnostic.hpp"

namespace waya::style {

/// Anything usable in `node | token`.
template <typename T>
concept StyleToken = requires (T t, Sty s) { { t.apply(s) } -> std::same_as<Sty>; };

// ── Colour ──────────────────────────────────────────────────────────────────
struct Fg { std::uint32_t c; constexpr Sty apply(Sty s) const { s.has_fg=true; s.fg=c; return s; } };
struct Bg { std::uint32_t c; constexpr Sty apply(Sty s) const { s.has_bg=true; s.bg=c; return s; } };
constexpr Fg fg(std::uint32_t hex) { return {hex}; }
constexpr Bg bg(std::uint32_t hex) { return {hex}; }

// ── Box model ───────────────────────────────────────────────────────────────
struct Pad   { Len l; constexpr Sty apply(Sty s) const { s.has_pad=true;   s.pad=l;    return s; } };
struct PadX  { Len l; constexpr Sty apply(Sty s) const { s.has_pad_x=true; s.pad_x=l;  return s; } };
struct PadY  { Len l; constexpr Sty apply(Sty s) const { s.has_pad_y=true; s.pad_y=l;  return s; } };
struct Margin{ Len l; constexpr Sty apply(Sty s) const { s.has_margin=true;s.margin=l; return s; } };
struct Width { Len l; constexpr Sty apply(Sty s) const { s.has_w=true;     s.w=l;      return s; } };
struct Height{ Len l; constexpr Sty apply(Sty s) const { s.has_h=true;     s.h=l;      return s; } };
struct MaxW  { Len l; constexpr Sty apply(Sty s) const { s.has_max_w=true; s.max_w=l;  return s; } };
struct MinW  { Len l; constexpr Sty apply(Sty s) const { s.has_min_w=true; s.min_w=l;  return s; } };
struct Radius{ Len l; constexpr Sty apply(Sty s) const { s.has_radius=true;s.radius=l; return s; } };

constexpr Pad    pad(Len l)     { return {l}; }
constexpr PadX   pad_x(Len l)   { return {l}; }
constexpr PadY   pad_y(Len l)   { return {l}; }
constexpr Margin margin(Len l)  { return {l}; }
constexpr Width  width(Len l)   { return {l}; }
constexpr Height height(Len l)  { return {l}; }
constexpr MaxW   max_w(Len l)   { return {l}; }
constexpr MinW   min_w(Len l)   { return {l}; }
constexpr Radius rounded(Len l) { return {l}; }

// ── Display / flex container ────────────────────────────────────────────────
// These are what UNLOCK the container-only tokens below (like maya's border_<>
// unlocking bcol).
struct Flex { Dir d; constexpr Sty apply(Sty s) const { s.display=Display::flex; s.direction=d; return s; } };
struct Grid {        constexpr Sty apply(Sty s) const { s.display=Display::grid; return s; } };
struct Block{        constexpr Sty apply(Sty s) const { s.display=Display::block; return s; } };
struct Hidden{       constexpr Sty apply(Sty s) const { s.display=Display::none; return s; } };

constexpr Flex flex(Dir d = Dir::row) { return {d}; }
inline constexpr Flex  row{Dir::row};   ///< `| row`  — a horizontal flex container
inline constexpr Flex  col{Dir::col};   ///< `| col`  — a vertical flex container
inline constexpr Grid  grid{};
inline constexpr Block block{};
inline constexpr Hidden hidden{};

// ── Container-only tokens — GATED at compile time ───────────────────────────
//
// gap/justify/align genuinely do nothing in CSS outside a flex/grid context.
// waya makes misuse a compile error. The check reads a `bool` inside a helper
// type (not the concept, not in a function body) so the diagnostic is one line.

namespace detail {
consteval auto container_msg(std::string_view prop) {
    diag::Msg<256> m;
    m += "waya: `"; m += prop;
    m += "` requires a flex or grid container. Add `| row`, `| col`, `| flex(...)` "
         "or `| grid` first. (This property has no effect outside a flex/grid "
         "context in CSS.)";
    return m;
}
} // namespace detail

struct Gap     { Len l;     constexpr Sty apply(Sty s) const { s.has_gap=true; s.gap=l; return s; } };
struct JustifyT{ Justify j; constexpr Sty apply(Sty s) const { s.justify=j; return s; } };
struct AlignT  { Align a;   constexpr Sty apply(Sty s) const { s.align=a; return s; } };
struct WrapT   { Wrap w;    constexpr Sty apply(Sty s) const { s.wrap=w; return s; } };

constexpr Gap      gap(Len l)          { return {l}; }
constexpr JustifyT justify(Justify j)  { return {j}; }
constexpr AlignT   items(Align a)      { return {a}; }
constexpr WrapT    wrap(Wrap w = Wrap::wrap) { return {w}; }

// Marker so the `|` overload knows these tokens must be gated. (Applied in dsl.)
template <typename T> inline constexpr bool is_container_only = false;
template <> inline constexpr bool is_container_only<Gap> = true;
template <> inline constexpr bool is_container_only<JustifyT> = true;
template <> inline constexpr bool is_container_only<AlignT> = true;
template <> inline constexpr bool is_container_only<WrapT> = true;

// Marker for tokens that MAKE a node a flex/grid container (they unlock the
// gated tokens above). The `|` overload records this in the node's ElemCfg.
template <typename T> inline constexpr bool makes_container = false;
template <> inline constexpr bool makes_container<Flex> = true;
template <> inline constexpr bool makes_container<Grid> = true;

// ── Flex item ───────────────────────────────────────────────────────────────
struct Grow   { int n; constexpr Sty apply(Sty s) const { s.has_grow=true; s.grow=n; return s; } };
struct Shrink { int n; constexpr Sty apply(Sty s) const { s.has_shrink=true; s.shrink=n; return s; } };
constexpr Grow   grow(int n = 1)   { return {n}; }
constexpr Shrink shrink(int n = 1) { return {n}; }

// ── Typography ──────────────────────────────────────────────────────────────
struct Wt     { Weight w;    constexpr Sty apply(Sty s) const { s.weight=w; return s; } };
struct Size   { Len l;       constexpr Sty apply(Sty s) const { s.has_size=true; s.size=l; return s; } };
struct Leading{ Len l;       constexpr Sty apply(Sty s) const { s.has_lh=true; s.line_height=l; return s; } };
struct ItalicT{              constexpr Sty apply(Sty s) const { s.italic=true; return s; } };
struct UnderT {              constexpr Sty apply(Sty s) const { s.underline=true; return s; } };
struct TAlign { TextAlign a; constexpr Sty apply(Sty s) const { s.text_align=a; return s; } };

constexpr Wt      weight(Weight w) { return {w}; }
inline constexpr Wt bold{Weight::bold};
inline constexpr Wt semibold{Weight::w600};
inline constexpr Wt medium{Weight::w500};
constexpr Size    size(Len l)      { return {l}; }
constexpr Leading leading(Len l)   { return {l}; }
inline constexpr ItalicT italic{};
inline constexpr UnderT  underline{};
constexpr TAlign  text(TextAlign a){ return {a}; }

// ── Effects ─────────────────────────────────────────────────────────────────
struct Shadow  {          constexpr Sty apply(Sty s) const { s.has_shadow=true; return s; } };
struct Opacity { int pct; constexpr Sty apply(Sty s) const { s.has_opacity=true; s.opacity_pct=pct; return s; } };
struct CursorT { Cursor c;constexpr Sty apply(Sty s) const { s.cursor=c; return s; } };
inline constexpr Shadow shadow{};
constexpr Opacity opacity(int pct) { return {pct}; }
inline constexpr CursorT pointer{Cursor::pointer};

} // namespace waya::style
