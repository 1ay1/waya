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
#include "assets.hpp"
#include "../color.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "msg.hpp"
#include <string_view>
#include <type_traits>
#include <tuple>
#include <utility>
#include <vector>

namespace waya::surface {

// The colour vocabulary belongs to the surface language: one `using namespace
// waya::surface` brings `rgb`/`rgba`/`hsl`/`Color` along — no second import.
using waya::Color;
using waya::rgb;
using waya::rgba;
using waya::hsl;

// ── Enums for the common layout/typography choices ──────────────────────────
enum class Flow    : std::uint8_t { none, row, col, stack, grid };
enum class Justify : std::uint8_t { none, start, center, end, between, around, evenly };
enum class Align   : std::uint8_t { none, start, center, end, stretch, baseline };
enum class Wrap    : std::uint8_t { none, wrap, nowrap };
enum class Pos     : std::uint8_t { none, relative, absolute, fixed, sticky };
enum class Weight  : std::uint8_t { none, thin, light, normal, medium, semibold, bold, black };
enum class Cursor  : std::uint8_t { none, pointer, text, move, not_allowed };

/// A length with a unit — so `w(50, Pct)` or `w(200)` (px default) both read well.
/// `dvh`/`dvw` are the MOBILE-SAFE viewport units: `100_vh` overshoots on phones
/// (the URL bar), `100_dvh` is what "full screen" almost always means.
enum class Unit : std::uint8_t { px, pct, rem, em, vw, vh, fr, fill /*100%*/, hug /*auto*/, dvh, dvw };
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
inline Len dvh(float v){ return {v, Unit::dvh}; }
inline Len dvw(float v){ return {v, Unit::dvw}; }

/// Length literals — `12_px`, `1.5_rem`, `50_pct`, `100_vh`. Reads like maya's
/// typed units and keeps a size self-documenting at the call site.
namespace literals {
inline Len operator""_px (long double v){ return {(float)v, Unit::px}; }
inline Len operator""_px (unsigned long long v){ return {(float)v, Unit::px}; }
inline Len operator""_rem(long double v){ return {(float)v, Unit::rem}; }
inline Len operator""_rem(unsigned long long v){ return {(float)v, Unit::rem}; }
inline Len operator""_em (long double v){ return {(float)v, Unit::em}; }
inline Len operator""_pct(long double v){ return {(float)v, Unit::pct}; }
inline Len operator""_pct(unsigned long long v){ return {(float)v, Unit::pct}; }
inline Len operator""_vw (long double v){ return {(float)v, Unit::vw}; }
inline Len operator""_vw (unsigned long long v){ return {(float)v, Unit::vw}; }
inline Len operator""_vh (long double v){ return {(float)v, Unit::vh}; }
inline Len operator""_vh (unsigned long long v){ return {(float)v, Unit::vh}; }
inline Len operator""_dvh(long double v){ return {(float)v, Unit::dvh}; }
inline Len operator""_dvh(unsigned long long v){ return {(float)v, Unit::dvh}; }
inline Len operator""_dvw(long double v){ return {(float)v, Unit::dvw}; }
inline Len operator""_dvw(unsigned long long v){ return {(float)v, Unit::dvw}; }
inline Len operator""_fr (unsigned long long v){ return {(float)v, Unit::fr}; }
} // namespace literals
inline Len fr(float v){ return {v, Unit::fr}; }
inline constexpr Len fill{100, Unit::fill};
inline constexpr Len hug {0,   Unit::hug};

/// The complete style value carried on a node. Named fields for the common
/// case; `extra` (any CSS prop/value) and `states` (hover / media / …) for the
/// long tail. Nothing is out of reach.
struct Style {
    // The POD prefix is hashed as raw BYTES (hash_style), so padding between
    // fields must be deterministic. Zero the whole object first, then the member
    // initializers set the real defaults — padding bytes stay 0 forever, so two
    // equal Styles always hash identically. (Copy/move keep this: they copy the
    // padding too, and copies of a zeroed-padding object have zeroed padding.)
    // Zero the POD prefix. memset through a void* so no compiler warns about a
    // non-trivially-copyable target — we only touch the trivially-copyable
    // [0, pod_bytes()) region, never the string/vector members past it.
    Style(){ void* raw = static_cast<void*>(this); std::memset(raw, 0, offsetof(Style, shadow_spec)); init_defaults(); }
    // Copy/move must reproduce the POD prefix EXACTLY — padding included — or a
    // clone would hash differently than its source (the byte-hash reads padding).
    // A defaulted copy ctor copies members field-by-field and leaves the
    // destination's padding indeterminate; memcpy the whole prefix instead.
    Style(const Style& o){ copy_pod(o); shadow_spec=o.shadow_spec; transition_spec=o.transition_spec; extra=o.extra; states=o.states; }
    Style(Style&& o) noexcept { copy_pod(o); shadow_spec=std::move(o.shadow_spec); transition_spec=std::move(o.transition_spec); extra=std::move(o.extra); states=std::move(o.states); }
    Style& operator=(const Style& o){ if(this!=&o){ copy_pod(o); shadow_spec=o.shadow_spec; transition_spec=o.transition_spec; extra=o.extra; states=o.states; } return *this; }
    Style& operator=(Style&& o) noexcept { if(this!=&o){ copy_pod(o); shadow_spec=std::move(o.shadow_spec); transition_spec=std::move(o.transition_spec); extra=std::move(o.extra); states=std::move(o.states); } return *this; }

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
    bool has_shadow=false;
    bool has_opacity=false;  float opacity=1;
    Cursor cursor=Cursor::none;
    bool has_transition=false;

    // stroke (for path) & fill
    bool has_stroke_w=false; float stroke_w=2;

    // ── END OF THE TRIVIALLY-COPYABLE POD PREFIX ──────────────────────────
    // Everything above is bytes we hash in ONE pass (hash_style). The members
    // below hold heap data and are hashed/compared individually. Keep this split
    // intact: pod_end() marks the boundary.
    std::string shadow_spec;      // "" = default nice shadow
    std::string transition_spec;

    // the universal channel — ANY css, so nothing is ever off-limits
    std::vector<std::pair<std::string,std::string>> extra;   // (prop, value)
    // stateful/responsive overlays: (selector-or-media, css-body-of-a-Style)
    std::vector<std::pair<std::string, std::shared_ptr<Style>>> states;

    /// The byte offset where the POD prefix ends (the first non-trivial member).
    /// The whole [0, pod_bytes()) range is trivially copyable and hashed as raw
    /// bytes in one FNV pass — 45 field-by-field mix() calls collapsed to one
    /// tight loop, the dominant per-node hashing cost.
    static constexpr std::size_t pod_bytes(){ return offsetof(Style, shadow_spec); }

    // Re-apply the non-zero member defaults after the ctor's memset. Only the
    // fields whose default isn't 0/false/none need listing here.
    void init_defaults(){
        opacity = 1; grow = 0; shrink = 1; stroke_w = 2;
        // (all enums default to their 0 == "none" value; all Len/bool default to
        // {0,px}/false, which is already zeroed — nothing else to restore.)
    }
    // Byte-copy the whole POD prefix INCLUDING padding, so a copy hashes exactly
    // like its source. void* to keep the compiler quiet about the wider type.
    void copy_pod(const Style& o){
        void* dst = static_cast<void*>(this);
        const void* src = static_cast<const void*>(&o);
        std::memcpy(dst, src, offsetof(Style, shadow_spec));
    }

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
// box/text/image/path are the visual atoms; the rest are real form controls
// (their value/checked flows up through on_input / on_change to update()).
enum class Kind : std::uint8_t {
    box, text, image, path,
    input, textarea,          // free text (single / multi-line)
    checkbox, radio,          // boolean / single-choice toggles
    select,                   // dropdown; children are `option`s
    button,                   // a real <button> (submit-free tap target)
    form,                     // groups controls; on_submit gathers named fields
    video, audio,             // media by url
    markup                    // raw trusted HTML (rich content escape hatch)
};

struct Node; using NodeRef = std::shared_ptr<Node>;
struct Pt { float x, y; bool operator==(const Pt&) const = default; };

/// One option in a `select`. `value` is what rides the wire; `label` is shown.
struct Opt { std::string value, label; bool operator==(const Opt&) const = default; };

/// A wired DOM event handler. `event` is the event name the client listens for
/// ("keydown", "focus", "blur", "submit", "drop", "pointerenter"…); `msg` is
/// the app Msg to deliver; `arg` narrows it (e.g. a key name for keydown, so
/// on_key("Enter",..) only fires on Enter). The payload the client sends up (a
/// field value, the dropped key, etc.) becomes the update `value`.
struct Handler { std::string event; int msg; std::string arg; bool operator==(const Handler&) const = default; };

struct Node {
    Kind  kind = Kind::box;
    Style style{};
    std::string     text, src;         // text: content; image: url; input/area/button: value/label
    std::string     placeholder, input_type;  // input
    std::string     name;              // radio/checkbox group name; form field name
    bool            checked=false;     // checkbox / radio state
    bool            disabled=false;    // control disabled
    bool            draggable=false;   // this node can be dragged
    std::vector<Opt> options;          // select: the choices
    std::string     selected;          // select: the chosen option value
    std::vector<Pt> points; bool closed=false;
    std::string     key;
    std::string     tag;               // override the HTML element (main/nav/h1/article…) for SEO/a11y
    int             on_tap=-1;         // click → message
    int             on_input=-1;       // input event → message (value sent up)
    int             on_change=-1;      // change event → message
    std::vector<Handler> events;       // generic wired events (keyboard/focus/drag/submit/pointer)
    std::vector<std::pair<std::string,std::string>> attrs;  // arbitrary HTML attrs (aria-*, role, data-*, title…)
    std::vector<NodeRef> kids;
    std::uint64_t   hash=0;
};

// ── Node pool ───────────────────────────────────────────────────────────────
// A Node is ~720 bytes, and view() builds a whole tree of them every frame only
// to diff it and drop the previous one. Profiling shows BUILDING the tree costs
// ~170x the DIFF — the make_shared<Node> allocation + destruction churn is the
// framework's real per-frame cost, not the (already O(1)-skip) diff.
//
// This pool recycles the fixed-size control-block+Node allocation. Because the
// owner loop builds frame N+1 while frame N is still alive, then drops frame N,
// the freed blocks return to a thread-local free-list and are immediately
// reused — so steady-state rendering makes ZERO calls to the global allocator
// for node storage. Nodes still reset all their fields on recycle (a fresh
// Node{} placement-new), so there's no state leakage; only the raw memory is
// reused. Fully transparent: node builders call new_node() instead of
// make_shared, the API and shared_ptr semantics are identical.
namespace detail {

struct NodePool {
    std::vector<void*> free_list;
    std::size_t chunk = 0;              // the size make_shared asks for (learned once)
    std::size_t high_water = 0;         // peak live blocks (diagnostics)
    std::size_t live = 0;

    // NOTE: no destructor frees the free-list. The pool is intentionally leaked
    // at program exit (see node_pool()) — recycled node storage that a still-live
    // shared_ptr might reference must NOT be freed by the pool, or its own
    // deleter double-frees it during static teardown. Leaking a per-thread
    // free-list at process exit is free (the OS reclaims it) and correct.

