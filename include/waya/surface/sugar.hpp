#pragma once
/// \file sugar.hpp
/// Delightful conveniences over the surface vocabulary. Optional \u2014 everything
/// here is just shorthand for things you can already write. Include it for a
/// nicer authoring experience: named colours, a spacing scale, and the `when` /
/// `each` combinators for conditional and list content.

#include "node.hpp"
#include "router.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
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

// Theme tokens — semantic colours that flow through CSS variables.
// A design-token system: name colours by ROLE (surface, primary, on_surface),
// set them ONCE at the root with `theme(t)`, and every node that uses a token
// mod (`bg_surface`, `fg_primary`…) reads the live value. Recolour a whole app —
// or ship dark/light — by swapping the Theme at the root. Nodes stay decoupled
// from concrete hex: maximally modular + reusable.
struct Theme {
    std::uint32_t bg       = color::bg0;    // page background
    std::uint32_t surface  = color::bg1;    // cards / panels
    std::uint32_t raised   = color::bg2;    // raised surface
    std::uint32_t line     = color::line;   // borders
    std::uint32_t text     = color::ink;    // primary text
    std::uint32_t muted    = color::muted;  // secondary text
    std::uint32_t primary  = color::brand;  // brand / accent
    std::uint32_t accent   = color::brand2; // secondary accent
    std::uint32_t success  = color::good;
    std::uint32_t warning  = color::warn;
    std::uint32_t danger   = color::bad;
    std::uint32_t on_primary = color::white;// text on a primary surface
};

namespace detail {
inline void put_var(Style& s, const char* name, std::uint32_t c){ s.extra.emplace_back(name, hexstr(c)); }
}

/// `theme(t)` — a ROOT mod: declares every token as a CSS variable, so all
/// descendants can read `var(--wa-primary)` etc. Put it on your page/app root.
inline Mod theme(const Theme& t){
    return sty([=](Style& s){
        detail::put_var(s, "--wa-bg", t.bg);          detail::put_var(s, "--wa-surface", t.surface);
        detail::put_var(s, "--wa-raised", t.raised);  detail::put_var(s, "--wa-line", t.line);
        detail::put_var(s, "--wa-text", t.text);      detail::put_var(s, "--wa-muted", t.muted);
        detail::put_var(s, "--wa-primary", t.primary); detail::put_var(s, "--wa-accent", t.accent);
        detail::put_var(s, "--wa-success", t.success); detail::put_var(s, "--wa-warning", t.warning);
        detail::put_var(s, "--wa-danger", t.danger);   detail::put_var(s, "--wa-on-primary", t.on_primary);
    });
}

// Token mods — use these instead of raw fg()/bg() to stay themeable. Each reads
// the live CSS variable set by `theme(...)` at the root.
inline Mod fg_token(const char* var){ return sty([=](Style& s){ s.extra.emplace_back("color", std::string("var(")+var+")"); }); }
inline Mod bg_token(const char* var){ return sty([=](Style& s){ s.extra.emplace_back("background", std::string("var(")+var+")"); }); }
inline const Mod fg_text    = fg_token("--wa-text");
inline const Mod fg_muted   = fg_token("--wa-muted");
inline const Mod fg_primary = fg_token("--wa-primary");
inline const Mod fg_on_primary = fg_token("--wa-on-primary");
inline const Mod bg_surface = bg_token("--wa-surface");
inline const Mod bg_raised  = bg_token("--wa-raised");
inline const Mod bg_primary = bg_token("--wa-primary");
inline const Mod bg_page    = bg_token("--wa-bg");
/// `border_token()` — a 1px border in the theme's line colour.
inline Mod border_token(){ return sty([](Style& s){ s.extra.emplace_back("border", "1px solid var(--wa-line)"); }); }

// ── memoization ─ skip rebuilding an expensive subtree when its inputs are same ─
// The diff already skips UNCHANGED subtrees in O(depth) via the subtree hash, so
// you rarely need this. Reach for it only when BUILDING a subtree is itself
// costly (a big list, a heavy chart) and its inputs change rarely: `memo(key,
// build)` returns the cached NodeRef while `key` is unchanged, skipping `build`.
// `key` is a hash the caller computes from the subtree's inputs.
namespace detail {
inline thread_local std::unordered_map<std::uint64_t, std::pair<std::uint64_t, NodeRef>> g_memo;
}
/// `memo(cache_id, deps_hash, [&]{ return … })` — cache_id identifies the memo
/// slot (one per call-site), deps_hash is the inputs' hash; build runs only when
/// deps_hash changes.
template <typename Fn>
NodeRef memo(std::uint64_t cache_id, std::uint64_t deps_hash, Fn build){
    auto& slot = detail::g_memo[cache_id];
    if (!slot.second || slot.first != deps_hash) { slot.first = deps_hash; slot.second = build(); }
    return slot.second;
}

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

