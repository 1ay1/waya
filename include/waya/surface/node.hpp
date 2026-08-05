#pragma once
/// \file node.hpp
/// The Surface — waya's substrate-free rendering model.
///
/// You describe WHAT to render with a handful of primitives and a clean,
/// complete style vocabulary; waya owns HOW (a DOM backend, a canvas backend,
/// later others). Nothing here mentions HTML, CSS, or the canvas API in your
/// code — yet the API can express anything CSS and layout can. See SURFACE.md.
///
///   col(
///       text("Dashboard") | fg(0x3b82f6) | font(28) | bold,
///       box( text("Requests"), text("42") )
///           | pad(12) | bg(0x1e293b) | round(12) | shadow()
///           | on(Hover, bg(0x334155)),                    // states are values
///       path(cpu_history) | stroke(0x22d3ee, 2),          // a chart — one node
///       text("+") | pad(8) | bg(0x334155) | round(8) | tap(Inc),
///   ) | gap(16) | pad(24) | center
///
/// Simple by default (named attrs cover the common 90%); never limiting (the
/// universal `css("prop","value")` reaches anything, and `path` draws any
/// shape). Every attribute is a uniform `Style` mutator, so they all compose
/// the same clean way.

#include "../core/hash.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

namespace waya::surface {

// ── Enums for the common layout/typography choices ──────────────────────────
enum class Flow    : std::uint8_t { none, row, col, stack };
enum class Justify : std::uint8_t { none, start, center, end, between, around, evenly };
enum class Align   : std::uint8_t { none, start, center, end, stretch, baseline };
enum class Wrap    : std::uint8_t { none, wrap, nowrap };
enum class Pos     : std::uint8_t { none, relative, absolute, fixed, sticky };
enum class Weight  : std::uint8_t { none, thin, light, normal, medium, semibold, bold, black };
enum class Cursor  : std::uint8_t { none, pointer, text, move, not_allowed };

/// A length with a unit — so `w(50, Pct)` or `w(200)` (px default) both read well.
enum class Unit : std::uint8_t { px, pct, rem, em, vw, vh, fr, fill /*100%*/, hug /*auto*/ };
struct Len {
    float value = 0; Unit unit = Unit::px;
    bool operator==(const Len&) const = default;
    [[nodiscard]] bool set() const { return value != 0 || unit != Unit::px; }
};
inline Len px(float v){ return {v, Unit::px}; }
inline Len pct(float v){ return {v, Unit::pct}; }
inline Len rem(float v){ return {v, Unit::rem}; }
inline Len vw(float v){ return {v, Unit::vw}; }
inline Len vh(float v){ return {v, Unit::vh}; }
inline Len fr(float v){ return {v, Unit::fr}; }
inline constexpr Len fill{100, Unit::fill};
inline constexpr Len hug {0,   Unit::hug};

/// The complete style value carried on a node. Named fields for the common
/// case; `extra` (any CSS prop/value) and `states` (hover / media / …) for the
/// long tail. Nothing is out of reach.
struct Style {
    // colour & text
    bool has_fg=false;      std::uint32_t fg=0;
    bool has_bg=false;      std::uint32_t bg=0;
    Len  font_size{};       Weight weight=Weight::none;
    bool italic=false, underline=false, strike=false;
    Justify text_align=Justify::none;
    bool has_lh=false;      float line_height=0;
    bool has_ls=false;      float letter_spacing=0;

    // box model
    Len  pad{}, pad_x{}, pad_y{};        // padding: all / horizontal / vertical
    Len  margin{}, margin_x{}, margin_y{};
    Len  w{}, h{}, min_w{}, max_w{}, min_h{}, max_h{};
    Len  radius{};
    bool has_border=false;  Len border_w{}; std::uint32_t border_c=0;

    // flex / layout
    Flow    flow=Flow::none;
    Justify justify=Justify::none;
    Align   align=Align::none;
    Wrap    wrap=Wrap::none;
    Len     gap{};
    bool    has_grow=false;  float grow=0;
    bool    has_shrink=false; float shrink=1;