    void* take(std::size_t n){
        if (chunk == 0) chunk = n;      // first alloc fixes the block size
        if (n != chunk){ ++live; return ::operator new(n); }  // odd size: bypass pool
        if (!free_list.empty()){ void* p = free_list.back(); free_list.pop_back(); ++live; return p; }
        ++live; if (live > high_water) high_water = live;
        return ::operator new(n);
    }
    void give(void* p, std::size_t n){
        --live;
        if (n != chunk){ ::operator delete(p); return; }
        // cap the free-list so a transient huge frame doesn't pin memory forever.
        if (free_list.size() < 65536) free_list.push_back(p);
        else ::operator delete(p);
    }
};

// The pool is heap-allocated and never deleted, so it strictly outlives every
// NodeRef (which may be destroyed during static teardown after main). A raw
// leaked pointer is the simplest destruction-order-safe lifetime.
inline NodePool& node_pool(){ static thread_local NodePool* p = new NodePool(); return *p; }

// A stateless allocator that routes make_shared's single block through the pool.
template <typename T>
struct PoolAlloc {
    using value_type = T;
    PoolAlloc() = default;
    template <typename U> PoolAlloc(const PoolAlloc<U>&) noexcept {}
    T* allocate(std::size_t n){ return static_cast<T*>(node_pool().take(n * sizeof(T))); }
    void deallocate(T* p, std::size_t n) noexcept { node_pool().give(p, n * sizeof(T)); }
    template <typename U> bool operator==(const PoolAlloc<U>&) const noexcept { return true; }
    template <typename U> bool operator!=(const PoolAlloc<U>&) const noexcept { return false; }
};

/// Allocate a fresh Node through the recycling pool. Drop-in for make_shared<Node>().
inline NodeRef new_node(){ return std::allocate_shared<Node>(PoolAlloc<Node>{}); }

} // namespace detail

// ── Hashing (bottom-up; captures style incl. extra/states) ──────────────────
template <typename I> requires std::is_integral_v<I> || std::is_enum_v<I>
inline std::uint64_t mix(std::uint64_t h, I v){ auto u=(std::uint64_t)v;
    for(int i=0;i<8;++i){h^=(u>>(i*8))&0xFF;h*=1099511628211ull;} return h; }
inline std::uint64_t mix(std::uint64_t h, std::string_view s){
    for(char c:s){h^=(std::uint8_t)c;h*=1099511628211ull;} return h; }
inline std::uint64_t mix(std::uint64_t h, float f){ std::uint32_t b; std::memcpy(&b,&f,4); return mix(h,(std::uint64_t)b); }
inline std::uint64_t mix(std::uint64_t h, const Len& l){ return mix(mix(h,l.value),l.unit); }

inline std::uint64_t hash_style(std::uint64_t h, const Style& s){
    // Hash the trivially-copyable POD prefix (~256 bytes of Len/bool/enum/uint/
    // float) 8 bytes at a time — one multiply per 64-bit word, not per byte and
    // not per field. This is the fast path that dominated per-node hashing.
    const unsigned char* base = reinterpret_cast<const unsigned char*>(&s);
    constexpr std::size_t N = Style::pod_bytes();
    std::size_t i = 0;
    for (; i + 8 <= N; i += 8){
        std::uint64_t w; std::memcpy(&w, base + i, 8);
        h ^= w; h *= 1099511628211ull;
    }
    for (; i < N; ++i){ h ^= base[i]; h *= 1099511628211ull; }   // tail < 8 bytes
    // The heap-backed members are hashed by content.
    h=mix(h,s.shadow_spec); h=mix(h,s.transition_spec);
    for(auto&[k,v]:s.extra){h=mix(h,k);h=mix(h,v);}
    for(auto&[sel,st]:s.states){h=mix(h,sel);h=hash_style(h,*st);}
    return h;
}
inline void finalize(Node& n){
    std::uint64_t h=1469598103934665603ull;
    h=mix(h,n.kind); h=hash_style(h,n.style);
    h=mix(h,n.text);h=mix(h,n.src);h=mix(h,n.placeholder);h=mix(h,n.input_type);
    h=mix(h,n.name);h=mix(h,n.checked);h=mix(h,n.disabled);h=mix(h,n.selected);
    h=mix(h,n.draggable);
    for(auto&o:n.options){h=mix(h,o.value);h=mix(h,o.label);}
    for(auto&e:n.events){h=mix(h,e.event);h=mix(h,(std::int64_t)e.msg);h=mix(h,e.arg);}
    for(auto&a:n.attrs){h=mix(h,a.first);h=mix(h,a.second);}
    for(auto&p:n.points){h=mix(h,p.x);h=mix(h,p.y);} h=mix(h,n.closed);
    h=mix(h,n.key); h=mix(h,n.tag); h=mix(h,(std::int64_t)n.on_tap); h=mix(h,(std::int64_t)n.on_input); h=mix(h,(std::int64_t)n.on_change);
    for(auto&k:n.kids) h=mix(h,k->hash);
    n.hash=h;
}

// ── Builders (variadic, so `box(a, b, c)` reads clean \u2014 no braces) ──────────
namespace detail {
inline void push(std::vector<NodeRef>&){}
template<typename... R> void push(std::vector<NodeRef>& v, NodeRef n, R... r){ v.push_back(std::move(n)); push(v,std::move(r)...); }
/// Push a compile-time-known pack of children, reserving EXACTLY once. The
/// variadic builders know the child count at the call site, so the kids vector
/// grows in a single allocation instead of reallocating 1→2→4… as push_back
/// walks the pack — for a 3-child box that's 1 malloc, not 3.
template<typename... Cs> void push_all(std::vector<NodeRef>& v, Cs... cs){
    if constexpr (sizeof...(Cs) > 0){ v.reserve(sizeof...(Cs)); push(v, std::move(cs)...); }
}
/// #rrggbb string from a 0xRRGGBB int — used by many colour-taking mods.
inline std::string hexstr(std::uint32_t c){ static const char* H="0123456789abcdef"; std::string o="#"; for(int s=20;s>=0;s-=4)o+=H[(c>>s)&0xF]; return o; }
/// Compact float→string: integral values print bare (no ".000000") and any
/// fractional value has its trailing zeros trimmed, so scale(1.1f) emits "1.1"
/// not "1.100000". std::to_string always pads to 6 decimals on libc++/libstdc++,
/// which leaked into transform:/filter: CSS values — this is the shared fix.
inline std::string numstr(float f){
    long long i=(long long)f; if((float)i==f) return std::to_string(i);
    std::string s=std::to_string(f);
    while(s.size()&&s.back()=='0') s.pop_back();
    if(s.size()&&s.back()=='.') s.pop_back();
    return s;
}
/// Serialize a Len to CSS (px/%/rem/…) — used by edge-offset mods.
inline std::string lenstr(Len l){
    auto n=[](float f){ long i=(long)f; return (float)i==f? std::to_string(i) : std::to_string(f); };
    switch(l.unit){ case Unit::px:return n(l.value)+"px"; case Unit::pct:return n(l.value)+"%";
        case Unit::rem:return n(l.value)+"rem"; case Unit::em:return n(l.value)+"em";
        case Unit::vw:return n(l.value)+"vw"; case Unit::vh:return n(l.value)+"vh";
        case Unit::fr:return n(l.value)+"fr"; case Unit::fill:return "100%"; case Unit::hug:return "auto";
        case Unit::dvh:return n(l.value)+"dvh"; case Unit::dvw:return n(l.value)+"dvw"; }
    return n(l.value)+"px"; }
}
template <typename... Cs> NodeRef box(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::box; detail::push_all(n->kids, std::move(cs)...); finalize(*n); return n; }
template <typename... Cs> NodeRef row(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::box; n->style.flow=Flow::row; detail::push_all(n->kids, std::move(cs)...); finalize(*n); return n; }
template <typename... Cs> NodeRef col(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::box; n->style.flow=Flow::col; detail::push_all(n->kids, std::move(cs)...); finalize(*n); return n; }
template <typename... Cs> NodeRef stack(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::box; n->style.flow=Flow::stack; detail::push_all(n->kids, std::move(cs)...); finalize(*n); return n; }
/// `grid(children...)` — a real CSS grid container. Shape it with `grid_cols`,
/// `grid_rows`, `grid_areas`, and `gap`; place children with `col_span`/
/// `row_span`/`area`. The layout tool for dashboards, galleries, and any 2-D UI
/// that flex can only fake. e.g. grid(a,b,c,d) | grid_cols("1fr 1fr") | gap(16).
template <typename... Cs> NodeRef grid(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::box; n->style.flow=Flow::grid; detail::push_all(n->kids, std::move(cs)...); finalize(*n); return n; }

inline NodeRef text(std::string s){ auto n=detail::new_node(); n->kind=Kind::text; n->text=std::move(s); finalize(*n); return n; }
inline NodeRef text(long long v){ return text(std::to_string(v)); }
inline NodeRef text(int v){ return text(std::to_string(v)); }
inline NodeRef image(std::string src){ auto n=detail::new_node(); n->kind=Kind::image; n->src=std::move(src); finalize(*n); return n; }
inline NodeRef path(std::vector<Pt> pts, bool closed=false){ auto n=detail::new_node(); n->kind=Kind::path; n->points=std::move(pts); n->closed=closed; finalize(*n); return n; }
/// `input(value)` — a real text field. Style/placeholder/on_input via modifiers.
inline NodeRef input(std::string value={}){ auto n=detail::new_node(); n->kind=Kind::input; n->text=std::move(value); n->input_type="text"; finalize(*n); return n; }
/// `textarea(value)` — a multi-line text field. Same on_input/on_change flow.
inline NodeRef textarea(std::string value={}){ auto n=detail::new_node(); n->kind=Kind::textarea; n->text=std::move(value); finalize(*n); return n; }
/// `checkbox(on)` — a boolean toggle. `on_change` fires with value "true"/"false".
inline NodeRef checkbox(bool on=false){ auto n=detail::new_node(); n->kind=Kind::checkbox; n->checked=on; finalize(*n); return n; }
/// `radio(name, value, on)` — one choice in a named group; `on_change` fires with `value`.
inline NodeRef radio(std::string group, std::string value, bool on=false){ auto n=detail::new_node(); n->kind=Kind::radio; n->name=std::move(group); n->text=std::move(value); n->checked=on; finalize(*n); return n; }
/// One `option` for a `select`. `value` rides the wire; `label` (or value) is shown.
inline Opt option(std::string value, std::string label={}){ return {std::move(value), label.empty()?value:std::move(label)}; }
/// `select(options, chosen)` — a dropdown; `on_change` fires with the chosen value.
inline NodeRef select(std::vector<Opt> options, std::string chosen={}){ auto n=detail::new_node(); n->kind=Kind::select; n->options=std::move(options); n->selected=std::move(chosen); finalize(*n); return n; }
/// `button(label)` — a real <button>; pair with `tap(msg)`. Distinct from a
/// tappable box: it's keyboard-focusable and announced as a button by default.
inline NodeRef button(std::string label){ auto n=detail::new_node(); n->kind=Kind::button; n->text=std::move(label); finalize(*n); return n; }
/// `form(fields…) | on_submit(Save)` — a real <form> that groups named controls.
/// Enter in any field, or a button inside it, fires submit; the runtime gathers
/// every named field into the update value as "name=value&name2=value2".
template <typename... Cs> NodeRef form(Cs... cs){ auto n=detail::new_node(); n->kind=Kind::form; detail::push_all(n->kids, std::move(cs)...); n->style.flow=Flow::col; finalize(*n); return n; }
/// `video(url)` — a media player. `controls`/`autoplay`/`loop` via attr(); size
/// via w()/h()/aspect() like any node.
inline NodeRef video(std::string src){ auto n=detail::new_node(); n->kind=Kind::video; n->src=std::move(src); n->attrs.emplace_back("controls",""); finalize(*n); return n; }
/// `audio(url)` — an audio player with default controls.
inline NodeRef audio(std::string src){ auto n=detail::new_node(); n->kind=Kind::audio; n->src=std::move(src); n->attrs.emplace_back("controls",""); finalize(*n); return n; }
/// `markup(html)` — inject TRUSTED raw HTML (rich text, an SVG icon, embedded
/// content). The one primitive that is NOT auto-escaped — never pass user input.
/// (Debug/strict builds flag markup that carries <script>/on*= as `markup-unsafe`.)
inline NodeRef markup(std::string html){ auto n=detail::new_node(); n->kind=Kind::markup; n->text=std::move(html); finalize(*n); return n; }
/// `sanitized_html(html)` — render rich HTML from a POSSIBLY-UNTRUSTED source
/// (markdown output, CMS/user content). Strips `<script>`/`<style>`/`<iframe>`
/// blocks, `on*=` inline handlers, and `javascript:` URLs before injecting, so
/// formatting survives but active content can't. When in doubt, reach for this
/// instead of markup(). (Not a full HTML sanitiser — for high-stakes untrusted
/// input, sanitise server-side with a dedicated library; this is a safe default.)
inline NodeRef sanitized_html(std::string html){
    auto lower=[&](const std::string& s){ std::string o; o.reserve(s.size()); for(char c:s) o+=(char)((c>='A'&&c<='Z')?c+32:c); return o; };
    // drop <script|style|iframe|object|embed ...>...</...> blocks
    for(const char* tag : {"script","style","iframe","object","embed"}){
        std::string open="<"; open+=tag; std::string close="</"; close+=tag; close+=">";
        for(;;){
            std::string lo=lower(html);
            auto s=lo.find(open); if(s==std::string::npos) break;
            auto e=lo.find(close, s);
            auto end = (e==std::string::npos) ? html.size() : e+close.size();
            html.erase(s, end-s);
        }
    }
    // strip inline event handlers  on...="..."  / on...='...'
    { std::string lo=lower(html); std::size_t i=0;
      while((i=lo.find(" on", i))!=std::string::npos){
        std::size_t eq=lo.find('=', i);
        // only treat as a handler if it's on<word>=
        bool word=true; for(std::size_t j=i+3;j<eq && j<lo.size();++j){ char c=lo[j]; if(!((c>='a'&&c<='z'))){ word=false; break; } }
        if(eq==std::string::npos||!word){ i+=3; continue; }
        std::size_t vs=eq+1; char q = (vs<html.size()? html[vs] : '\0');
        std::size_t ve;
        if(q=='"'||q=='\''){ ve=html.find(q, vs+1); ve=(ve==std::string::npos)?html.size():ve+1; }
        else { ve=html.find_first_of(" >", vs); ve=(ve==std::string::npos)?html.size():ve; }
        html.erase(i, ve-i); lo=lower(html);
      } }
    // neutralise javascript: URLs
    { std::string lo=lower(html); std::size_t i=0;
      while((i=lo.find("javascript:", i))!=std::string::npos){ html.replace(i,11,"#"); lo=lower(html); i+=1; } }
    auto n=detail::new_node(); n->kind=Kind::markup; n->text=std::move(html); finalize(*n); return n;
}

// ═══════════════════════════════════════════════════════════════════════════
//  ONE uniform modifier. maya's principle: everything is a node, and everything
//  you do to a node is the SAME kind of thing — a `Mod`, a function Node→Node.
//  Style attrs, tap, key, on(state), at(breakpoint): all Mods, all `node | mod`.
//  Compose freely; they read like one clean sentence.
// ═══════════════════════════════════════════════════════════════════════════

// ObjectMod — a Node-mutating modifier stored WITHOUT a heap allocation for the
// common case. maya interns styles at compile time; waya's view() is dynamic, so
// we can't fully intern, but we CAN avoid the per-mod std::function heap alloc
// that dominated the old cost. A Mod holds a small inline buffer (fits any of
// waya's mod closures — they capture a few POD values) plus a function pointer;
// only an oversized closure falls back to the heap. Same value semantics, same
// API, ~zero allocation.
class Mod {
    static constexpr std::size_t Buf = 40;   // fits the vocabulary's closures
    // vtable-ish: invoke, copy, destroy — as raw function pointers (no RTTI).
    using Invoke  = void(*)(const void*, Node&);
    using CopyFn  = void(*)(void*, const void*);
    using MoveFn  = void(*)(void*, void*);
    using KillFn  = void(*)(void*);

    alignas(std::max_align_t) unsigned char buf_[Buf]{};
    void* heap_ = nullptr;         // set only when the closure doesn't fit inline
    std::size_t size_ = 0;         // sizeof the stored closure (for heap re-alloc)
    Invoke invoke_ = nullptr;
    CopyFn copy_   = nullptr;
    MoveFn move_   = nullptr;
    KillFn kill_   = nullptr;

    void* obj()             { return heap_ ? heap_ : (void*)buf_; }
    const void* obj() const { return heap_ ? heap_ : (const void*)buf_; }

public:
    Mod() = default;   // the identity (no-op) mod

    /// Construct from any callable f(Node&). Stored inline when it fits.
    template <typename F>
        requires (!std::is_same_v<std::decay_t<F>, Mod> && std::is_invocable_v<F&, Node&>)
    Mod(F f) {
        using D = std::decay_t<F>;
        size_   = sizeof(D);
        invoke_ = [](const void* p, Node& n){ (*const_cast<D*>(static_cast<const D*>(p)))(n); };
        copy_   = [](void* dst, const void* src){ new (dst) D(*static_cast<const D*>(src)); };
        move_   = [](void* dst, void* src){ new (dst) D(std::move(*static_cast<D*>(src))); };
        kill_   = [](void* p){ static_cast<D*>(p)->~D(); };
        if constexpr (sizeof(D) <= Buf && alignof(D) <= alignof(std::max_align_t))
            new (buf_) D(std::move(f));
        else { heap_ = ::operator new(sizeof(D)); new (heap_) D(std::move(f)); }
    }

    Mod(const Mod& o) { copy_from(o); }
    Mod(Mod&& o) noexcept { move_from(o); }
    Mod& operator=(const Mod& o){ if(this!=&o){ reset(); copy_from(o);} return *this; }
    Mod& operator=(Mod&& o) noexcept { if(this!=&o){ reset(); move_from(o);} return *this; }
    ~Mod(){ reset(); }

