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
#include "../core/str.hpp"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

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

// ── Effects ──────────────────────────────────────────────────────────
struct Shadow  {          constexpr Sty apply(Sty s) const { s.has_shadow=true; return s; } };
struct Opacity { int pct; constexpr Sty apply(Sty s) const { s.has_opacity=true; s.opacity_pct=pct; return s; } };
struct CursorT { Cursor c;constexpr Sty apply(Sty s) const { s.cursor=c; return s; } };
inline constexpr Shadow shadow{};
constexpr Opacity opacity(int pct) { return {pct}; }
inline constexpr CursorT pointer{Cursor::pointer};

// ═══════════════════════════════════════════════════════════════════════════
//  The universal channel — "general enough like maya": ANY CSS is one pipe.
// ═══════════════════════════════════════════════════════════════════════════
//
// The named tokens above are SUGAR. Everything CSS can express — including
// properties waya has never heard of, vendor prefixes, calc(), custom
// properties, grid templates — is reachable with the same clean pipe:
//
//   div_(...) | prop<"backdrop-filter", "blur(8px)">
//             | prop<"grid-template-columns", "repeat(3, 1fr)">
//             | prop<"transition", "all .2s ease">
//             | var_<"--brand", "#3b82f6">
//
// This is maya's escape hatch (raw Canvas next to the safe DSL), but it stays
// first-class: prop values are still interned and diffed like any other style.

template <Str Name, Str Value>
struct Prop {
    Sty apply(Sty s) const {
        s.extra.emplace_back(std::string(Name.view()), std::string(Value.view()));
        return s;
    }
};
/// Any CSS property/value, typed at compile time:  | prop<"filter", "blur(2px)">
template <Str Name, Str Value> inline constexpr Prop<Name, Value> prop{};

/// A CSS custom property (variable):  | var_<"--gap", "12px">
template <Str Name, Str Value> inline constexpr Prop<Name, Value> var_{};

/// Runtime property value, when the value isn't known at compile time:
///   | prop_dyn("width", std::to_string(w) + "px")
struct PropDyn {
    std::string name, value;
    Sty apply(Sty s) const { s.extra.emplace_back(name, value); return s; }
};
inline PropDyn prop_dyn(std::string name, std::string value) {
    return {std::move(name), std::move(value)};
}

// ── States & responsive — pseudo-classes and media queries as values ────────
//
//   button(...) | on<Hover>(bg(0x2563eb))          // :hover { background:... }
//               | on<Focus>(prop<"outline", "2px solid">)
//   card(...)   | at<Md>(width(50_pct))            // @media (min-width:768px)
//
// A state/breakpoint wraps ANOTHER style delta, so anything expressible in the
// base style is expressible in a state too — no separate, weaker API.

enum class Pseudo : std::uint8_t { Hover, Focus, Active, Disabled, FocusVisible,
                                   FirstChild, LastChild, Checked };
enum class Break  : std::uint8_t { Sm, Md, Lg, Xl, X2l };

namespace detail {
consteval std::string_view pseudo_sel(Pseudo p) {
    switch (p) {
        case Pseudo::Hover:        return ":hover";
        case Pseudo::Focus:        return ":focus";
        case Pseudo::Active:       return ":active";
        case Pseudo::Disabled:     return ":disabled";
        case Pseudo::FocusVisible: return ":focus-visible";
        case Pseudo::FirstChild:   return ":first-child";
        case Pseudo::LastChild:    return ":last-child";
        case Pseudo::Checked:      return ":checked";
    }
    return "";
}
consteval std::string_view break_query(Break b) {
    switch (b) {
        case Break::Sm:  return "@media (min-width:640px)";
        case Break::Md:  return "@media (min-width:768px)";
        case Break::Lg:  return "@media (min-width:1024px)";
        case Break::Xl:  return "@media (min-width:1280px)";
        case Break::X2l: return "@media (min-width:1536px)";
    }
    return "";
}
// Serialise a nested style delta to a CSS body (declarations only). Declared
// here, defined in css.hpp where the serialiser lives.
std::string declarations_of(const Sty& s);
} // namespace detail

/// `on<Hover>(...)` — apply a style delta in a pseudo-class state.
template <Pseudo P, StyleToken... Toks>
struct OnState {
    std::tuple<Toks...> toks;
    Sty apply(Sty s) const {
        Sty delta{};
        std::apply([&](const auto&... t){ ((delta = t.apply(delta)), ...); }, toks);
        s.states.emplace_back(std::string(detail::pseudo_sel(P)),
                              detail::declarations_of(delta));
        return s;
    }
};
template <Pseudo P, StyleToken... Toks>
OnState<P, Toks...> on(Toks... toks) { return {std::make_tuple(std::move(toks)...)}; }

/// `at<Md>(...)` — apply a style delta at a responsive breakpoint.
template <Break B, StyleToken... Toks>
struct AtBreak {
    std::tuple<Toks...> toks;
    Sty apply(Sty s) const {
        Sty delta{};
        std::apply([&](const auto&... t){ ((delta = t.apply(delta)), ...); }, toks);
        s.states.emplace_back(std::string(detail::break_query(B)) + "__MEDIA__",
                              detail::declarations_of(delta));
        return s;
    }
};
template <Break B, StyleToken... Toks>
AtBreak<B, Toks...> at(Toks... toks) { return {std::make_tuple(std::move(toks)...)}; }

// Convenience aliases so `on<Hover>` reads naturally.
inline constexpr Pseudo Hover = Pseudo::Hover;
inline constexpr Pseudo Focus = Pseudo::Focus;
inline constexpr Pseudo Active = Pseudo::Active;
inline constexpr Break  Sm = Break::Sm, Md = Break::Md, Lg = Break::Lg, Xl = Break::Xl;

} // namespace waya::style
