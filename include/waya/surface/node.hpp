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
enum class Kind : std::uint8_t { box, text, image, path, input };

struct Node; using NodeRef = std::shared_ptr<Node>;
struct Pt { float x, y; bool operator==(const Pt&) const = default; };

struct Node {
    Kind  kind = Kind::box;
    Style style{};
    std::string     text, src;         // text: content; image: url; input: value
    std::string     placeholder, input_type;  // input
    std::vector<Pt> points; bool closed=false;
    std::string     key;
    int             on_tap=-1;         // click → message
    int             on_input=-1;       // input event → message (value sent up)
    int             on_change=-1;      // change event → message
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
    h=mix(h,n.text);h=mix(h,n.src);h=mix(h,n.placeholder);h=mix(h,n.input_type);
    for(auto&p:n.points){h=mix(h,p.x);h=mix(h,p.y);} h=mix(h,n.closed);
    h=mix(h,n.key); h=mix(h,(std::int64_t)n.on_tap); h=mix(h,(std::int64_t)n.on_input); h=mix(h,(std::int64_t)n.on_change);
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
/// `input(value)` — a real text field. Style/placeholder/on_input via modifiers.
inline NodeRef input(std::string value={}){ auto n=std::make_shared<Node>(); n->kind=Kind::input; n->text=std::move(value); n->input_type="text"; finalize(*n); return n; }

// ═══════════════════════════════════════════════════════════════════════════
//  ONE uniform modifier. maya's principle: everything is a node, and everything
//  you do to a node is the SAME kind of thing — a `Mod`, a function Node→Node.
//  Style attrs, tap, key, on(state), at(breakpoint): all Mods, all `node | mod`.
//  Compose freely; they read like one clean sentence.
// ═══════════════════════════════════════════════════════════════════════════

struct Mod { std::function<void(Node&)> apply; };
inline NodeRef operator|(NodeRef n, const Mod& m){ if(m.apply) m.apply(*n); finalize(*n); return n; }
/// Mods compose: `a | b` is a Mod that applies a then b (so you can name bundles).
inline Mod operator|(Mod a, Mod b){ return {[=](Node& n){ a.apply(n); b.apply(n); }}; }

// A style-only mod — the common case. `sfn` takes a Style& mutator.
inline Mod sty(std::function<void(Style&)> f){ return {[f=std::move(f)](Node& n){ f(n.style); }}; }
/// A do-nothing Mod — the identity for `|`. Makes conditional styling clean:
///   text(x) | (active ? bold : noop)
inline const Mod noop = {[](Node&){}};

// ── colour & text ──────────────────────────────────────────────────────────────
inline Mod fg(std::uint32_t c){ return sty([=](Style& s){ s.has_fg=true; s.fg=c; }); }
inline Mod bg(std::uint32_t c){ return sty([=](Style& s){ s.has_bg=true; s.bg=c; }); }
inline Mod font(Len sz){ return sty([=](Style& s){ s.font_size=sz; }); }
inline Mod font(float px_){ return font(px(px_)); }
inline Mod weight(Weight w){ return sty([=](Style& s){ s.weight=w; }); }
inline const Mod bold      = sty([](Style& s){ s.weight=Weight::bold; });
inline const Mod semibold  = sty([](Style& s){ s.weight=Weight::semibold; });
inline const Mod medium    = sty([](Style& s){ s.weight=Weight::medium; });
inline const Mod italic    = sty([](Style& s){ s.italic=true; });
inline const Mod underline = sty([](Style& s){ s.underline=true; });
inline const Mod strike    = sty([](Style& s){ s.strike=true; });
inline Mod text_align(Justify j){ return sty([=](Style& s){ s.text_align=j; }); }
inline Mod leading(float lh){ return sty([=](Style& s){ s.has_lh=true; s.line_height=lh; }); }
inline Mod tracking(float ls){ return sty([=](Style& s){ s.has_ls=true; s.letter_spacing=ls; }); }
inline const Mod nowrap_text = sty([](Style& s){ s.extra.emplace_back("white-space","nowrap"); });
inline const Mod truncate = sty([](Style& s){ s.extra.emplace_back("white-space","nowrap"); s.extra.emplace_back("overflow","hidden"); s.extra.emplace_back("text-overflow","ellipsis"); });

// ── box model ────────────────────────────────────────────────────────────────
inline Mod pad(Len l){ return sty([=](Style& s){ s.pad=l; }); }
inline Mod pad(float p){ return pad(px(p)); }
inline Mod pad_x(Len l){ return sty([=](Style& s){ s.pad_x=l; }); }
inline Mod pad_x(float p){ return pad_x(px(p)); }
inline Mod pad_y(Len l){ return sty([=](Style& s){ s.pad_y=l; }); }
inline Mod pad_y(float p){ return pad_y(px(p)); }
inline Mod margin(Len l){ return sty([=](Style& s){ s.margin=l; }); }
inline Mod margin(float m){ return margin(px(m)); }
inline Mod w(Len l){ return sty([=](Style& s){ s.w=l; }); }
inline Mod w(float v){ return w(px(v)); }
inline Mod h(Len l){ return sty([=](Style& s){ s.h=l; }); }
inline Mod h(float v){ return h(px(v)); }
inline Mod size(Len side){ return sty([=](Style& s){ s.w=side; s.h=side; }); }   // square
inline Mod size(float side){ return size(px(side)); }
inline Mod max_w(Len l){ return sty([=](Style& s){ s.max_w=l; }); }
inline Mod min_w(Len l){ return sty([=](Style& s){ s.min_w=l; }); }
inline Mod max_h(Len l){ return sty([=](Style& s){ s.max_h=l; }); }
inline Mod min_h(Len l){ return sty([=](Style& s){ s.min_h=l; }); }
inline Mod round(Len l){ return sty([=](Style& s){ s.radius=l; }); }
inline Mod round(float r){ return round(px(r)); }
inline const Mod pill = sty([](Style& s){ s.radius=px(9999); });
inline Mod border(float width_, std::uint32_t color){ return sty([=](Style& s){ s.has_border=true; s.border_w=px(width_); s.border_c=color; }); }
inline Mod aspect(float ratio){ return sty([=](Style& s){ s.extra.emplace_back("aspect-ratio", std::to_string(ratio)); }); }

// ── layout ────────────────────────────────────────────────────────────────
inline const Mod wrap = sty([](Style& s){ s.wrap=Wrap::wrap; });
inline const Mod nowrap = sty([](Style& s){ s.wrap=Wrap::nowrap; });
inline Mod justify(Justify j){ return sty([=](Style& s){ s.justify=j; }); }
inline Mod align(Align a){ return sty([=](Style& s){ s.align=a; }); }
inline Mod gap(Len l){ return sty([=](Style& s){ s.gap=l; }); }
inline Mod gap(float g){ return gap(px(g)); }
inline Mod grow(float g=1){ return sty([=](Style& s){ s.has_grow=true; s.grow=g; }); }
inline Mod shrink(float g=1){ return sty([=](Style& s){ s.has_shrink=true; s.shrink=g; }); }
/// `center` — centre children both axes; the single most common layout, one word.
inline const Mod center = sty([](Style& s){ if(s.flow==Flow::none) s.flow=Flow::row; s.justify=Justify::center; s.align=Align::center; });
/// `between` — push children to opposite ends.
inline const Mod between = sty([](Style& s){ s.justify=Justify::between; });
inline Mod overflow(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("overflow", v); }); }
inline const Mod scroll = sty([](Style& s){ s.extra.emplace_back("overflow","auto"); });
inline const Mod clip   = sty([](Style& s){ s.extra.emplace_back("overflow","hidden"); });