    /// Apply the mod to a node. No-op when empty.
    void apply(Node& n) const { if (invoke_) invoke_(obj(), n); }
    explicit operator bool() const { return invoke_ != nullptr; }

private:
    void reset(){
        if (invoke_) { kill_(obj()); if (heap_) ::operator delete(heap_); }
        heap_ = nullptr; size_ = 0; invoke_ = nullptr; copy_ = nullptr; move_ = nullptr; kill_ = nullptr;
    }
    void copy_from(const Mod& o){
        invoke_=o.invoke_; copy_=o.copy_; move_=o.move_; kill_=o.kill_; size_=o.size_;
        if(!o.invoke_) return;
        if(o.heap_){ heap_ = ::operator new(o.size_); copy_(heap_, o.obj()); }
        else       { copy_(buf_, o.obj()); }
    }
    void move_from(Mod& o){
        invoke_=o.invoke_; copy_=o.copy_; move_=o.move_; kill_=o.kill_; size_=o.size_;
        if(!o.invoke_) return;
        if(o.heap_){
            // steal the heap block; clear the source's vtable so its destructor
            // does NOT run kill_ on a now-empty inline buffer.
            heap_=o.heap_; o.heap_=nullptr;
            o.invoke_=nullptr; o.copy_=nullptr; o.move_=nullptr; o.kill_=nullptr; o.size_=0;
        } else {
            move_(buf_, o.obj());   // move-construct into our inline buffer
            o.reset();              // destroy the moved-from inline object
        }
    }
};
/// Mods compose: `a | b` is a Mod that applies a then b (so you can name bundles).
inline Mod operator|(Mod a, Mod b){ return Mod([a=std::move(a), b=std::move(b)](Node& n){ a.apply(n); b.apply(n); }); }

// A style-only mod — the common case. `sfn` takes a Style& mutator.
template <typename F> Mod sty(F f){ return Mod([f=std::move(f)](Node& n){ f(n.style); }); }
/// A do-nothing Mod — the identity for `|`. Makes conditional styling clean:
///   text(x) | (active ? bold : noop)
inline const Mod noop = Mod([](Node&){});

// ── Deferred finalize ───────────────────────────────────────────────
// finalize() re-hashes a node's whole field set; doing it after EVERY piped mod
// (`x | a | b | c` → 3 hashes) was the dominant per-frame cost. `Building` is a
// tiny proxy returned by `node | mod`: it applies mods WITHOUT re-hashing and
// finalizes exactly ONCE, lazily, when the node is consumed (converted to a
// NodeRef — as a child, a return value, or at render). maya's deferred style
// resolution, transposed. The API is unchanged: a Building IS-A node handle.
struct Building {
    NodeRef n;
    bool dirty = false;
    explicit Building(NodeRef node) : n(std::move(node)) {}
    /// finalize-on-consume: hand back a fully-hashed NodeRef.
    operator NodeRef() { if (dirty) { finalize(*n); dirty = false; } return n; }
    Node& operator*()  const { return *n; }
    Node* operator->() const { return n.get(); }
    NodeRef done() { if (dirty) { finalize(*n); dirty = false; } return n; }
};
/// `node | mod` — apply and DEFER the hash. Chained pipes stay a Building, so a
/// whole chain costs one finalize (on consume), not one per mod.
inline Building operator|(NodeRef n, const Mod& m){ m.apply(*n); Building b{std::move(n)}; b.dirty = true; return b; }
inline Building operator|(Building b, const Mod& m){ m.apply(*b.n); b.dirty = true; return b; }

// ── conditional mods ─ apply a mod only when a condition holds ──────────────
// Kills the `cond ? someMod : noop` ternary that litters real views. Reads as
// intent: `text(x) | when_(active, semibold) | when_(pinned, ring(brand))`.
inline Mod when_(bool cond, Mod m){ return cond ? m : noop; }
/// `when_(cond, a, b)` — a when true, else b.
inline Mod when_(bool cond, Mod a, Mod b){ return cond ? a : b; }
/// `maybe(opt, mod-of-value)` — apply a mod built from an optional's value when
/// it's present: `img | maybe(alt_text, [](auto& s){ return attr("alt", s); })`.
template <typename T, typename Fn>
Mod maybe(const std::optional<T>& o, Fn make){ return o ? make(*o) : noop; }

// ── colour & text ──────────────────────────────────────────────────────────────
inline Mod fg(std::uint32_t c){ return sty([=](Style& s){ s.has_fg=true; s.fg=c; }); }
inline Mod bg(std::uint32_t c){ return sty([=](Style& s){ s.has_bg=true; s.bg=c; }); }
/// Typed-Color overloads: `fg(indigo)`, `bg(rgba(0,0,0,.4))`. An opaque Color
/// uses the fast interned fg/bg path; a translucent one rides the css() channel
/// so alpha is preserved.
inline Mod fg(Color c){ return c.has_alpha()
    ? sty([v=c.css()](Style& s){ s.extra.emplace_back("color", v); })
    : sty([c](Style& s){ s.has_fg=true; s.fg=c.opaque(); }); }
inline Mod bg(Color c){ return c.has_alpha()
    ? sty([v=c.css()](Style& s){ s.extra.emplace_back("background", v); })
    : sty([c](Style& s){ s.has_bg=true; s.bg=c.opaque(); }); }
inline Mod font(Len sz){ return sty([=](Style& s){ s.font_size=sz; }); }
inline Mod font(float px_){ return font(px(px_)); }
/// `font_fluid(min_px, max_px)` — responsive type: the size scales with the
/// viewport width between the two bounds, so a big heading shrinks on a phone
/// instead of overflowing. Uses CSS clamp(); no media queries. `max` is also the
/// desktop size. e.g. font_fluid(28, 76) — 28px on a phone up to 76px on desktop.
inline Mod font_fluid(float min_px, float max_px){
    // preferred = min + (max-min) scaled across ~[360px, 1200px] viewport.
    float span = max_px - min_px;
    std::string pref = detail::numstr(min_px) + "px + " + detail::numstr(span * 100.0f / 840.0f) + "vw";
    return sty([=](Style& s){ s.extra.emplace_back("font-size",
        "clamp(" + detail::numstr(min_px) + "px, calc(" + pref + "), " + detail::numstr(max_px) + "px)"); });
}
inline Mod weight(Weight w){ return sty([=](Style& s){ s.weight=w; }); }
inline const Mod bold      = sty([](Style& s){ s.weight=Weight::bold; });
inline const Mod semibold  = sty([](Style& s){ s.weight=Weight::semibold; });
inline const Mod medium    = sty([](Style& s){ s.weight=Weight::medium; });
inline const Mod italic    = sty([](Style& s){ s.italic=true; });
inline const Mod underline = sty([](Style& s){ s.underline=true; });
inline const Mod strike    = sty([](Style& s){ s.strike=true; });
inline Mod text_align(Justify j){ return sty([=](Style& s){ s.text_align=j; }); }
/// text-align convenience consts (so you never need the Justify enum for text).
inline const Mod text_center = sty([](Style& s){ s.text_align=Justify::center; });
inline const Mod text_left   = sty([](Style& s){ s.text_align=Justify::start; });
inline const Mod text_right  = sty([](Style& s){ s.text_align=Justify::end; });
inline Mod leading(float lh){ return sty([=](Style& s){ s.has_lh=true; s.line_height=lh; }); }
inline Mod tracking(float ls){ return sty([=](Style& s){ s.has_ls=true; s.letter_spacing=ls; }); }
/// `tracking_em(v)` — letter-spacing in em (scales with font size). Negative
/// tightens display headings: `tracking_em(-0.03f)`.
inline Mod tracking_em(float em){ return sty([=](Style& s){ s.extra.emplace_back("letter-spacing", detail::numstr(em)+"em"); }); }
inline const Mod nowrap_text = sty([](Style& s){ s.extra.emplace_back("white-space","nowrap"); });
inline const Mod truncate = sty([](Style& s){ s.extra.emplace_back("white-space","nowrap"); s.extra.emplace_back("overflow","hidden"); s.extra.emplace_back("text-overflow","ellipsis"); });
/// `line_clamp(n)` — truncate multi-line text to n lines with an ellipsis.
inline Mod line_clamp(int lines){ return sty([=](Style& s){
    s.extra.emplace_back("display","-webkit-box");
    s.extra.emplace_back("-webkit-line-clamp", std::to_string(lines));
    s.extra.emplace_back("-webkit-box-orient","vertical");
    s.extra.emplace_back("overflow","hidden"); }); }
inline const Mod uppercase  = sty([](Style& s){ s.extra.emplace_back("text-transform","uppercase"); });
inline const Mod lowercase  = sty([](Style& s){ s.extra.emplace_back("text-transform","lowercase"); });
inline const Mod capitalize = sty([](Style& s){ s.extra.emplace_back("text-transform","capitalize"); });
/// `tabular_nums` — fixed-width digits so numbers don't jitter (counters, tables).
inline const Mod tabular_nums = sty([](Style& s){ s.extra.emplace_back("font-variant-numeric","tabular-nums"); });
/// `no_select` — text isn't selectable (buttons, chrome).
inline const Mod no_select = sty([](Style& s){ s.extra.emplace_back("user-select","none"); s.extra.emplace_back("-webkit-user-select","none"); });
/// `no_pointer` — the node ignores pointer events (overlays that pass clicks through).
inline const Mod no_pointer = sty([](Style& s){ s.extra.emplace_back("pointer-events","none"); });

// ── box model ────────────────────────────────────────────────────────────────
inline Mod pad(Len l){ return sty([=](Style& s){ s.pad=l; }); }
inline Mod pad(float p){ return pad(px(p)); }
/// `pad_fluid(min_px, max_px)` — padding that shrinks on small screens (clamp,
/// scaled by viewport width). So a roomy card on desktop isn't cramped—or
/// overflowing—on a phone. e.g. pad_fluid(16, 56).
inline Mod pad_fluid(float min_px, float max_px){
    float span = max_px - min_px;
    return sty([=](Style& s){ s.extra.emplace_back("padding",
        "clamp(" + detail::numstr(min_px) + "px, calc(" + detail::numstr(min_px) + "px + "
        + detail::numstr(span * 100.0f / 840.0f) + "vw), " + detail::numstr(max_px) + "px)"); });
}
inline Mod pad_x(Len l){ return sty([=](Style& s){ s.pad_x=l; }); }
inline Mod pad_x(float p){ return pad_x(px(p)); }
inline Mod pad_y(Len l){ return sty([=](Style& s){ s.pad_y=l; }); }
inline Mod pad_y(float p){ return pad_y(px(p)); }
inline Mod margin(Len l){ return sty([=](Style& s){ s.margin=l; }); }
inline Mod margin(float m){ return margin(px(m)); }
/// `margin_left`/`margin_right` — single-side spacing (a spacer nudge, an
/// icon offset). `margin_left(auto_px)` = push to the right edge in a flex row.
inline Mod margin_left(float m){ return sty([=](Style& s){ s.extra.emplace_back("margin-left", detail::numstr(m)+"px"); }); }
inline Mod margin_right(float m){ return sty([=](Style& s){ s.extra.emplace_back("margin-right", detail::numstr(m)+"px"); }); }
/// `no_shrink` — flex:0 0 auto: this item keeps its size in a tight row (icons,
/// fixed rails, a status LED that must not squash).
inline const Mod no_shrink = sty([](Style& s){ s.extra.emplace_back("flex","0 0 auto"); });
inline Mod w(Len l){ return sty([=](Style& s){ s.w=l; }); }
inline Mod w(float v){ return w(px(v)); }
inline Mod h(Len l){ return sty([=](Style& s){ s.h=l; }); }
inline Mod h(float v){ return h(px(v)); }
inline Mod size(Len side){ return sty([=](Style& s){ s.w=side; s.h=side; }); }   // square
inline Mod size(float side){ return size(px(side)); }
inline Mod max_h(Len l){ return sty([=](Style& s){ s.max_h=l; }); }
inline Mod max_h(float v){ return max_h(px(v)); }
inline Mod max_w(Len l){ return sty([=](Style& s){ s.max_w=l; }); }
inline Mod max_w(float v){ return max_w(px(v)); }
inline Mod min_h(Len l){ return sty([=](Style& s){ s.min_h=l; }); }
/// `min_h(0)` is THE flexbox scroll incantation (let a flex child shrink so its
/// scroll region works) — but Len{0,px} reads as "unset", so 0 is special-cased
/// to emit explicitly rather than silently doing nothing.
inline Mod min_h(float v){ return v==0 ? sty([](Style& s){ s.extra.emplace_back("min-height","0"); }) : min_h(px(v)); }
inline Mod min_w(Len l){ return sty([=](Style& s){ s.min_w=l; }); }
inline Mod min_w(float v){ return v==0 ? sty([](Style& s){ s.extra.emplace_back("min-width","0"); }) : min_w(px(v)); }
inline Mod round(Len l){ return sty([=](Style& s){ s.radius=l; }); }
inline Mod round(float r){ return round(px(r)); }
/// `round(tl, tr, br, bl)` — per-corner radii (asymmetric pills, tabs,
/// speech bubbles: `round(10, 4, 4, 10)`).
inline Mod round(float tl, float tr, float br, float bl){
    return sty([=](Style& s){ s.extra.emplace_back("border-radius",
        detail::numstr(tl)+"px "+detail::numstr(tr)+"px "+detail::numstr(br)+"px "+detail::numstr(bl)+"px"); }); }
/// int overload so `round(10)` is never ambiguous with std::round (from <cmath>,
/// which an app may pull in). Corner radius in px.
inline Mod round(int r){ return round(px((float)r)); }
inline const Mod pill = sty([](Style& s){ s.radius=px(9999); });
inline Mod border(float width_, std::uint32_t color){ return sty([=](Style& s){ s.has_border=true; s.border_w=px(width_); s.border_c=color; }); }
/// `border(1, rgba(accent, .25f))` — a translucent border (the hairline-with-
/// alpha look every dark panel uses). Alpha-free Colors take the fast path.
inline Mod border(float width_, Color c){ return c.has_alpha()
    ? sty([w_=width_, v=c.css()](Style& s){ s.extra.emplace_back("border", detail::numstr(w_)+"px solid "+v); })
    : border(width_, c.opaque()); }
/// `border_color(c)` — recolor an existing border without restating its width
/// (the hover/active accent-shift pattern).
inline Mod border_color(Color c){ return sty([v=c.css()](Style& s){ s.extra.emplace_back("border-color", v); }); }
inline Mod border_color(std::uint32_t c){ return border_color(rgb(c)); }
/// directional borders — the common "divider under a header" / "left rail" cases.
inline Mod border_bottom(float w_, std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("border-bottom", std::to_string((int)w_)+"px solid "+detail::hexstr(c)); }); }
inline Mod border_top(float w_, std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("border-top", std::to_string((int)w_)+"px solid "+detail::hexstr(c)); }); }
inline Mod border_left(float w_, std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("border-left", std::to_string((int)w_)+"px solid "+detail::hexstr(c)); }); }
inline Mod border_right(float w_, std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("border-right", std::to_string((int)w_)+"px solid "+detail::hexstr(c)); }); }
inline Mod aspect(float ratio){ return sty([=](Style& s){ s.extra.emplace_back("aspect-ratio", detail::numstr(ratio)); }); }

// ── layout ────────────────────────────────────────────────────────────────
inline const Mod wrap = sty([](Style& s){ s.wrap=Wrap::wrap; });
inline const Mod nowrap = sty([](Style& s){ s.wrap=Wrap::nowrap; });
inline Mod justify(Justify j){ return sty([=](Style& s){ s.justify=j; }); }
inline Mod align(Align a){ return sty([=](Style& s){ s.align=a; }); }
inline Mod gap(Len l){ return sty([=](Style& s){ s.gap=l; }); }
inline Mod gap(float g){ return gap(px(g)); }
// (row_gap/col_gap live in complete.hpp with the rest of the axis-gap family.)
/// `pad_safe(min_px)` — padding that RESPECTS the device safe area (notches,
/// home bars): each side is max(min_px, env(safe-area-inset-…)). The full-bleed
/// app-frame primitive; box-sizing is pinned so 100dvh math stays exact.
inline Mod pad_safe(float min_px=14){
    return sty([=](Style& s){ auto m = detail::numstr(min_px)+"px";
        s.extra.emplace_back("padding",
            "max("+m+", env(safe-area-inset-top)) max("+m+", env(safe-area-inset-right)) "
            "max("+m+", env(safe-area-inset-bottom)) max("+m+", env(safe-area-inset-left))");
        s.extra.emplace_back("box-sizing","border-box"); }); }