    // position
    Pos  pos=Pos::none;
    Len  top{}, right{}, bottom{}, left{};
    bool has_z=false; int z=0;

    // effects
    bool has_shadow=false;   std::string shadow_spec;   // "" = default nice shadow
    bool has_opacity=false;  float opacity=1;
    Cursor cursor=Cursor::none;
    bool has_transition=false; std::string transition_spec;

    // stroke (for path) & fill
    bool has_stroke_w=false; float stroke_w=2;

    // the universal channel — ANY css, so nothing is ever off-limits
    std::vector<std::pair<std::string,std::string>> extra;   // (prop, value)
    // stateful/responsive overlays: (selector-or-media, css-body-of-a-Style)
    std::vector<std::pair<std::string, std::shared_ptr<Style>>> states;

    bool operator==(const Style& o) const {
        // compare everything except `states` deeply enough for diffing; states
        // are compared by their serialised effect via the node hash.
        auto tie = [](const Style& s){ return std::tie(
            s.has_fg,s.fg,s.has_bg,s.bg,s.font_size,s.weight,s.italic,s.underline,s.strike,
            s.text_align,s.has_lh,s.line_height,s.has_ls,s.letter_spacing,
            s.pad,s.pad_x,s.pad_y,s.margin,s.margin_x,s.margin_y,
            s.w,s.h,s.min_w,s.max_w,s.min_h,s.max_h,s.radius,
            s.has_border,s.border_w,s.border_c,
            s.flow,s.justify,s.align,s.wrap,s.gap,s.has_grow,s.grow,s.has_shrink,s.shrink,
            s.pos,s.top,s.right,s.bottom,s.left,s.has_z,s.z,
            s.has_shadow,s.shadow_spec,s.has_opacity,s.opacity,s.cursor,
            s.has_transition,s.transition_spec,s.has_stroke_w,s.stroke_w,s.extra); };
        if (tie(*this) != tie(o)) return false;
        if (states.size() != o.states.size()) return false;
        for (std::size_t i=0;i<states.size();++i)
            if (states[i].first != o.states[i].first || !(*states[i].second == *o.states[i].second)) return false;
        return true;
    }
};

// ── Primitives ───────────────────────────────────────────────────────────────
enum class Kind : std::uint8_t { box, text, image, path };

struct Node; using NodeRef = std::shared_ptr<Node>;
struct Pt { float x, y; bool operator==(const Pt&) const = default; };

struct Node {
    Kind  kind = Kind::box;
    Style style{};
    std::string     text, src;
    std::vector<Pt> points; bool closed=false;
    std::string     key;
    int             on_tap=-1;
    std::vector<NodeRef> kids;
    std::uint64_t   hash=0;
};

// ── Hashing (bottom-up; captures style incl. extra/states) ──────────────────
template <typename I> requires std::is_integral_v<I> || std::is_enum_v<I>
inline std::uint64_t mix(std::uint64_t h, I v){ auto u=(std::uint64_t)v;
    for(int i=0;i<8;++i){h^=(u>>(i*8))&0xFF;h*=1099511628211ull;} return h; }
inline std::uint64_t mix(std::uint64_t h, std::string_view s){
    for(char c:s){h^=(std::uint8_t)c;h*=1099511628211ull;} return h; }
inline std::uint64_t mix(std::uint64_t h, float f){ std::uint32_t b; std::memcpy(&b,&f,4); return mix(h,(std::uint64_t)b); }
inline std::uint64_t mix(std::uint64_t h, const Len& l){ return mix(mix(h,l.value),l.unit); }

inline std::uint64_t hash_style(std::uint64_t h, const Style& s){
    h=mix(h,s.has_fg);h=mix(h,(std::uint64_t)s.fg);h=mix(h,s.has_bg);h=mix(h,(std::uint64_t)s.bg);
    h=mix(h,s.font_size);h=mix(h,s.weight);h=mix(h,s.italic);h=mix(h,s.underline);h=mix(h,s.strike);
    h=mix(h,s.text_align);h=mix(h,s.line_height);h=mix(h,s.letter_spacing);
    h=mix(h,s.pad);h=mix(h,s.pad_x);h=mix(h,s.pad_y);h=mix(h,s.margin);h=mix(h,s.margin_x);h=mix(h,s.margin_y);
    h=mix(h,s.w);h=mix(h,s.h);h=mix(h,s.min_w);h=mix(h,s.max_w);h=mix(h,s.min_h);h=mix(h,s.max_h);h=mix(h,s.radius);
    h=mix(h,s.has_border);h=mix(h,s.border_w);h=mix(h,(std::uint64_t)s.border_c);
    h=mix(h,s.flow);h=mix(h,s.justify);h=mix(h,s.align);h=mix(h,s.wrap);h=mix(h,s.gap);h=mix(h,s.grow);h=mix(h,s.shrink);
    h=mix(h,s.pos);h=mix(h,s.top);h=mix(h,s.right);h=mix(h,s.bottom);h=mix(h,s.left);h=mix(h,(std::uint64_t)s.z);
    h=mix(h,s.has_shadow);h=mix(h,s.shadow_spec);h=mix(h,s.opacity);h=mix(h,s.cursor);h=mix(h,s.transition_spec);
    h=mix(h,s.stroke_w);
    for(auto&[k,v]:s.extra){h=mix(h,k);h=mix(h,v);}
    for(auto&[sel,st]:s.states){h=mix(h,sel);h=hash_style(h,*st);}
    return h;
}
inline void finalize(Node& n){
    std::uint64_t h=1469598103934665603ull;
    h=mix(h,n.kind); h=hash_style(h,n.style);
    h=mix(h,n.text);h=mix(h,n.src);
    for(auto&p:n.points){h=mix(h,p.x);h=mix(h,p.y);} h=mix(h,n.closed);
    h=mix(h,n.key); h=mix(h,(std::int64_t)n.on_tap);
    for(auto&k:n.kids) h=mix(h,k->hash);
    n.hash=h;
}

// ── Builders (variadic, so `box(a, b, c)` reads clean \u2014 no braces) ──────────
namespace detail {
inline void push(std::vector<NodeRef>&){}
template<typename... R> void push(std::vector<NodeRef>& v, NodeRef n, R... r){ v.push_back(std::move(n)); push(v,std::move(r)...); }
}
template <typename... Cs> NodeRef box(Cs... cs){ auto n=std::make_shared<Node>(); n->kind=Kind::box; detail::push(n->kids, std::move(cs)...); finalize(*n); return n; }
template <typename... Cs> NodeRef row(Cs... cs){ auto n=box(std::move(cs)...); n->style.flow=Flow::row; finalize(*n); return n; }
template <typename... Cs> NodeRef col(Cs... cs){ auto n=box(std::move(cs)...); n->style.flow=Flow::col; finalize(*n); return n; }
template <typename... Cs> NodeRef stack(Cs... cs){ auto n=box(std::move(cs)...); n->style.flow=Flow::stack; finalize(*n); return n; }

inline NodeRef text(std::string s){ auto n=std::make_shared<Node>(); n->kind=Kind::text; n->text=std::move(s); finalize(*n); return n; }
inline NodeRef text(long long v){ return text(std::to_string(v)); }
inline NodeRef text(int v){ return text(std::to_string(v)); }
inline NodeRef image(std::string src){ auto n=std::make_shared<Node>(); n->kind=Kind::image; n->src=std::move(src); finalize(*n); return n; }
inline NodeRef path(std::vector<Pt> pts, bool closed=false){ auto n=std::make_shared<Node>(); n->kind=Kind::path; n->points=std::move(pts); n->closed=closed; finalize(*n); return n; }

// ── Attributes — every one is a uniform Style mutator ───────────────────────
/// An Attr is a function that mutates a Style. `node | attr` applies it and
/// re-hashes. This uniformity is what makes the whole API compose cleanly.
struct Attr { std::function<void(Style&)> apply; };
inline NodeRef operator|(NodeRef n, const Attr& a){ a.apply(n->style); finalize(*n); return n; }


// colour & text
inline Attr fg(std::uint32_t c){ return {[=](Style& s){ s.has_fg=true; s.fg=c; }}; }
inline Attr bg(std::uint32_t c){ return {[=](Style& s){ s.has_bg=true; s.bg=c; }}; }
inline Attr font(Len sz){ return {[=](Style& s){ s.font_size=sz; }}; }
inline Attr font(float px_){ return font(px(px_)); }
inline Attr weight(Weight w){ return {[=](Style& s){ s.weight=w; }}; }
inline const Attr bold      { [](Style& s){ s.weight=Weight::bold; } };
inline const Attr semibold  { [](Style& s){ s.weight=Weight::semibold; } };
inline const Attr italic    { [](Style& s){ s.italic=true; } };
inline const Attr underline { [](Style& s){ s.underline=true; } };
inline Attr text_align(Justify j){ return {[=](Style& s){ s.text_align=j; }}; }
inline Attr leading(float lh){ return {[=](Style& s){ s.has_lh=true; s.line_height=lh; }}; }
inline Attr tracking(float ls){ return {[=](Style& s){ s.has_ls=true; s.letter_spacing=ls; }}; }

// box model
inline Attr pad(Len l){ return {[=](Style& s){ s.pad=l; }}; }
inline Attr pad(float p){ return pad(px(p)); }
inline Attr pad_x(Len l){ return {[=](Style& s){ s.pad_x=l; }}; }
inline Attr pad_x(float p){ return pad_x(px(p)); }
inline Attr pad_y(Len l){ return {[=](Style& s){ s.pad_y=l; }}; }
inline Attr pad_y(float p){ return pad_y(px(p)); }
inline Attr margin(Len l){ return {[=](Style& s){ s.margin=l; }}; }
inline Attr margin(float m){ return margin(px(m)); }
inline Attr w(Len l){ return {[=](Style& s){ s.w=l; }}; }
inline Attr w(float v){ return w(px(v)); }
inline Attr h(Len l){ return {[=](Style& s){ s.h=l; }}; }
inline Attr h(float v){ return h(px(v)); }
inline Attr max_w(Len l){ return {[=](Style& s){ s.max_w=l; }}; }
inline Attr min_w(Len l){ return {[=](Style& s){ s.min_w=l; }}; }
inline Attr round(Len l){ return {[=](Style& s){ s.radius=l; }}; }
inline Attr round(float r){ return round(px(r)); }
inline const Attr pill { [](Style& s){ s.radius=px(9999); } };
inline Attr border(float width_, std::uint32_t color){ return {[=](Style& s){ s.has_border=true; s.border_w=px(width_); s.border_c=color; }}; }

// layout
inline const Attr wrap { [](Style& s){ s.wrap=Wrap::wrap; } };
inline Attr justify(Justify j){ return {[=](Style& s){ s.justify=j; }}; }
inline Attr align(Align a){ return {[=](Style& s){ s.align=a; }}; }
inline Attr gap(Len l){ return {[=](Style& s){ s.gap=l; }}; }
inline Attr gap(float g){ return gap(px(g)); }
inline Attr grow(float g=1){ return {[=](Style& s){ s.has_grow=true; s.grow=g; }}; }
inline Attr shrink(float g=1){ return {[=](Style& s){ s.has_shrink=true; s.shrink=g; }}; }
/// `center` — centre children both axes. The single most common layout, one word.
inline const Attr center { [](Style& s){ if(s.flow==Flow::none) s.flow=Flow::row; s.justify=Justify::center; s.align=Align::center; } };

// position
inline Attr absolute(Len top_={}, Len left_={}){ return {[=](Style& s){ s.pos=Pos::absolute; s.top=top_; s.left=left_; }}; }
inline Attr fixed(){ return {[](Style& s){ s.pos=Pos::fixed; }}; }
inline Attr sticky(){ return {[](Style& s){ s.pos=Pos::sticky; }}; }
inline Attr z(int zi){ return {[=](Style& s){ s.has_z=true; s.z=zi; }}; }

// effects
inline Attr shadow(std::string spec=""){ return {[=](Style& s){ s.has_shadow=true; s.shadow_spec=spec; }}; }
inline Attr opacity(float o){ return {[=](Style& s){ s.has_opacity=true; s.opacity=o; }}; }
inline const Attr pointer { [](Style& s){ s.cursor=Cursor::pointer; } };
inline Attr transition(std::string spec="all .15s ease"){ return {[=](Style& s){ s.has_transition=true; s.transition_spec=spec; }}; }
inline Attr stroke(std::uint32_t color, float width_=2){ return {[=](Style& s){ s.has_fg=true; s.fg=color; s.has_stroke_w=true; s.stroke_w=width_; }}; }

// ── The universal channel — reach ANY css, so nothing is off-limits ─────────
inline Attr css(std::string prop, std::string value){ return {[=](Style& s){ s.extra.emplace_back(prop, value); }}; }
inline Attr var(std::string name, std::string value){ return {[=](Style& s){ s.extra.emplace_back("--"+name, value); }}; }

// ── States & responsive — hover/focus/etc. and breakpoints as values ────────
enum class State : std::uint8_t { Hover, Focus, Active, Disabled };
enum class Break : std::uint8_t { Sm, Md, Lg, Xl };
inline constexpr State Hover=State::Hover, Focus=State::Focus, Active=State::Active;
inline constexpr Break Sm=Break::Sm, Md=Break::Md, Lg=Break::Lg, Xl=Break::Xl;

namespace detail {
inline std::string state_sel(State st){ switch(st){ case State::Hover:return ":hover"; case State::Focus:return ":focus";
    case State::Active:return ":active"; case State::Disabled:return ":disabled"; } return ""; }
inline std::string break_q(Break b){ switch(b){ case Break::Sm:return "@media(min-width:640px)"; case Break::Md:return "@media(min-width:768px)";
    case Break::Lg:return "@media(min-width:1024px)"; case Break::Xl:return "@media(min-width:1280px)"; } return ""; }
template <typename... A> std::shared_ptr<Style> build_style(A... attrs){ auto s=std::make_shared<Style>(); (attrs.apply(*s), ...); return s; }
}
/// `on(Hover, bg(...), fg(...))` — a style overlay for a state.
template <typename... A> Attr on(State st, A... attrs){
    auto sub = detail::build_style(attrs...);
    return {[=](Style& s){ s.states.emplace_back(detail::state_sel(st), sub); }};
}
/// `at(Md, w(fill), pad(24))` — a style overlay at a breakpoint.
template <typename... A> Attr at(Break b, A... attrs){
    auto sub = detail::build_style(attrs...);
    return {[=](Style& s){ s.states.emplace_back(detail::break_q(b), sub); }};
}

// interactivity & identity — these need the NODE, not just the Style, so they
// have their own pipe overloads.
struct TapTag { int msg; };
/// `tap(Msg)` — this node responds with a message when clicked/tapped.
inline TapTag tap(int msg){ return {msg}; }
inline NodeRef operator|(NodeRef n, TapTag t){ n->on_tap=t.msg; finalize(*n); return n; }

struct KeyTag { std::string k; };
/// `key("id")` — stable identity for keyed diffing of lists.
inline KeyTag key(std::string k){ return {std::move(k)}; }
inline NodeRef operator|(NodeRef n, KeyTag t){ n->key=std::move(t.k); finalize(*n); return n; }

} // namespace waya::surface