// ── position ──────────────────────────────────────────────────────────────
inline Mod absolute(Len top_={}, Len left_={}){ return sty([=](Style& s){ s.pos=Pos::absolute; s.top=top_; s.left=left_; }); }
inline const Mod fixed  = sty([](Style& s){ s.pos=Pos::fixed; });
inline const Mod sticky = sty([](Style& s){ s.pos=Pos::sticky; });
inline const Mod relative = sty([](Style& s){ s.pos=Pos::relative; });
inline Mod inset(Len t, Len r, Len b, Len l){ return sty([=](Style& s){ s.top=t; s.right=r; s.bottom=b; s.left=l; }); }
inline Mod z(int zi){ return sty([=](Style& s){ s.has_z=true; s.z=zi; }); }

// ── effects ──────────────────────────────────────────────────────────────
inline Mod shadow(std::string spec=""){ return sty([=](Style& s){ s.has_shadow=true; s.shadow_spec=spec; }); }
inline Mod opacity(float o){ return sty([=](Style& s){ s.has_opacity=true; s.opacity=o; }); }
inline const Mod pointer = sty([](Style& s){ s.cursor=Cursor::pointer; });
inline Mod cursor(Cursor c){ return sty([=](Style& s){ s.cursor=c; }); }
inline Mod transition(std::string spec="all .15s ease"){ return sty([=](Style& s){ s.has_transition=true; s.transition_spec=spec; }); }
inline Mod blur(float px_){ return sty([=](Style& s){ s.extra.emplace_back("filter","blur("+std::to_string((int)px_)+"px)"); }); }
inline Mod backdrop_blur(float px_){ return sty([=](Style& s){ s.extra.emplace_back("backdrop-filter","blur("+std::to_string((int)px_)+"px)"); }); }
inline Mod scale(float f){ return sty([=](Style& s){ s.extra.emplace_back("transform","scale("+std::to_string(f)+")"); }); }
inline Mod rotate(float deg){ return sty([=](Style& s){ s.extra.emplace_back("transform","rotate("+std::to_string((int)deg)+"deg)"); }); }
inline Mod stroke(std::uint32_t color, float width_=2){ return sty([=](Style& s){ s.has_fg=true; s.fg=color; s.has_stroke_w=true; s.stroke_w=width_; }); }