inline Mod grow(float g=1){ return sty([=](Style& s){ s.has_grow=true; s.grow=g; }); }
inline Mod shrink(float g=1){ return sty([=](Style& s){ s.has_shrink=true; s.shrink=g; }); }
/// `center` — centre children both axes; the single most common layout, one word.
inline const Mod center = sty([](Style& s){ if(s.flow==Flow::none) s.flow=Flow::row; s.justify=Justify::center; s.align=Align::center; });
/// Convenience alignment consts (so you don't need the Justify/Align enums for
/// the everyday cases — these are what `css("align-items"/"justify-content")` was for).
inline const Mod items_center  = sty([](Style& s){ s.align=Align::center; });
inline const Mod items_start   = sty([](Style& s){ s.align=Align::start; });
inline const Mod items_end     = sty([](Style& s){ s.align=Align::end; });
inline const Mod items_stretch = sty([](Style& s){ s.align=Align::stretch; });
inline const Mod justify_center  = sty([](Style& s){ s.justify=Justify::center; });
inline const Mod justify_start   = sty([](Style& s){ s.justify=Justify::start; });
inline const Mod justify_end     = sty([](Style& s){ s.justify=Justify::end; });
inline const Mod justify_between = sty([](Style& s){ s.justify=Justify::between; });
inline const Mod place_center  = sty([](Style& s){ if(s.flow==Flow::none) s.flow=Flow::row; s.justify=Justify::center; s.align=Align::center; });
inline const Mod items_baseline = sty([](Style& s){ s.align=Align::baseline; });
/// `center_x` — horizontally centre a block in its parent (the `margin:0 auto`
/// pattern for a max-width page container).
inline const Mod center_x = sty([](Style& s){ s.extra.emplace_back("margin-left","auto"); s.extra.emplace_back("margin-right","auto"); });
/// `flex_1` — grow AND shrink to fill available space (the flex:1 shorthand).
inline const Mod flex_1 = sty([](Style& s){ s.has_grow=true; s.grow=1; s.has_shrink=true; s.shrink=1; s.extra.emplace_back("flex-basis","0%"); });
/// `flex_none` — don't grow or shrink (icons/avatars that must keep their size).
inline const Mod flex_none = sty([](Style& s){ s.has_grow=true; s.grow=0; s.has_shrink=true; s.shrink=0; });
/// `outline_none` — remove the default focus outline (pair with focus_ring for a
/// nicer one; never remove it without a visible replacement).
inline const Mod outline_none = sty([](Style& s){ s.extra.emplace_back("outline","none"); });
/// `bg_none` — no background fill. `no_border` — remove any border. Together with
/// `outline_none` these give an "unstyled" input that inherits its parent.
inline const Mod bg_none   = sty([](Style& s){ s.extra.emplace_back("background","transparent"); });
inline const Mod no_border = sty([](Style& s){ s.extra.emplace_back("border","none"); });
/// `scroll_margin(px)` — offset an anchor target from the top when scrolled to
/// (so a sticky header doesn't cover it). The #hash-link fix.
inline Mod scroll_margin(float px_){ return sty([=](Style& s){ s.extra.emplace_back("scroll-margin-top", std::to_string((int)px_)+"px"); }); }
/// `round_cap` — rounded ends on a drawn `path` stroke (charts, sparklines).
inline const Mod round_cap = sty([](Style& s){ s.extra.emplace_back("stroke-linecap","round"); s.extra.emplace_back("stroke-linejoin","round"); });
// Flex-direction as MODS (the col()/row() builders make containers; these flip
// an existing container's axis — essential for responsive `on_phone(column)`).
inline const Mod column     = sty([](Style& s){ s.flow=Flow::col; });
inline const Mod horizontal = sty([](Style& s){ s.flow=Flow::row; });
// ── grid placement ──────────────────────────────────────────────────────────
// Turn a container into a grid and shape its tracks. These set display:grid
// implicitly, so `box(...) | grid_cols("1fr 1fr")` just works without grid().
/// `grid_cols("1fr 2fr")` or `grid_cols(3)` for N equal columns.
inline Mod grid_cols(std::string tracks){ return sty([=](Style& s){ s.flow=Flow::grid; s.extra.emplace_back("grid-template-columns", tracks); }); }
inline Mod grid_cols(int n){ return grid_cols("repeat(" + std::to_string(n) + ",minmax(0,1fr))"); }
/// `grid_template("2fr 1fr 1fr")` — alias for grid_cols(tracks) (explicit tracks).
inline Mod grid_template(std::string tracks){ return grid_cols(std::move(tracks)); }
/// `grid_rows("auto 1fr")` or `grid_rows(2)` for N equal rows.
inline Mod grid_rows(std::string tracks){ return sty([=](Style& s){ s.flow=Flow::grid; s.extra.emplace_back("grid-template-rows", tracks); }); }
inline Mod grid_rows(int n){ return grid_rows("repeat(" + std::to_string(n) + ",minmax(0,1fr))"); }
/// `grid_areas("'nav main' 'nav foot'")` — named template areas; place children
/// with `area("nav")`. The whole holy-grail layout in two lines.
inline Mod grid_areas(std::string tmpl){ return sty([=](Style& s){ s.flow=Flow::grid; s.extra.emplace_back("grid-template-areas", tmpl); }); }
/// `auto_grid(min_px)` — a responsive gallery: as many equal columns as fit at
/// >= min_px each, wrapping automatically. The one-liner for card grids.
inline Mod auto_grid(float min_px){ return sty([=](Style& s){ s.flow=Flow::grid;
    s.extra.emplace_back("grid-template-columns",
        "repeat(auto-fit,minmax(min(" + std::to_string((int)min_px) + "px,100%),1fr))"); }); }
/// `col_span(n)` / `row_span(n)` — a grid CHILD spanning n tracks.
inline Mod col_span(int n){ return sty([=](Style& s){ s.extra.emplace_back("grid-column", "span " + std::to_string(n)); }); }
inline Mod row_span(int n){ return sty([=](Style& s){ s.extra.emplace_back("grid-row", "span " + std::to_string(n)); }); }
/// `area("nav")` — place a grid CHILD into a named area from grid_areas().
inline Mod area(std::string name){ return sty([=](Style& s){ s.extra.emplace_back("grid-area", name); }); }
/// `between` — push children to opposite ends.
inline const Mod between = sty([](Style& s){ s.justify=Justify::between; });
inline Mod overflow(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("overflow", v); }); }
inline const Mod scroll = sty([](Style& s){ s.extra.emplace_back("overflow","auto"); });
inline const Mod scroll_x = sty([](Style& s){ s.extra.emplace_back("overflow-x","auto"); s.extra.emplace_back("overflow-y","hidden"); });
inline const Mod scroll_y = sty([](Style& s){ s.extra.emplace_back("overflow-y","auto"); s.extra.emplace_back("overflow-x","hidden"); });
inline const Mod clip   = sty([](Style& s){ s.extra.emplace_back("overflow","hidden"); });
/// `no_scrollbar` — scroll still works, the scrollbar chrome is hidden (carousels).
/// Uses the standard `scrollbar-width` (widely supported in modern browsers).
inline const Mod no_scrollbar = sty([](Style& s){
    s.extra.emplace_back("scrollbar-width","none");
    s.extra.emplace_back("-ms-overflow-style","none"); });

// ── position ────────────────────────────────────────────────────
inline Mod absolute(Len top_={}, Len left_={}){ return sty([=](Style& s){ s.pos=Pos::absolute; s.top=top_; s.left=left_; }); }
inline const Mod fixed  = sty([](Style& s){ s.pos=Pos::fixed; });
inline const Mod sticky = sty([](Style& s){ s.pos=Pos::sticky; });
inline const Mod relative = sty([](Style& s){ s.pos=Pos::relative; });
/// `positioned()` — make this box the positioning ANCHOR for its absolutely-
/// positioned children (pin_top_right, absolute, …). Without it, `position:
/// absolute` children resolve against the viewport, not the box — the classic
/// web surprise. Put it on the container: `box(content, badge|pin_top_right()) |
/// positioned()`.
inline const Mod positioned = sty([](Style& s){ if(s.pos==Pos::none) s.pos=Pos::relative; });
// Individual edge offsets — emitted via the extra channel so an explicit 0 works
// (the top/left/… fields treat Len{0} as "unset"). Accept a Len or a bare px number.
inline Mod top(Len l){ return sty([=](Style& s){ s.extra.emplace_back("top", detail::lenstr(l)); }); }
inline Mod top(float v){ return top(px(v)); }
inline Mod bottom(Len l){ return sty([=](Style& s){ s.extra.emplace_back("bottom", detail::lenstr(l)); }); }
inline Mod bottom(float v){ return bottom(px(v)); }
inline Mod left(Len l){ return sty([=](Style& s){ s.extra.emplace_back("left", detail::lenstr(l)); }); }
inline Mod left(float v){ return left(px(v)); }
inline Mod right(Len l){ return sty([=](Style& s){ s.extra.emplace_back("right", detail::lenstr(l)); }); }
inline Mod right(float v){ return right(px(v)); }
/// `sticky_top(offset)` — the ubiquitous sticky header: sticks to the top of the
/// scroll container at `offset` (default 0). One word for the most common
/// position pattern on the web.
inline Mod sticky_top(float offset=0){ return sty([=](Style& s){ s.pos=Pos::sticky; s.extra.emplace_back("top", std::to_string((int)offset)+"px"); }); }
inline Mod sticky_bottom(float offset=0){ return sty([=](Style& s){ s.pos=Pos::sticky; s.extra.emplace_back("bottom", std::to_string((int)offset)+"px"); }); }
/// Pin an absolutely-positioned child to a CORNER of its positioned ancestor —
/// the classic "badge on a card" / "close button" placement.
inline Mod pin_top_right(float o=8){ return sty([=](Style& s){ s.pos=Pos::absolute; auto p=std::to_string((int)o)+"px"; s.extra.emplace_back("top",p); s.extra.emplace_back("right",p); }); }
inline Mod pin_top_left(float o=8){ return sty([=](Style& s){ s.pos=Pos::absolute; auto p=std::to_string((int)o)+"px"; s.extra.emplace_back("top",p); s.extra.emplace_back("left",p); }); }
inline Mod pin_bottom_right(float o=8){ return sty([=](Style& s){ s.pos=Pos::absolute; auto p=std::to_string((int)o)+"px"; s.extra.emplace_back("bottom",p); s.extra.emplace_back("right",p); }); }
inline Mod pin_bottom_left(float o=8){ return sty([=](Style& s){ s.pos=Pos::absolute; auto p=std::to_string((int)o)+"px"; s.extra.emplace_back("bottom",p); s.extra.emplace_back("left",p); }); }
inline Mod inset(Len t, Len r, Len b, Len l){
    // Emit the CSS `inset` shorthand via the extra channel, so an explicit 0
    // works (Len{0} would be treated as "unset" by the top/left/… path).
    auto v=[](Len x){ return x.unit==Unit::pct? std::to_string((int)x.value)+"%" : std::to_string((int)x.value)+"px"; };
    return sty([=](Style& s){ s.extra.emplace_back("inset", v(t)+" "+v(r)+" "+v(b)+" "+v(l)); });
}
/// `pin()` — stretch to all four edges of the nearest positioned ancestor
/// (inset:0). Pair with `absolute`/`fixed`. The overlay/full-bleed primitive.
inline Mod pin(){ return sty([](Style& s){ s.extra.emplace_back("inset", "0"); }); }
/// `at_pct(top%, left%)` — absolutely position a node at a PERCENTAGE of its
/// positioned ancestor, centred on that point, via INLINE style (not an interned
/// class). This matters for animation: a node whose position changes every frame
/// (a moving game piece, a drifting particle) would otherwise mint a brand-new
/// interned CSS class per unique coordinate — unbounded class growth and a
/// defeated style cache. Emitting top/left as an inline style keeps the class
/// stable (only the cheap attr delta changes each tick) and rides the fast
/// morphAttrs paint path. Centres via translate(-50%,-50%) and hints the
/// compositor with will-change, so motion stays on the GPU. Pair with a sized,
/// rounded child for a dot/sprite; the node itself becomes position:absolute.
inline Mod at_pct(float top_pct, float left_pct){
    char buf[96];
    std::snprintf(buf, sizeof(buf),
        "position:absolute;top:%.3f%%;left:%.3f%%;transform:translate(-50%%,-50%%);will-change:top,left",
        top_pct, left_pct);
    std::string style = buf;
    return {[style](Node& n){ n.attrs.emplace_back("style", style); }};
}
inline Mod z(int zi){ return sty([=](Style& s){ s.has_z=true; s.z=zi; }); }

// ── frame-animated properties ─ inline style, class-stable (see at_pct) ──────
// A property that CHANGES EVERY FRAME (a moving sprite's `left`, a progress
// bar's `width`, a live `transform`) must NOT go through the interned style
// mods (left/w/translate/…): each unique value would mint a brand-new CSS class,
// so a 30fps animation leaks thousands of one-off classes, bloats the
// stylesheet, and ships a full class-swap (set_shell) every tick. These emit the
// property as INLINE style so the element's class stays stable and only a tiny
// attr delta travels each frame. Use them for anything that moves continuously;
// keep the static properties (size, colour, rounding) on the normal mods.
/// `animate_style("left", "42.5%")` — set ONE property inline (class-stable).
/// The general form; the named helpers below cover the common animated axes.
/// Multiple animate_style/at_* mods on one node MERGE into a single style attr
/// (so `at_left(x) | at_width(w)` both apply instead of the second winning).
inline Mod animate_style(std::string prop, std::string value){
    return {[prop=std::move(prop), value=std::move(value)](Node& n){
        std::string decl = prop + ":" + value;
        for(auto& a : n.attrs){
            if(a.first == "style"){
                if(!a.second.empty() && a.second.back() != ';') a.second += ';';
                a.second += decl;
                return;
            }
        }
        n.attrs.emplace_back("style", std::move(decl));
    }};
}
namespace detail {
inline std::string pctstr(float v){ char b[24]; std::snprintf(b,sizeof b,"%.3f%%",v); return b; }
inline std::string pxstr (float v){ char b[24]; std::snprintf(b,sizeof b,"%.2fpx",v); return b; }
}
/// `at_left(pct)` / `at_top(pct)` — a continuously-moving position, inline
/// (class-stable). Percentage of the positioned ancestor. Unlike at_pct these
/// don't centre the node (no translate) — the element's own edge is placed.
inline Mod at_left(float pct){ return animate_style("left", detail::pctstr(pct)); }
inline Mod at_top (float pct){ return animate_style("top",  detail::pctstr(pct)); }
/// `at_width(pct)` / `at_height(pct)` — a continuously-changing size, inline
/// (class-stable). The progress-bar / meter fill case.
inline Mod at_width (float pct){ return animate_style("width",  detail::pctstr(pct)); }
inline Mod at_height(float pct){ return animate_style("height", detail::pctstr(pct)); }
/// `at_left_px(x)` etc. — the pixel variants for px-based motion.
inline Mod at_left_px(float x){ return animate_style("left", detail::pxstr(x)); }
inline Mod at_top_px (float y){ return animate_style("top",  detail::pxstr(y)); }
/// `move_xy(x, y)` — a live translate offset, inline (the transform-based motion
/// path; cheapest for the compositor as it never triggers layout).
inline Mod move_xy(float x, float y=0){
    char b[48]; std::snprintf(b,sizeof b,"translate(%.2fpx,%.2fpx)", x, y);
    return animate_style("transform", b);
}