// ── routing: render the screen for the current route ────────────────────
/// `screens(id, { {Home, [&]{…}}, {UserDetail, [&]{…}} })` — pick the builder
/// for the current screen id and render it. Replaces the manual
/// when(route==A, …, when(route==B, …)) ladder: a flat, scalable table. Unknown
/// ids render an empty box (add a catch-all id for a 404 screen).
inline NodeRef screens(int id, std::vector<std::pair<int, std::function<NodeRef()>>> table){
    for (auto& [k, build] : table) if (k == id) return build();
    return box();
}

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

/// `page(bg_color, content…)` — the app root for CONTENT that flows down the
/// page (articles, forms, dashboards): fills at least the viewport, paints its
/// background edge-to-edge, pads fluidly, and grows + scrolls naturally when the
/// content is taller than the screen. This is the safe default for most apps.
template <typename... Cs> NodeRef page(std::uint32_t bg_color, Cs... cs){
    auto n = col(std::move(cs)...);
    n->style.has_bg = true; n->style.bg = bg_color;
    n->style.extra.emplace_back("min-height", "100dvh");   // dynamic vh: excludes mobile browser chrome
    n->style.extra.emplace_back("min-width", "0");
    n->style.extra.emplace_back("padding", "clamp(0px, 3vw, 2.5rem)");
    finalize(*n);
    return n;
}

/// `app_shell(bg_color, content…)` — the app root for a FIXED-VIEWPORT app whose
/// inner region scrolls (chat, mail, a board): bounded to exactly the viewport
/// height (dynamic, so it tracks the mobile browser chrome + keyboard), so a
/// composer/toolbar stays pinned and only the region you mark `scroll_fill()`
/// scrolls — the app never scrolls off the bottom of a phone.
template <typename... Cs> NodeRef app_shell(std::uint32_t bg_color, Cs... cs){
    auto n = col(std::move(cs)...);
    n->style.has_bg = true; n->style.bg = bg_color;
    n->style.extra.emplace_back("height", "100dvh");
    n->style.extra.emplace_back("max-height", "100dvh");
    n->style.extra.emplace_back("min-height", "0");
    n->style.extra.emplace_back("min-width", "0");
    n->style.extra.emplace_back("overflow", "hidden");   // only scroll_fill() regions scroll
    n->style.extra.emplace_back("padding", "clamp(0px, 3vw, 2.5rem)");
    finalize(*n);
    return n;
}

/// `safe_area` — pad by the device's safe-area insets (iPhone notch / home bar).
/// Put it on a full-bleed page so content isn't hidden under the notch.
inline Mod safe_area(){ return css("padding",
    "env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)"); }

/// `centered(max_rem, content)` — a column capped to `max_rem` wide and centred,
/// growing to fill available height. The classic "readable centred content"
/// container (chat, article, form). Fluidly full-width below the cap. Passes the
/// height budget through (min-height:0) so a scrolling child inside stays
/// contained rather than overflowing the page.
inline NodeRef centered(float max_rem, NodeRef content){
    auto n = col(std::move(content));
    n->style.has_grow = true; n->style.grow = 1;
    n->style.extra.emplace_back("width", "100%");
    n->style.extra.emplace_back("max-width", std::to_string((int)max_rem) + "rem");
    n->style.extra.emplace_back("margin-inline", "auto");
    n->style.extra.emplace_back("min-height", "0");   // allow inner flex scroller to be bounded
    finalize(*n);
    return n;
}

/// `fills(content)` — mark a node to fill its flex parent AND be a proper flex
/// container whose own overflowing child can scroll (flex:1 + min-height:0). Use
/// on a scrollable region (a chat log, a list) so it takes the leftover space
/// and scrolls internally instead of pushing siblings off-screen.
inline Mod scroll_fill(){
    return sty([](Style& s){
        s.has_grow = true; s.grow = 1;
        s.extra.emplace_back("min-height", "0");
        s.extra.emplace_back("overflow-y", "auto");
        s.extra.emplace_back("-webkit-overflow-scrolling", "touch");
    });
}

} // namespace waya::surface