// ── image fit ───────────────────────────────────────────────────────────
inline const Mod cover   = sty([](Style& s){ s.extra.emplace_back("object-fit","cover"); });
inline const Mod contain = sty([](Style& s){ s.extra.emplace_back("object-fit","contain"); });

// ── gradients (delightful sugar over the universal channel) ──────────────
namespace detail { inline std::string hexstr(std::uint32_t c){ static const char* H="0123456789abcdef"; std::string o="#"; for(int s=20;s>=0;s-=4)o+=H[(c>>s)&0xF]; return o; } }
inline Mod gradient(std::uint32_t a, std::uint32_t b, int deg=90){
    return sty([=](Style& s){ s.extra.emplace_back("background",
        "linear-gradient("+std::to_string(deg)+"deg,"+detail::hexstr(a)+","+detail::hexstr(b)+")"); }); }
/// gradient text: paint the gradient and clip it to the glyphs.
inline Mod gradient_text(std::uint32_t a, std::uint32_t b, int deg=90){
    return sty([=](Style& s){
        s.extra.emplace_back("background","linear-gradient("+std::to_string(deg)+"deg,"+detail::hexstr(a)+","+detail::hexstr(b)+")");
        s.extra.emplace_back("-webkit-background-clip","text");
        s.extra.emplace_back("background-clip","text");
        s.extra.emplace_back("color","transparent"); }); }

// ── the universal channel — reach ANY css, nothing off-limits ────────────
inline Mod css(std::string prop, std::string value){ return sty([=](Style& s){ s.extra.emplace_back(prop, value); }); }
inline Mod var(std::string name, std::string value){ return sty([=](Style& s){ s.extra.emplace_back("--"+name, value); }); }

// ── states & responsive — hover/focus/etc. and breakpoints, also Mods ─────
enum class State : std::uint8_t { Hover, Focus, Active, Disabled };
enum class Break : std::uint8_t { Sm, Md, Lg, Xl };
inline constexpr State Hover=State::Hover, Focus=State::Focus, Active=State::Active, Disabled=State::Disabled;
inline constexpr Break Sm=Break::Sm, Md=Break::Md, Lg=Break::Lg, Xl=Break::Xl;

namespace detail {
inline std::string state_sel(State st){ switch(st){ case State::Hover:return ":hover"; case State::Focus:return ":focus";
    case State::Active:return ":active"; case State::Disabled:return ":disabled"; } return ""; }
inline std::string break_q(Break b){ switch(b){ case Break::Sm:return "@media(min-width:640px)"; case Break::Md:return "@media(min-width:768px)";
    case Break::Lg:return "@media(min-width:1024px)"; case Break::Xl:return "@media(min-width:1280px)"; } return ""; }
/// Build a sub-Style by applying style-only Mods to a fresh node's style.
template <typename... M> std::shared_ptr<Style> sub_style(M... mods){
    Node tmp; (mods.apply(tmp), ...); return std::make_shared<Style>(tmp.style); }
}
/// `on(Hover, bg(...), scale(1.02f))` — a style overlay for a state. A Mod.
template <typename... M> Mod on(State st, M... mods){
    auto sub = detail::sub_style(mods...);
    return sty([=](Style& s){ s.states.emplace_back(detail::state_sel(st), sub); });
}
/// `at(Md, w(fill), pad(24))` — a style overlay at a breakpoint. A Mod.
template <typename... M> Mod at(Break b, M... mods){
    auto sub = detail::sub_style(mods...);
    return sty([=](Style& s){ s.states.emplace_back(detail::break_q(b), sub); });
}

// ── interactivity & identity — Mods that touch the node, not just its style ─
inline Mod tap(int msg){ return {[=](Node& n){ n.on_tap=msg; }}; }
inline Mod on_input(int msg){ return {[=](Node& n){ n.on_input=msg; }}; }
inline Mod on_change(int msg){ return {[=](Node& n){ n.on_change=msg; }}; }
inline Mod placeholder(std::string p){ return {[=](Node& n){ n.placeholder=p; }}; }
inline Mod type(std::string t){ return {[=](Node& n){ n.input_type=t; }}; }
inline Mod key(std::string k){ return {[=](Node& n){ n.key=k; }}; }

} // namespace waya::surface