// ── effects ────────────────────────────────────────────────────────
inline Mod shadow(std::string spec=""){ return sty([=](Style& s){ s.has_shadow=true; s.shadow_spec=spec; }); }

namespace detail {
/// Append one layer to the node's box-shadow, COMPOSABLY: `ring(…) | inset_light()
/// | glow_under(…)` builds a single comma-joined box-shadow, exactly how depth
/// is really drawn (every polished bevel/knob is 2–3 layered shadows).
inline void add_shadow_layer(Style& s, std::string layer){
    for (auto& [k,v] : s.extra) if (k=="box-shadow"){ v += ", " + layer; return; }
    s.extra.emplace_back("box-shadow", std::move(layer));
}
}

/// `ring(color, w)` — a crisp outline drawn OUTSIDE the border-box that never
/// shifts layout (focus rings, selection states, avatar rims).
inline Mod ring(Color c, float w_=2){ return sty([=](Style& s){ detail::add_shadow_layer(s, "0 0 0 "+detail::numstr(w_)+"px "+c.css()); }); }
inline Mod ring(std::uint32_t c, float w_=2){ return ring(rgb(c), w_); }
/// `inset_ring(color, w)` — the same ring drawn INSIDE the box (segmented
/// displays, wells, pressed states).
inline Mod inset_ring(Color c, float w_=1){ return sty([=](Style& s){ detail::add_shadow_layer(s, "inset 0 0 0 "+detail::numstr(w_)+"px "+c.css()); }); }
inline Mod inset_ring(std::uint32_t c, float w_=1){ return inset_ring(rgb(c), w_); }
/// `inset_light(strength)` / `inset_dark(strength)` — the top-highlight and
/// bottom-shade of a bevel. `button-like` = `inset_light() | inset_dark()`.
inline Mod inset_light(float a=.25f, float y_=1){ return sty([=](Style& s){ detail::add_shadow_layer(s, "inset 0 "+detail::numstr(y_)+"px 0 rgba(255,255,255,"+detail::numstr(a)+")"); }); }
inline Mod inset_dark(float a=.35f, float y_=-2){ return sty([=](Style& s){ detail::add_shadow_layer(s, "inset 0 "+detail::numstr(y_)+"px 4px rgba(0,0,0,"+detail::numstr(a)+")"); }); }
/// `inset_glow(color, blur)` — a soft luminous bloom INSIDE the box (CRT
/// panes, neon wells, focus pools). Composes with rings and outer glows.
inline Mod inset_glow(Color c, float blur_=24){ return sty([=](Style& s){ detail::add_shadow_layer(s, "inset 0 0 "+detail::numstr(blur_)+"px "+c.css()); }); }
inline Mod inset_glow(std::uint32_t c, float blur_=24){ return inset_glow(rgb(c), blur_); }
/// `glow_under(color, blur, y)` — a coloured drop glow beneath (active tabs,
/// hot buttons, LED bleed). Composes with rings/insets on the same node.
inline Mod glow_under(Color c, float blur_=16, float y_=6){ return sty([=](Style& s){ detail::add_shadow_layer(s, "0 "+detail::numstr(y_)+"px "+detail::numstr(blur_)+"px "+detail::numstr(-blur_/4)+"px "+c.css()); }); }
inline Mod glow_under(std::uint32_t c, float blur_=16, float y_=6){ return glow_under(rgb(c), blur_, y_); }

inline Mod opacity(float o){ return sty([=](Style& s){ s.has_opacity=true; s.opacity=o; }); }
inline const Mod pointer = sty([](Style& s){ s.cursor=Cursor::pointer; });
/// `clickable` — re-enable pointer events on a child inside a `no_pointer`
/// layer (a live control inside a decorative overlay).
inline const Mod clickable  = sty([](Style& s){ s.extra.emplace_back("pointer-events","auto"); });
// (inline_block lives in complete.hpp with the browser-parity display mods.)
inline Mod cursor(Cursor c){ return sty([=](Style& s){ s.cursor=c; }); }
inline Mod transition(std::string spec="all .15s ease"){ return sty([=](Style& s){ s.has_transition=true; s.transition_spec=spec; }); }
inline Mod blur(float px_){ return sty([=](Style& s){ s.extra.emplace_back("filter","blur("+std::to_string((int)px_)+"px)"); }); }
inline Mod backdrop_blur(float px_){ return sty([=](Style& s){ s.extra.emplace_back("backdrop-filter","blur("+std::to_string((int)px_)+"px)"); }); }
inline Mod scale(float f){ return sty([=](Style& s){ s.extra.emplace_back("transform","scale("+detail::numstr(f)+")"); }); }
inline Mod rotate(float deg){ return sty([=](Style& s){ s.extra.emplace_back("transform","rotate("+std::to_string((int)deg)+"deg)"); }); }
/// `translate(x, y)` — nudge a node by px on each axis (offsets, tooltips, art).
inline Mod translate(float x, float y=0){ return sty([=](Style& s){ s.extra.emplace_back("transform",
    "translate("+std::to_string((int)x)+"px,"+std::to_string((int)y)+"px)"); }); }
inline Mod stroke(std::uint32_t color, float width_=2){ return sty([=](Style& s){ s.has_fg=true; s.fg=color; s.has_stroke_w=true; s.stroke_w=width_; }); }

// ── motion ─ composable animation mods over the shell's @keyframes library ───
// Each just sets the `animation` shorthand referencing a `wa-*` keyframe defined
// once in the page shell. They compose with everything and automatically honour
// prefers-reduced-motion (the shell neutralises animations for that preference).
/// `animate("wa-fade-up", 400)` — the general form: any keyframe by name, a
/// duration in ms, and an easing. Use the named helpers below for the common ones.
inline Mod animate(std::string keyframes, int ms=400, std::string ease="cubic-bezier(.2,.7,.2,1)", std::string fill="both"){
    return sty([=](Style& s){ s.extra.emplace_back("animation",
        keyframes + " " + std::to_string(ms) + "ms " + ease + " " + fill); });
}
/// `custom_animation(name, spec, ms)` — define AND apply a brand-new keyframe in
/// one call. `spec` is the @keyframes body, e.g. "0%{transform:none}50%{transform:
/// rotate(6deg)}100%{transform:none}". The keyframe is registered on the document
/// (deduped by name across every use), so a component can ship its own animation
/// without touching the shell. This is the seam that makes the motion vocabulary
/// open-ended: anything you can write in CSS keyframes, you can animate here.
inline Mod custom_animation(std::string name, std::string spec, int ms=600,
                            std::string ease="ease", std::string fill="both", std::string iter="1"){
    assets().keyframes(name, spec);
    return sty([=](Style& s){ s.extra.emplace_back("animation",
        name + " " + std::to_string(ms) + "ms " + ease + " " + iter + " " + fill); });
}
// Entrances (play once): the polish that makes new content feel alive.
inline Mod fade_in(int ms=300){ return animate("wa-fade", ms); }
inline Mod fade_up(int ms=400){ return animate("wa-fade-up", ms); }
inline Mod fade_down(int ms=400){ return animate("wa-fade-down", ms); }
inline Mod slide_in(int ms=400){ return animate("wa-slide-left", ms); }
inline Mod slide_in_left(int ms=400){ return animate("wa-slide-right", ms); }
inline Mod pop_in(int ms=350){ return animate("wa-pop", ms); }
// Loops (infinite): spinners, attention, live indicators.
inline Mod spin(int ms=900){ return sty([=](Style& s){ s.extra.emplace_back("animation",
    "wa-spin " + std::to_string(ms) + "ms linear infinite"); }); }
inline Mod pulse(int ms=1600){ return sty([=](Style& s){ s.extra.emplace_back("animation",
    "wa-pulse " + std::to_string(ms) + "ms ease-in-out infinite"); }); }
inline Mod bounce(int ms=900){ return sty([=](Style& s){ s.extra.emplace_back("animation",
    "wa-bounce " + std::to_string(ms) + "ms ease-in-out infinite"); }); }
inline Mod ping(int ms=1200){ return sty([=](Style& s){ s.extra.emplace_back("animation",
    "wa-ping " + std::to_string(ms) + "ms cubic-bezier(0,0,.2,1) infinite"); }); }
/// `shimmer()` — a moving sheen for skeleton loaders. Sets its own gradient bg.
inline Mod shimmer(std::uint32_t base=0x1e293b, std::uint32_t hi=0x334155, int ms=1400){
    return sty([=](Style& s){
        s.extra.emplace_back("background",
            "linear-gradient(90deg,"+detail::hexstr(base)+" 25%,"+detail::hexstr(hi)+" 37%,"+detail::hexstr(base)+" 63%)");
        s.extra.emplace_back("background-size","200% 100%");
        s.extra.emplace_back("animation","wa-shimmer "+std::to_string(ms)+"ms linear infinite");
    });
}
/// `delay(ms)` — stagger an entrance (pair with fade_up in an each() index).
inline Mod delay(int ms){ return sty([=](Style& s){ s.extra.emplace_back("animation-delay", std::to_string(ms)+"ms"); }); }

// ── elevation & modern surface effects ────────────────────────────────
/// `elevation(3)` — a Material-style depth scale (1–5). Higher = more lifted.
/// Reads as real depth (layered, tinted shadows), not a flat drop-shadow.
inline Mod elevation(int level){
    static const char* E[] = {
        "none",
        "0 1px 2px rgba(0,0,0,.30)",
        "0 2px 4px rgba(0,0,0,.30),0 1px 2px rgba(0,0,0,.25)",
        "0 6px 12px rgba(0,0,0,.32),0 2px 4px rgba(0,0,0,.28)",
        "0 12px 24px rgba(0,0,0,.34),0 4px 8px rgba(0,0,0,.28)",
        "0 24px 48px rgba(0,0,0,.40),0 8px 16px rgba(0,0,0,.30)",
    };
    int i = level<0?0:level>5?5:level;
    return sty([=](Style& s){ s.extra.emplace_back("box-shadow", E[i]); });
}
/// `glow(color)` — a soft coloured halo (brand buttons, active/live states).
inline Mod glow(std::uint32_t color, int spread=24){
    auto h = detail::hexstr(color);
    return sty([=](Style& s){ s.extra.emplace_back("box-shadow",
        "0 0 "+std::to_string(spread)+"px "+h+"66, 0 0 "+std::to_string(spread/2)+"px "+h+"44"); });
}
/// `ring(color, width)` — a focus/selection ring (outline that doesn't shift layout).
inline Mod ring(std::uint32_t color, int width=2){
    return sty([=](Style& s){ s.extra.emplace_back("box-shadow",
        "0 0 0 "+std::to_string(width)+"px "+detail::hexstr(color)); });
}
/// `glass(blur, tint)` — frosted-glass panel: translucent tint + backdrop blur.
/// The modern "floating panel over content" look, in one word.
inline Mod glass(int blur_px=14, std::uint32_t tint=0xffffff, float alpha=0.08f){
    auto h = detail::hexstr(tint);
    char a[3]; std::snprintf(a, sizeof(a), "%02x", (int)(alpha*255));
    return sty([=](Style& s){
        s.extra.emplace_back("background", h + std::string(a));
        s.extra.emplace_back("backdrop-filter", "blur("+std::to_string(blur_px)+"px)");
        s.extra.emplace_back("-webkit-backdrop-filter", "blur("+std::to_string(blur_px)+"px)");
        s.extra.emplace_back("border", "1px solid "+h+"22");
    });
}

// ── typography presets ─ composable text styles (still overridable by | mods) ─
/// The type scale. `display` is the biggest (hero/title); use with `text(...)`.
inline const Mod display  = sty([](Style& s){ s.font_size={32,Unit::px}; s.weight=Weight::black; s.line_height=1.1f; s.has_lh=true; });
inline const Mod heading  = sty([](Style& s){ s.font_size={24,Unit::px}; s.weight=Weight::bold;  s.line_height=1.2f; s.has_lh=true; });
inline const Mod subtitle = sty([](Style& s){ s.font_size={18,Unit::px}; s.weight=Weight::semibold; });
inline const Mod body     = sty([](Style& s){ s.font_size={15,Unit::px}; s.line_height=1.6f; s.has_lh=true; });
inline const Mod caption  = sty([](Style& s){ s.font_size={13,Unit::px}; });
inline const Mod label    = sty([](Style& s){ s.font_size={12,Unit::px}; s.weight=Weight::semibold; s.extra.emplace_back("text-transform","uppercase"); s.extra.emplace_back("letter-spacing",".06em"); });
inline const Mod mono     = sty([](Style& s){ s.extra.emplace_back("font-family","ui-monospace,SFMono-Regular,Menlo,Consolas,monospace"); });

// ── image fit ───────────────────────────────────────────────────────────
inline const Mod cover   = sty([](Style& s){ s.extra.emplace_back("object-fit","cover"); });
inline const Mod contain = sty([](Style& s){ s.extra.emplace_back("object-fit","contain"); });
/// `fit("cover"|"contain"|"fill"|"none"|"scale-down")` — explicit object-fit.
inline Mod fit(std::string how){ return sty([=](Style& s){ s.extra.emplace_back("object-fit", how); }); }

// ── completeness: the everyday properties, named so you never reach for css() ─
// These fill the last gaps that used to force a raw css("…") in real UIs.
/// `w_full` / `w_half` / `w_frac(n,d)` — fractional widths without a Len.
inline const Mod w_full = sty([](Style& s){ s.w={100,Unit::pct}; });
inline const Mod h_full = sty([](Style& s){ s.h={100,Unit::pct}; });
inline const Mod w_half = sty([](Style& s){ s.w={50,Unit::pct}; });
inline const Mod w_screen = sty([](Style& s){ s.extra.emplace_back("width","100%"); });
/// Full-viewport height, mobile-correct: `100dvh` is what "full screen" means
/// on a phone (the `vh` unit overshoots behind the URL bar).
inline const Mod h_screen = sty([](Style& s){ s.extra.emplace_back("min-height","100dvh"); });
inline Mod w_frac(int num, int den){ return sty([=](Style& s){ s.w={100.f*num/den, Unit::pct}; }); }
/// `mx_auto` — centre a fixed/max-width block horizontally (the classic page
/// column: `col(…) | max_w(1200) | mx_auto | w_full`).
inline const Mod mx_auto = sty([](Style& s){ s.extra.emplace_back("margin-left","auto"); s.extra.emplace_back("margin-right","auto"); });
/// `square(px)` / `circle(px)` — equal-sided box; circle also rounds fully.
inline Mod square(float px_){ return sty([=](Style& s){ s.w={px_,Unit::px}; s.h={px_,Unit::px}; }); }
inline Mod circle(float px_){ return sty([=](Style& s){ s.w={px_,Unit::px}; s.h={px_,Unit::px}; s.radius={9999,Unit::px}; }); }
/// text decoration — named, no css(). (`underline` already exists; these add the rest.)
inline const Mod line_through = sty([](Style& s){ s.extra.emplace_back("text-decoration","line-through"); });
inline const Mod no_underline = sty([](Style& s){ s.extra.emplace_back("text-decoration","none"); });
inline const Mod pre_wrap      = sty([](Style& s){ s.extra.emplace_back("white-space","pre-wrap"); });
/// `pre` — preserve whitespace and newlines exactly, no wrapping (ASCII art,
/// terminal output, code). The stricter sibling of `pre_wrap`.
inline const Mod pre           = sty([](Style& s){ s.extra.emplace_back("white-space","pre"); });
inline const Mod break_word    = sty([](Style& s){ s.extra.emplace_back("overflow-wrap","anywhere"); });
/// overflow control, named.
inline const Mod clip_content = sty([](Style& s){ s.extra.emplace_back("overflow","hidden"); });
inline const Mod clip_x = sty([](Style& s){ s.extra.emplace_back("overflow-x","hidden"); });
inline const Mod clip_y = sty([](Style& s){ s.extra.emplace_back("overflow-y","hidden"); });
inline const Mod overflow_visible = sty([](Style& s){ s.extra.emplace_back("overflow","visible"); });
/// interaction / selection.
inline const Mod select_none = sty([](Style& s){ s.extra.emplace_back("user-select","none"); });
inline const Mod select_all  = sty([](Style& s){ s.extra.emplace_back("user-select","all"); });
/// image filters, named (no filter: strings).
inline Mod grayscale(int pct=100){ return sty([=](Style& s){ s.extra.emplace_back("filter","grayscale("+std::to_string(pct)+"%)"); }); }
inline Mod brightness(int pct){ return sty([=](Style& s){ s.extra.emplace_back("filter","brightness("+std::to_string(pct)+"%)"); }); }
inline Mod saturate(int pct){ return sty([=](Style& s){ s.extra.emplace_back("filter","saturate("+std::to_string(pct)+"%)"); }); }
inline Mod contrast(int pct){ return sty([=](Style& s){ s.extra.emplace_back("filter","contrast("+std::to_string(pct)+"%)"); }); }
inline Mod sepia(int pct=100){ return sty([=](Style& s){ s.extra.emplace_back("filter","sepia("+std::to_string(pct)+"%)"); }); }
/// `font_family(stack)` — a named font stack (rarely needed; `mono` covers code).
inline Mod font_family(std::string stack){ return sty([=](Style& s){ s.extra.emplace_back("font-family", stack); }); }

// ── gradients (delightful sugar over the universal channel) ─────────────
inline Mod gradient(std::uint32_t a, std::uint32_t b, int deg=90){
    return sty([=](Style& s){ s.extra.emplace_back("background",
        "linear-gradient("+std::to_string(deg)+"deg,"+detail::hexstr(a)+","+detail::hexstr(b)+")"); }); }
/// `gradient(rgba(…), rgba(…), deg)` — the alpha-aware form (glass sheens,
/// scrims, tinted overlays).
inline Mod gradient(Color a, Color b, int deg=90){
    return sty([av=a.css(), bv=b.css(), deg](Style& s){ s.extra.emplace_back("background",
        "linear-gradient("+std::to_string(deg)+"deg,"+av+","+bv+")"); }); }
/// `orb(inner, outer, cx, cy)` — a hard radial-gradient FILL: the lit-from-a-
/// point look for solid shapes (wheels, knobs, LED dots, vignetted panels).
/// `cx`/`cy` place the highlight as percentages (40,35 = upper-left light).
/// (Contrast `radial(color, x, y)` below: a soft page-scale glow backdrop.)
inline Mod orb(Color inner, Color outer, int cx=50, int cy=50){
    return sty([i=inner.css(), o=outer.css(), cx, cy](Style& s){ s.extra.emplace_back("background",
        "radial-gradient(circle at "+std::to_string(cx)+"% "+std::to_string(cy)+"%, "+i+", "+o+")"); }); }
inline Mod orb(std::uint32_t inner, std::uint32_t outer, int cx=50, int cy=50){ return orb(rgb(inner), rgb(outer), cx, cy); }
/// `veil(opacity)` — a translucent black backdrop (modal dimmers, image
/// scrims). `veil(.55)` ≡ background: rgba(0,0,0,.55).
inline Mod veil(float a=.5f){ return sty([=](Style& s){ s.extra.emplace_back("background", "rgba(0,0,0,"+detail::numstr(a)+")"); }); }
/// gradient text: paint the gradient and clip it to the glyphs.
inline Mod gradient_text(std::uint32_t a, std::uint32_t b, int deg=90){
    return sty([=](Style& s){
        s.extra.emplace_back("background","linear-gradient("+std::to_string(deg)+"deg,"+detail::hexstr(a)+","+detail::hexstr(b)+")");
        s.extra.emplace_back("-webkit-background-clip","text");
        s.extra.emplace_back("background-clip","text");
        s.extra.emplace_back("color","transparent"); }); }

// ── gorgeous backgrounds ─ one mod each, no hand-written css() ──────────────
namespace detail {
/// #rrggbbaa from a colour + 0..1 alpha (for translucent fills).
inline std::string rgba_hex(std::uint32_t c, float a){
    char buf[3]; std::snprintf(buf, sizeof(buf), "%02x", (int)(a*255+0.5f)); return hexstr(c)+buf; }
}
/// `radial(color, x%, y%)` — a soft radial glow bloom over a base, positioned at
/// (x,y). The signature backdrop for hero sections. Fades to transparent. The
/// bloom size is a fraction of the VIEWPORT (vmax), so it fills a small phone
/// and a 4K monitor proportionally instead of staying a fixed rem blob.
inline Mod radial(std::uint32_t color, int x=50, int y=-10, std::uint32_t base=0x090b14, int size=90){
    return sty([=](Style& s){ s.extra.emplace_back("background",
        "radial-gradient("+std::to_string(size)+"vmax "+std::to_string(size-15)+"vmax at "+std::to_string(x)+"% "+std::to_string(y)+"%,"
        +detail::rgba_hex(color,0.18f)+", transparent 60%), "+detail::hexstr(base)); }); }
/// `mesh(a, b, base)` — a two-blob mesh gradient (top-left + top-right), the
/// modern "ambient light" backdrop. Beautiful behind a dark hero. Sized in vmax
/// so it scales with the viewport.
inline Mod mesh(std::uint32_t a, std::uint32_t b, std::uint32_t base=0x090b14){
    return sty([=](Style& s){ s.extra.emplace_back("background",
        "radial-gradient(80vmax 60vmax at 15% -5%,"+detail::rgba_hex(a,0.20f)+", transparent 60%),"
        "radial-gradient(70vmax 55vmax at 90% 10%,"+detail::rgba_hex(b,0.14f)+", transparent 55%), "+detail::hexstr(base)); }); }
/// `gradient_bg(a, b, deg)` — a linear-gradient FILL (alias for gradient, reads
/// clearer when you mean "the background").
inline Mod gradient_bg(std::uint32_t a, std::uint32_t b, int deg=135){ return gradient(a,b,deg); }

// ── flashy backgrounds ─ animated, alive ──────────────────────────────
/// `aurora(a, b, c)` — a slow, drifting three-colour gradient. The living
/// backdrop behind a hero. Set on the page root.
inline Mod aurora(std::uint32_t a, std::uint32_t b, std::uint32_t c, int secs=18){
    return sty([=](Style& s){
        s.extra.emplace_back("background","linear-gradient(120deg,"+detail::hexstr(a)+","+detail::hexstr(b)+","+detail::hexstr(c)+","+detail::hexstr(a)+")");
        s.extra.emplace_back("background-size","300% 300%");
        s.extra.emplace_back("animation","wa-aurora "+std::to_string(secs)+"s ease infinite"); }); }
/// `aurora_text(a, b, c)` — gradient text whose hue slowly rotates. Living headline.
inline Mod aurora_text(std::uint32_t a, std::uint32_t b, std::uint32_t c, int secs=8){
    return sty([=](Style& s){
        s.extra.emplace_back("background","linear-gradient(90deg,"+detail::hexstr(a)+","+detail::hexstr(b)+","+detail::hexstr(c)+")");
        s.extra.emplace_back("background-size","200% auto");
        s.extra.emplace_back("-webkit-background-clip","text");
        s.extra.emplace_back("background-clip","text");
        s.extra.emplace_back("color","transparent");
        s.extra.emplace_back("animation","wa-aurora "+std::to_string(secs)+"s ease infinite"); }); }
/// `glow_text(color, blur)` — a coloured bloom behind text (neon headline).
inline Mod glow_text(std::uint32_t color, int blur=24){
    return sty([=](Style& s){ s.extra.emplace_back("text-shadow",
        "0 0 "+std::to_string(blur)+"px "+detail::rgba_hex(color,0.6f)+", 0 0 "+std::to_string(blur*2)+"px "+detail::rgba_hex(color,0.3f)); }); }
/// `text_glow(color, blur)` — the precise single-layer form: one halo, your
/// alpha (`text_glow(rgba(green, .8f), 6)` = phosphor glyphs, LED digits).
inline Mod text_glow(Color c, float blur_=8){
    return sty([v=c.css(), blur_](Style& s){ s.extra.emplace_back("text-shadow", "0 0 "+detail::numstr(blur_)+"px "+v); }); }
inline Mod text_glow(std::uint32_t c, float blur_=8){ return text_glow(rgba(c, .8f), blur_); }
/// `float_()` — the node gently bobs up and down (hero art, badges).
inline Mod float_(int secs=4){ return sty([=](Style& s){ s.extra.emplace_back("animation","wa-float "+std::to_string(secs)+"s ease-in-out infinite"); }); }
/// `breathe()` — a slow opacity+scale breath (ambient glows, live dots).
inline Mod breathe(int secs=3){ return sty([=](Style& s){ s.extra.emplace_back("animation","wa-breathe "+std::to_string(secs)+"s ease-in-out infinite"); }); }
/// `gradient_border(a, b, width)` — the popular glowing-edge card: a gradient
/// ring drawn with a mask so only the border shows. width in px.
inline Mod gradient_border(std::uint32_t a, std::uint32_t b, int width=1, int deg=135){
    return sty([=](Style& s){
        s.extra.emplace_back("border", std::to_string(width)+"px solid transparent");
        s.extra.emplace_back("background",
            "linear-gradient(var(--wa-surface,#141b2e),var(--wa-surface,#141b2e)) padding-box,"
            "linear-gradient("+std::to_string(deg)+"deg,"+detail::hexstr(a)+","+detail::hexstr(b)+") border-box"); }); }

// ── translucent surfaces ─ the frosted-panel look, without rgba() by hand ────
/// `tint(color, alpha)` — a translucent fill (default: faint white, .04). The
/// "raised surface over a dark bg" look used on every card.
inline Mod tint(std::uint32_t color=0xffffff, float alpha=0.04f){
    return sty([=](Style& s){ s.extra.emplace_back("background", detail::rgba_hex(color, alpha)); }); }
/// `hairline(color, alpha)` — a 1px translucent border (default faint white .10).
inline Mod hairline(std::uint32_t color=0xffffff, float alpha=0.10f){
    return sty([=](Style& s){ s.extra.emplace_back("border", "1px solid "+detail::rgba_hex(color, alpha)); }); }
/// `frost(blur)` — tint + hairline + backdrop blur in one: an instant glass card.
inline Mod frost(int blur_px=14, float alpha=0.05f){
    return sty([=](Style& s){
        s.extra.emplace_back("background", detail::rgba_hex(0xffffff, alpha));
        s.extra.emplace_back("border", "1px solid "+detail::rgba_hex(0xffffff, 0.12f));
        s.extra.emplace_back("backdrop-filter", "blur("+std::to_string(blur_px)+"px)");
        s.extra.emplace_back("-webkit-backdrop-filter", "blur("+std::to_string(blur_px)+"px)"); }); }
/// `drop_shadow(color, blur, alpha)` — a coloured glow around a node's SHAPE
/// (respects transparency and SVG paths, unlike box-shadow). Great for art/icons.
inline Mod drop_shadow(std::uint32_t color, int blur_=24, float alpha=0.25f){ return sty([=](Style& s){
    s.extra.emplace_back("filter", "drop-shadow(0 0 "+std::to_string(blur_)+"px "+detail::rgba_hex(color, alpha)+")"); }); }

// ── the universal channel — reach ANY css, nothing off-limits ────────────
// The escape hatches. They keep the vocabulary open-ended — but a team that
// wants to GUARANTEE its UI is built purely from the named vocabulary (never a
// hand-written property or HTML attribute) can build with -DWAYA_NO_RAW_CSS,
// which turns every use below into a COMPILE ERROR. That makes "you never touch
// HTML/CSS" an enforceable invariant, not just a convention: if it compiles, no
// raw web strings leaked in. (Named mods still cover the common 90%+; if you hit
// a genuine gap, add a named mod for it rather than reopening the hatch.)
#if defined(WAYA_NO_RAW_CSS)
template <typename... A>
[[deprecated("WAYA_NO_RAW_CSS: raw css() is disabled — use a named style mod (fg/pad/round/gradient/…) or add one")]]
Mod css(A...){ static_assert(sizeof...(A)==999, "waya: css() is disabled under WAYA_NO_RAW_CSS — use a named mod"); return {}; }
template <typename... A>
[[deprecated("WAYA_NO_RAW_CSS: var() is disabled — use a named mod")]]
Mod var(A...){ static_assert(sizeof...(A)==999, "waya: var() is disabled under WAYA_NO_RAW_CSS"); return {}; }
#else
inline Mod css(std::string prop, std::string value){ return sty([=](Style& s){ s.extra.emplace_back(prop, value); }); }
inline Mod var(std::string name, std::string value){ return sty([=](Style& s){ s.extra.emplace_back("--"+name, value); }); }
#endif
// Library-internal raw-style helper: named mods (hover_lift, press, …) build on
// this so they keep working even under WAYA_NO_RAW_CSS — the user called a NAMED
// mod, not css(), so no abstraction leaked. Not part of the public vocabulary.
namespace detail {
inline Mod raw_css(std::string prop, std::string value){ return sty([=](Style& s){ s.extra.emplace_back(prop, value); }); }
}

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
/// Below a breakpoint (max-width) — for phone-first overrides. 1px under the
/// breakpoint so `at(Md,..)` and `below(Md,..)` never both apply.
inline std::string below_q(Break b){ switch(b){ case Break::Sm:return "@media(max-width:639.98px)"; case Break::Md:return "@media(max-width:767.98px)";
    case Break::Lg:return "@media(max-width:1023.98px)"; case Break::Xl:return "@media(max-width:1279.98px)"; } return ""; }
/// Build a sub-Style by applying style-only Mods to a fresh node's style.
template <typename... M> std::shared_ptr<Style> sub_style(M... mods){
    Node tmp; (mods.apply(tmp), ...); return std::make_shared<Style>(tmp.style); }
}
/// `on(Hover, bg(...), scale(1.02f))` — a style overlay for a state. A Mod.
template <typename... M> Mod on(State st, M... mods){
    auto sub = detail::sub_style(mods...);
    return sty([=](Style& s){ s.states.emplace_back(detail::state_sel(st), sub); });
}
/// `at(Md, w(fill), pad(24))` — apply mods AT AND ABOVE a breakpoint (min-width;
/// mobile-first: your base styles are the phone, `at` scales up).
template <typename... M> Mod at(Break b, M... mods){
    auto sub = detail::sub_style(mods...);
    return sty([=](Style& s){ s.states.emplace_back(detail::break_q(b), sub); });
}
/// `below(Md, col, gap(8))` — apply mods BELOW a breakpoint (max-width). The
/// direct "on phones/tablets, do X" override. Pairs with `at` for either end.
template <typename... M> Mod below(Break b, M... mods){
    auto sub = detail::sub_style(mods...);
    return sty([=](Style& s){ s.states.emplace_back(detail::below_q(b), sub); });
}

// ── mobile-first convenience ──────────────────────────────────────
/// `on_phone(col, gap(8))` — overrides that apply on phone widths (< 768px).
template <typename... M> Mod on_phone(M... mods){ return below(Break::Md, mods...); }
/// `on_desktop(row, gap(24))` — overrides from tablet/desktop up (≥ 768px).
template <typename... M> Mod on_desktop(M... mods){ return at(Break::Md, mods...); }

// ── responsive visibility ──────────────────────────────────────
inline const Mod hidden = sty([](Style& s){ s.extra.emplace_back("display","none"); });
/// `hide_below(Md)` — not shown below the breakpoint (desktop-only element).
inline Mod hide_below(Break b){ return below(b, hidden); }
/// `hide_above(Md)` — not shown at/above the breakpoint (mobile-only element).
inline Mod hide_above(Break b){ return at(b, hidden); }
/// `only_phone()` / `only_desktop()` — show exclusively on that class of device.
inline Mod only_phone(){ return hide_above(Break::Md); }
inline Mod only_desktop(){ return hide_below(Break::Md); }

// ── interaction polish ─ the gestures every pretty UI uses, one mod each ────
/// `hover_lift(px)` — the node eases UP on hover (the universal "this is
/// interactive" cue). Bundles the transition + the :hover transform.
inline Mod hover_lift(float px_=3){
    return transition("transform .18s cubic-bezier(.2,.7,.2,1), box-shadow .18s ease")
         | on(Hover, detail::raw_css("transform", "translateY(-"+std::to_string((int)px_)+"px)"));
}
/// `press()` — the node scales down slightly while held (tactile button feel).
inline Mod press(float to=0.96f){
    return on(Active, detail::raw_css("transform", "scale("+detail::numstr(to)+")"));
}
/// `hover_glow(color)` — a coloured halo blooms on hover (brand buttons, cards).
inline Mod hover_glow(std::uint32_t color, int spread=26){
    auto h = detail::hexstr(color);
    return transition("box-shadow .22s ease, transform .18s ease")
         | on(Hover, detail::raw_css("box-shadow", "0 0 "+std::to_string(spread)+"px "+h+"66"));
}
/// `hover_bg(color, alpha)` — fill lightens on hover (ghost buttons, list rows).
inline Mod hover_bg(std::uint32_t color=0xffffff, float alpha=0.08f){
    return transition("background-color .15s ease")
         | on(Hover, detail::raw_css("background", detail::rgba_hex(color, alpha)));
}
/// `interactive()` — the everyday combo: pointer cursor + lift + press.
inline Mod interactive(){ return pointer | hover_lift(2) | press(); }

// ── interactivity & identity — Mods that touch the node, not just its style ─
// TYPED messages: `tap(Inc{})`, `on_input([](std::string v){ return SetName{v}; })`.
// The app's Msg is its own variant (maya/Elm), NOT an int. At build time each
// handler is registered in the per-render table and an opaque wire TOKEN is
// stored on the node; the runtime resolves the token back to the real Msg on
// the round-trip. The int in the node is a private wire detail, never authored.
template <typename Msg> Mod tap(Msg m){ int tok = detail::register_msg<Msg>(std::move(m)); return {[=](Node& n){ n.on_tap=tok; }}; }
/// `on_input(fn)` — fn maps the field's live text to a Msg: on_input([](std::string v){ return SetName{v}; }).
template <typename Fn> requires (!std::is_integral_v<Fn> && !std::is_enum_v<Fn>)
Mod on_input(Fn fn){
    using Msg = std::invoke_result_t<Fn, std::string>;
    int tok = detail::register_mapper<Msg>(std::function<Msg(std::string)>(std::move(fn)));
    return {[=](Node& n){ n.on_input=tok; }};
}
/// int/enum overload: the field's value rides to update() as the 3rd arg.
template <typename E> requires (std::is_integral_v<E> || std::is_enum_v<E>)
Mod on_input(E msg){ int tok = detail::register_msg<E>(msg); return {[=](Node& n){ n.on_input=tok; }}; }
template <typename Fn> requires (!std::is_integral_v<Fn> && !std::is_enum_v<Fn>)
Mod on_change(Fn fn){
    using Msg = std::invoke_result_t<Fn, std::string>;
    int tok = detail::register_mapper<Msg>(std::function<Msg(std::string)>(std::move(fn)));
    return {[=](Node& n){ n.on_change=tok; }};
}
template <typename E> requires (std::is_integral_v<E> || std::is_enum_v<E>)
Mod on_change(E msg){ int tok = detail::register_msg<E>(msg); return {[=](Node& n){ n.on_change=tok; }}; }
inline Mod placeholder(std::string p){ return {[=](Node& n){ n.placeholder=p; }}; }
inline Mod type(std::string t){ return {[=](Node& n){ n.input_type=t; }}; }
inline Mod name(std::string nm){ return {[=](Node& n){ n.name=nm; }}; }
inline Mod checked(bool on=true){ return {[=](Node& n){ n.checked=on; }}; }
inline Mod disabled(bool on=true){ return {[=](Node& n){ n.disabled=on; }}; }
inline Mod key(std::string k){ return {[=](Node& n){ n.key=k; }}; }
/// `animated()` — mark a keyed list item so the browser smoothly ANIMATES its
/// motion: it slides from its old position to its new one on reorder (FLIP), and
/// fades+rises in when first inserted. Put it on each item of a keyed list
/// alongside `key(...)`; the client tracks it by that key. Beautiful reordering,
/// insertion, and filtering with zero extra state.
///   each_keyed(items, key_of, [](auto& it){ return row(...) | key(it.id) | animated(); })
inline Mod animated(){ return {[](Node& n){
    // use the node's own key as the FLIP identity (falls back to a stable-ish id)
    n.attrs.emplace_back("data-wa-flip", n.key.empty()? std::string("_") : n.key); }}; }

// ── the general event mod — wire any DOM event to a Msg ───────────────────
/// `on("pointerenter", Show{})` — the escape hatch: any DOM event name → Msg. For
/// value-carrying events (keydown/drop) the event's value is IGNORED here (fixed
/// Msg); use on_keydown/on_drop with a mapper to read it. `arg` narrows (a key).
template <typename Msg> Mod on(std::string event, Msg m, std::string arg={}){
    int tok = detail::register_msg<Msg>(std::move(m));
    return {[=](Node& n){ n.events.push_back({event, tok, arg}); }};
}
/// value-carrying general event: fn maps the event value to a Msg.
template <typename Fn> requires (!std::is_integral_v<Fn> && !std::is_enum_v<Fn>)
Mod on_ev(std::string event, Fn fn, std::string arg={}){
    using Msg = std::invoke_result_t<Fn, std::string>;
    int tok = detail::register_mapper<Msg>(std::function<Msg(std::string)>(std::move(fn)));
    return {[=](Node& n){ n.events.push_back({event, tok, arg}); }};
}
template <typename E> requires (std::is_integral_v<E> || std::is_enum_v<E>)
Mod on_ev(std::string event, E msg, std::string arg={}){
    int tok = detail::register_msg<E>(msg);
    return {[=](Node& n){ n.events.push_back({event, tok, arg}); }};
}

// ── keyboard ────────────────────────────────────────────────────
/// `on_key("Enter", Submit{})` — fire only when that key is pressed while focused.
template <typename Msg> Mod on_key(std::string k, Msg m){ return on("keydown", std::move(m), std::move(k)); }
template <typename Msg> Mod on_enter(Msg m){ return on_key("Enter", std::move(m)); }
template <typename Msg> Mod on_escape(Msg m){ return on_key("Escape", std::move(m)); }
/// `on_keydown(fn)` — any key; the pressed key name is mapped to a Msg by fn.
template <typename Fn> Mod on_keydown(Fn fn){ return on_ev("keydown", std::move(fn)); }
/// `on_shortcut("mod+k", Open{})` — a GLOBAL keyboard shortcut that fires from
/// anywhere on the page (no focus needed). `mod` = Cmd on macOS / Ctrl elsewhere;
/// combos like "ctrl+shift+p", "alt+ArrowDown". The Cmd+K command-palette move.
/// Put it on any node that's mounted while the shortcut should be live.
template <typename Msg> Mod on_shortcut(std::string combo, Msg m){ return on("shortcut", std::move(m), std::move(combo)); }
/// `hotkey("?", ShowHelp{})` — alias for a single-key global shortcut.
template <typename Msg> Mod hotkey(std::string k, Msg m){ return on("shortcut", std::move(m), std::move(k)); }

// ── focus ─────────────────────────────────────────────────────
template <typename Msg> Mod on_focus(Msg m){ return on("focus", std::move(m)); }
template <typename Msg> Mod on_blur(Msg m){ return on("blur", std::move(m)); }

// ── pointer / hover as EVENTS (distinct from :hover styling) ────────────────
template <typename Msg> Mod on_enter_pointer(Msg m){ return on("pointerenter", std::move(m)); }
template <typename Msg> Mod on_leave_pointer(Msg m){ return on("pointerleave", std::move(m)); }
/// `on_hover(Enter{}, Leave{})` — the common pair (tooltips, previews).
template <typename Msg> Mod on_hover(Msg enter, Msg leave){
    return on("pointerenter", std::move(enter)) | on("pointerleave", std::move(leave));
}

// ── forms ─────────────────────────────────────────────────────────────
/// `on_submit(fn)` on a `form(...)` — fn maps the gathered "a=1&b=2" field string
/// to a Msg. Fires on Enter or a submit button.
template <typename Fn> Mod on_submit(Fn fn){ return on_ev("submit", std::move(fn)); }

/// The parsed fields of a submitted form. `on_submit` hands your `fn` the raw
/// "a=1&b=2" (URL-encoded) string; `FormData::parse(s)` turns it into a keyed
/// lookup so you read fields by name instead of hand-parsing:
///
///   on_submit([](std::string body){
///       auto f = FormData::parse(body);
///       return SignUp{ f.get("email"), f.get("password") };
///   })
struct FormData {
    std::vector<std::pair<std::string,std::string>> fields;

    static std::string url_decode(std::string_view s){
        std::string o; o.reserve(s.size());
        auto hex = [](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10;
                                    if(c>='A'&&c<='F'){return c-'A'+10;} return 0; };
        for(std::size_t i=0;i<s.size();++i){
            char c=s[i];
            if(c=='+') o+=' ';
            else if(c=='%' && i+2<s.size()){ o+=(char)(hex(s[i+1])*16+hex(s[i+2])); i+=2; }
            else o+=c;
        }
        return o;
    }
    static FormData parse(std::string_view body){
        FormData d; std::size_t i=0;
        while(i<body.size()){
            std::size_t amp=body.find('&', i); if(amp==std::string_view::npos) amp=body.size();
            std::string_view pair=body.substr(i, amp-i);
            std::size_t eq=pair.find('=');
            std::string k=url_decode(eq==std::string_view::npos?pair:pair.substr(0,eq));
            std::string v=eq==std::string_view::npos?std::string{}:url_decode(pair.substr(eq+1));
            if(!k.empty()) d.fields.emplace_back(std::move(k), std::move(v));
            i=amp+1;
        }
        return d;
    }
    /// Value for `key` (first match), or `fallback` if absent.
    std::string get(std::string_view key, std::string fallback={}) const {
        for(auto&[k,v]:fields) { if(k==key) return v; }
        return fallback;
    }
    bool has(std::string_view key) const { for(auto&[k,v]:fields){(void)v; if(k==key) return true;} return false; }
    /// A checkbox reads as present/"on"/"true" → true.
    bool checked(std::string_view key) const { auto v=get(key); return v=="on"||v=="true"||v=="1"; }
};

// ── drag & drop ───────────────────────────────────────────────────────────────────────────────
/// Mark a node draggable and give it a payload id (rides as the drag data).
inline Mod draggable(std::string payload={}){ return {[=](Node& n){ n.draggable=true; if(!payload.empty()) n.name=payload; }}; }
/// `on_drop(fn)` on a drop target — fn maps the dropped payload to a Msg.
template <typename Fn> Mod on_drop(Fn fn){ return on_ev("drop", std::move(fn)); }

// ── appear (lazy load / infinite scroll) ────────────────────────────
/// `on_appear(LoadMore{})` — fire ONCE when this node first scrolls into view.
/// An element becoming visible is a user event (they scrolled to it), exactly
/// like a keypress — so it's a Msg, not client code. The canonical patterns:
///
///   // infinite scroll: a sentinel row at the end of the list
///   rows.push_back(box() | h(1) | on_appear(LoadMore{}));
///
///   // lazy section: don't compute the heavy chart until it's seen
///   m.chart_seen ? heavy_chart(m) : box() | h(240) | on_appear(ChartSeen{})
///
/// Fires once per DOM lifetime (the client unobserves after firing); if the
/// diff replaces the node, a fresh element re-arms — which is exactly right
/// for "next page" sentinels that move.
template <typename Msg> Mod on_appear(Msg m){ return on("appear", std::move(m)); }
/// `drop_arg(id)` — tag a drop target with WHERE it is (e.g. a column id). The
/// client delivers `"<dragged-payload>:<id>"` to this node's on_drop mapper, so
/// the app learns both what was dropped AND where. Pair with `on_drop(fn)`.
inline Mod drop_arg(std::string id){ return {[=](Node& n){ n.attrs.emplace_back("data-drop-arg", id); }}; }
/// `drop_target(id, fn)` — the one-liner: mark a node a drop zone tagged `id`,
/// with `fn` mapping the delivered `"<payload>:<id>"` string to a Msg. Parse the
/// two halves in your handler (payload before the last ':', target id after).
template <typename Fn> Mod drop_target(std::string id, Fn fn){
    return {[id, fn = std::move(fn)](Node& n){
        on_drop(fn).apply(n);
        n.attrs.emplace_back("data-drop-arg", id);
    }};
}

// ── file upload ─────────────────────────────────────────────────────────────────────────
/// A file the user picked, delivered to your update as decoded BYTES — no
/// multipart, no endpoints, no JS. The client reads the file, ships it over the
/// live socket, and `FileData::parse` gives you name/mime/content:
///
///   file_input(on_file([](FileData f){ return Import{f.name, f.content}; }))
///
/// Size: the client caps a file at 8 MB raw (the frame stays under the WS
/// limit). For bigger uploads, serve a dedicated endpoint — this is the
/// zero-ceremony path for avatars, CSV imports, config files.
struct FileData {
    std::string name;      ///< the picked file's name ("report.csv")
    std::string mime;      ///< its MIME type ("text/csv"; may be a guess)
    std::string content;   ///< the DECODED bytes, ready to use

    /// Decode standard base64 (the client never emits whitespace or url-safe).
    static std::string b64decode(std::string_view s){
        auto val=[](char c)->int{ if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26;
            if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; return -1; };
        std::string o; o.reserve(s.size()*3/4);
        int buf=0, bits=0;
        for(char c : s){ if(c=='=')break; int v=val(c); if(v<0)continue;
            buf=(buf<<6)|v; bits+=6; if(bits>=8){ bits-=8; o+=(char)((buf>>bits)&0xFF); } }
        return o;
    }
    /// Parse the wire payload "<name>|<mime>|<base64>" a file frame carries.
    static FileData parse(std::string_view raw){
        FileData f;
        auto b1=raw.find('|'); if(b1==std::string_view::npos){ f.content=b64decode(raw); return f; }
        auto b2=raw.find('|', b1+1); if(b2==std::string_view::npos){ f.name=std::string(raw.substr(0,b1)); f.content=b64decode(raw.substr(b1+1)); return f; }
        f.name=std::string(raw.substr(0,b1));
        f.mime=std::string(raw.substr(b1+1,b2-b1-1));
        f.content=b64decode(raw.substr(b2+1));
        return f;
    }
};
/// `on_file(fn)` — on a `file_input()`: fn maps the picked FileData to a Msg.
/// (Multiple selected files each fire fn once.)
template <typename Fn> Mod on_file(Fn fn){
    using Msg = std::invoke_result_t<Fn, FileData>;
    return on_ev("file", [f=std::move(fn)](std::string raw) -> Msg {
        return f(FileData::parse(raw));
    });
}
/// `on_paste_file(fn)` — fn maps a PASTED image/file to a Msg. When the user
/// pastes (Cmd/Ctrl+V) onto this node and the clipboard holds a file (a
/// screenshot, an image copied from a page), the client reads it and delivers a
/// FileData — the same bytes-in-your-update path as `on_file`, no upload dialog.
/// A plain-text paste is ignored here (use `on_paste(Msg)` for text). The node
/// must be focusable (an input/textarea, or `| focusable()` on a box).
template <typename Fn> Mod on_paste_file(Fn fn){
    using Msg = std::invoke_result_t<Fn, FileData>;
    return on_ev("pastefile", [f=std::move(fn)](std::string raw) -> Msg {
        return f(FileData::parse(raw));
    });
}
/// `accept(".csv,.json")` / `accept("image/*")` — restrict the file picker.
inline Mod accept(std::string a){ return {[a=std::move(a)](Node& n){ n.attrs.emplace_back("accept", a); }}; }
/// `multiple()` — let the picker select several files (each delivers a Msg).
inline Mod multiple(){ return {[](Node& n){ n.attrs.emplace_back("multiple", ""); }}; }
/// `file_input(on_file(…))` — a real file picker wired to your update.
/// (An unwired positional overload `file_input(bool, accept)` lives in forms.hpp.)
template <typename... M> requires (std::is_same_v<std::remove_cvref_t<M>, Mod> && ...)
NodeRef file_input(M... mods){
    auto n = detail::new_node(); n->kind=Kind::input; n->input_type="file";
    (mods.apply(*n), ...); finalize(*n); return n;
}

// ── arbitrary attributes & accessibility ──────────────────────────────
/// `attr("title","Save")` — set ANY HTML attribute. The escape hatch for the
/// attribute channel, the way `css()` is for the style channel.
inline Mod attr(std::string name, std::string value){ return {[=](Node& n){ n.attrs.emplace_back(name, value); }}; }
/// `anchor("pricing")` — give a node a stable id: a `#pricing` deep-link
/// target, and the target name for `Cmd::scroll_to("pricing")` /
/// `Cmd::focus("pricing")`.
inline Mod anchor(std::string id_){ return attr("id", std::move(id_)); }
/// `optimistic()` — pair with tap: the element shows an instant busy state (dim +
/// wait cursor + click-disabled) the MOMENT it's clicked, before the server
/// responds, cleared on the next paint. Makes an action feel instant on a slow
/// link. Use on a submit/save button: `button("Save") | tap(Save{}) | optimistic()`.
inline Mod optimistic(){ return attr("data-opt", "1"); }
/// `role("dialog")`, `aria("label","Close")` — accessibility, first-class.
inline Mod role(std::string r){ return attr("role", std::move(r)); }
inline Mod aria(std::string k, std::string v){ return attr("aria-" + k, std::move(v)); }
/// `aria_label("Close menu")` — the accessible name for an icon-only control.
inline Mod aria_label(std::string l){ return attr("aria-label", std::move(l)); }
/// `dialog(modal=true)` — the composite a11y contract for a dialog/modal: it
/// sets `role=dialog`, `aria-modal`, a `tabindex=-1` so it can hold programmatic
/// focus, AND `data-modal` — the hook the client uses to make the layer
/// underneath inert (clicks + Tab trapped). One mod for the whole dance that
/// every modal previously hand-wrote as four `attr(…)` calls.
inline Mod dialog(bool modal=true){ return {[=](Node& n){
    n.attrs.emplace_back("role", "dialog");
    n.attrs.emplace_back("aria-modal", modal ? "true" : "false");
    n.attrs.emplace_back("tabindex", "-1");
    if (modal) n.attrs.emplace_back("data-modal", "1");
}}; }
/// `aria_expanded(open)` / `aria_pressed(on)` / `aria_selected(on)` — the state
/// of a disclosure, a toggle button, a tab/option. Screen readers announce
/// “collapsed”→”expanded”, “not pressed”→”pressed” as your Model flips them.
inline Mod aria_expanded(bool open){ return attr("aria-expanded", open ? "true" : "false"); }
inline Mod aria_pressed(bool on){ return attr("aria-pressed", on ? "true" : "false"); }
inline Mod aria_selected(bool on){ return attr("aria-selected", on ? "true" : "false"); }
/// `aria_current("page")` — mark the active item in a set (the current nav link,
/// step, or page). Common values: "page", "step", "true".
inline Mod aria_current(std::string what="page"){ return attr("aria-current", std::move(what)); }
/// `aria_hidden` — hide a purely-decorative node from assistive tech (an icon
/// beside a text label, an ambient background). Different from `sr_only` (which
/// hides from SIGHT but keeps it for readers); this is the opposite.
inline const Mod aria_hidden = {[](Node& n){ n.attrs.emplace_back("aria-hidden", "true"); }};
/// `sr_only()` — visually hidden but read by screen readers (skip links, icon
/// button labels, live-region status). The a11y hallmark, one word.
inline const Mod sr_only = sty([](Style& s){
    s.extra.emplace_back("position","absolute"); s.extra.emplace_back("width","1px");
    s.extra.emplace_back("height","1px"); s.extra.emplace_back("padding","0");
    s.extra.emplace_back("margin","-1px"); s.extra.emplace_back("overflow","hidden");
    s.extra.emplace_back("clip","rect(0,0,0,0)"); s.extra.emplace_back("white-space","nowrap");
    s.extra.emplace_back("border","0"); });
/// `live_region(assertive)` — mark a container whose text changes should be
/// ANNOUNCED by a screen reader when they update. This is the a11y counterpart
/// to waya's whole premise: the server streams a delta and the DOM changes
/// silently, so without this a blind user never hears "3 items added", "saved",
/// "error: email taken". `polite` (default) waits for a pause; `assertive`
/// interrupts (use sparingly, for errors/alerts). Put it on a box that holds
/// status text and update that text from your Model.
inline Mod live_region(bool assertive=false){
    return {[=](Node& n){
        n.attrs.emplace_back("aria-live", assertive ? "assertive" : "polite");
        n.attrs.emplace_back("aria-atomic", "true");   // read the whole region, not just the diff
    }};
}
/// `status(text)` — a ready-made polite live region for transient status
/// ("Saved", "Copied", "Loading…"). Renders visibly; pair with `sr_only` to make
/// it screen-reader-only. `role=status` implies aria-live=polite.
inline NodeRef status(std::string t){
    auto n = detail::new_node(); n->kind=Kind::text; n->text=std::move(t);
    n->attrs.emplace_back("role", "status");
    n->attrs.emplace_back("aria-live", "polite");
    n->attrs.emplace_back("aria-atomic", "true");
    finalize(*n); return n;
}
/// `alert(text)` — an assertive live region for errors/warnings that must be
/// heard immediately. `role=alert` implies aria-live=assertive.
inline NodeRef alert(std::string t){
    auto n = detail::new_node(); n->kind=Kind::text; n->text=std::move(t);
    n->attrs.emplace_back("role", "alert");
    n->attrs.emplace_back("aria-live", "assertive");
    n->attrs.emplace_back("aria-atomic", "true");
    finalize(*n); return n;
}
/// `autofocus()` — focus this control when it mounts (a search box, the first
/// field of a form, a command palette input).
inline Mod autofocus(){ return attr("autofocus", ""); }
/// `focus_ring(color)` — a clean keyboard-focus ring via :focus-visible (only
/// shows for keyboard nav, not mouse clicks — the modern, non-annoying default).
inline Mod focus_ring(std::uint32_t color=0x6366f1){
    return on(State::Focus, sty([=](Style& s){
        s.extra.emplace_back("outline", "2px solid " + detail::hexstr(color));
        s.extra.emplace_back("outline-offset", "2px"); })); }
/// `focus_within(...)` — style a CONTAINER while any descendant is focused (a
/// form field group highlighting when its input has focus).
template <typename... M> Mod focus_within(M... mods){
    auto st = detail::sub_style(mods...);
    return {[st](Node& n){ n.style.states.emplace_back(":focus-within", st); }}; }
/// `group()` on a parent + `group_hover(...)` on a descendant — reveal/restyle a
/// child when the PARENT is hovered (a card whose action buttons appear on
/// hover, a row whose delete icon shows on hover). Registers one global rule.
inline Mod group(){
    assets().css(".wa-group:hover [data-wa-gh]{opacity:1;transform:none;pointer-events:auto}");
    return attr("class", "wa-group"); }
inline Mod group_hidden(){   // the child: hidden until the group is hovered
    return {[](Node& n){ n.attrs.emplace_back("data-wa-gh", "");
        n.style.extra.emplace_back("opacity","0");
        n.style.extra.emplace_back("transform","translateY(4px)");
        n.style.extra.emplace_back("pointer-events","none");
        n.style.extra.emplace_back("transition","opacity .15s ease, transform .15s ease"); }}; }
/// `ripple(color)` — a material-style click ripple emanating from the pointer.
/// Pure CSS+one tiny client hook keyed on a marker; needs no state in your Model.
inline Mod ripple(std::uint32_t color=0xffffff){
    assets().css("[data-wa-ripple]{position:relative;overflow:hidden}"
        "@keyframes wa-ripple{to{transform:scale(4);opacity:0}}"
        ".wa-ripple-ink{position:absolute;border-radius:50%;transform:scale(0);animation:wa-ripple .6s linear;pointer-events:none}");
    return {[color](Node& n){ n.attrs.emplace_back("data-wa-ripple", "");
        char b[8]; std::snprintf(b,sizeof(b),"#%06x",color&0xFFFFFF);
        n.attrs.emplace_back("data-wa-ripple-color", b); }}; }
/// `tap_pop()` — instant client-side press feedback for a tap target. On
/// pointerdown the element plays a tiny scale "pop" (down-then-back) RIGHT AWAY,
/// with zero Model round-trip — so an action feels immediate even when the
/// server is a network hop away and the real result paints a moment later. This
/// is the perceptual counterpart to `optimistic()` (which dims + disables): use
/// `tap_pop()` on lively targets (game pieces, toggles, cards) where you want
/// snappy tactile response rather than a busy state. Pure CSS + one delegated
/// pointerdown hook keyed on a marker; needs no state in your Model.
inline Mod tap_pop(){
    assets().css("@keyframes wa-tap-pop{0%{transform:scale(1)}40%{transform:scale(.86)}100%{transform:scale(1)}}"
        ".wa-tap-pop-go{animation:wa-tap-pop .18s cubic-bezier(.2,.7,.2,1)}");
    return {[](Node& n){ n.attrs.emplace_back("data-wa-pop", ""); }}; }
inline Mod title(std::string t){ return attr("title", std::move(t)); }
inline Mod alt(std::string a){ return attr("alt", std::move(a)); }
/// `safe_url(s)` — neutralise a dangerous URL scheme. `javascript:`, `data:`,
/// and `vbscript:` in an href/src are a script-injection vector even after HTML
/// escaping; this returns "#" for those so a user-supplied link can't run code.
inline std::string safe_url(std::string u){
    std::string lc; for(char c: u){ if(c=='\t'||c=='\n'||c=='\r'||c==' ') continue; lc += (char)((c>='A'&&c<='Z')?c+32:c); }
    auto starts = [&](const char* p){ std::size_t i=0; for(; p[i]; ++i){ if(i>=lc.size()||lc[i]!=p[i]) return false; } return true; };
    if(starts("javascript:") || starts("data:") || starts("vbscript:")) return "#";
    return u;
}
/// `href(url)` — set a link target, URL-scheme-sanitised. Use this for ANY href
/// built from data you don't fully control; it's the safe default over
/// `attr("href", …)`. Renders the node as an <a> if it isn't already.
inline Mod href(std::string url){ return {[u=safe_url(std::move(url))](Node& n){
    if(n.tag.empty() || n.tag=="span") { n.tag="a"; }
    n.attrs.emplace_back("href", u); }}; }
/// `link_to(label, url)` — a real, safe anchor in one call.
inline NodeRef link_to(std::string label, std::string url){
    auto n = detail::new_node(); n->kind=Kind::text; n->text=std::move(label);
    n->tag="a"; n->attrs.emplace_back("href", safe_url(std::move(url))); finalize(*n); return n;
}
/// `tab_index(0)` — make any node keyboard-focusable (so on_key works on it).
inline Mod tab_index(int i){ return attr("tabindex", std::to_string(i)); }
/// `focusable()` — sugar for tabindex 0 (a div that can receive keyboard focus).
inline Mod focusable(){ return tab_index(0); }
/// `stop()` — clicks inside this node do NOT bubble to an ancestor's tap. Put it
/// on a modal/menu PANEL so clicking the content doesn't trigger the backdrop's
/// close-on-tap. Pairs with `overlay(...) | tap(Close)`.
inline Mod stop(){ return attr("data-stop", "1"); }

// ── semantic HTML ─ SEO + accessibility: render as a real element, not a div ─
/// `as("main")` — render this box/text as a specific HTML element. Search engines
/// and screen readers weight semantic tags (main/nav/header/article/h1…) heavily,
/// so a landmark box should say what it IS. Layout/behaviour are unchanged.
inline Mod as(std::string html_tag){ return {[=](Node& n){ n.tag=html_tag; }}; }
// Landmark containers (put on a box):
inline const Mod as_main    = as("main");
inline const Mod as_nav     = as("nav");
inline const Mod as_header  = as("header");
inline const Mod as_footer  = as("footer");
inline const Mod as_article = as("article");
inline const Mod as_section = as("section");
inline const Mod as_aside   = as("aside");
/// `heading(1)`…`heading(6)` — render a text as an <h1>…<h6> (document outline).
inline Mod heading_level(int level){ int l = level<1?1:level>6?6:level; return as("h"+std::to_string(l)); }
/// `as_p()` — a text as a real <p> paragraph.
inline const Mod as_p = as("p");

/// `jsonld(schema)` — not a node mod: build a JSON-LD string for Meta.json_ld.
/// (kept here for discoverability; assign the result to your Meta.)
inline std::string jsonld(std::string type, std::vector<std::pair<std::string,std::string>> fields){
    std::string o = "{\"@context\":\"https://schema.org\",\"@type\":\"" + type + "\"";
    for(auto&[k,v]:fields){ o+=",\""+k+"\":\""; for(char c:v){ if(c=='"'||c=='\\')o+='\\'; o+=c; } o+="\""; }
    o += "}"; return o;
}

} // namespace waya::surface
