#pragma once
/// \file sugar.hpp
/// General, unopinionated conveniences over the surface vocabulary. Everything
/// here is either a pure combinator (when/each/screens/fragment), a layout shell
/// (page/app_shell/centered/scroll_fill), a floating-layer primitive (overlay/
/// modal/anchored), or the design-TOKEN system (theme + fg_*/bg_* token mods).
/// None of it bakes in a specific look — opinionated, ready-made COMPONENTS
/// (buttons, cards, dialogs, badges, the theme presets) live in the official
/// component library, waya/ui.hpp, which is built entirely on top of this.

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
// mod (`bg_surface`, `fg_primary`…) reads the live value. Recolour a whole app,
// ship dark/light, or switch themes LIVE — just swap the Theme at the root and
// the whole tree re-tints in one paint (nodes read the variables). Nodes stay
// decoupled from concrete hex: maximally modular + reusable.
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

    // ── the neutral default (slate/indigo) ─ the schema needs ONE baseline;
    // opinionated named presets (light/midnight/ocean/rose) live in the official
    // component library (waya/ui/theme.hpp), not the core.
    static Theme dark()  { return {}; }     // the default (slate/indigo)
    /// Recolour just the accent, keeping the rest — `Theme::dark().tint(0x22c55e)`.
    Theme tint(std::uint32_t p, std::uint32_t a = 0) const { Theme t=*this; t.primary=p; if(a) t.accent=a; return t; }
};

namespace detail {
inline void put_var(Style& s, const char* name, std::uint32_t c){ s.extra.emplace_back(name, hexstr(c)); }
}

/// `theme(t)` — a ROOT mod: declares every token as a CSS variable, so all
/// descendants can read `var(--wa-primary)` etc. Put it on your page/app root.
/// Because the variables live on ONE node, switching themes live (store the
/// Theme in your Model, change it in update) re-tints the whole app in a single
/// paint — and the colour transition animates smoothly (see below).
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
inline const Mod fg_accent  = fg_token("--wa-accent");
inline const Mod fg_on_primary = fg_token("--wa-on-primary");
inline const Mod bg_surface = bg_token("--wa-surface");
inline const Mod bg_raised  = bg_token("--wa-raised");
inline const Mod bg_primary = bg_token("--wa-primary");
inline const Mod bg_accent  = bg_token("--wa-accent");
inline const Mod bg_page    = bg_token("--wa-bg");
/// `border_token()` — a 1px border in the theme's line colour.
inline Mod border_token(){ return sty([](Style& s){ s.extra.emplace_back("border", "1px solid var(--wa-line)"); }); }

// ── expressive building blocks ─ the things every real UI needs, one call ────
/// `push()` — a flexible gap that shoves siblings apart. `row(logo, push(), menu)`
/// pins logo left and menu right. Reads clearer than a bare growing box.
inline NodeRef push(){ auto n = box(); n->style.has_grow=true; n->style.grow=1; finalize(*n); return n; }

// ── floating layers ─ dropdowns, tooltips, popovers without absolute-by-hand ─
/// `anchored(trigger, floating, place)` — render `floating` positioned relative
/// to `trigger`. `place`: "bottom"/"bottom-right"/"top"/"right". The parent is
/// made position:relative and the floater absolute — no CSS by hand. Show/hide
/// the floater with `when(open, …)`.
inline NodeRef anchored(NodeRef trigger, NodeRef floating, std::string place="bottom"){
    auto& s = floating->style;
    s.pos = Pos::absolute; s.has_z = true; s.z = 50;
    if (place=="bottom" || place=="bottom-right") s.extra.emplace_back("top","calc(100% + 8px)");
    if (place=="top")    s.extra.emplace_back("bottom","calc(100% + 8px)");
    if (place=="right")  { s.extra.emplace_back("left","calc(100% + 8px)"); s.extra.emplace_back("top","0"); }
    if (place=="bottom-right" || place=="top") s.extra.emplace_back("right","0");
    else if (place=="bottom") s.extra.emplace_back("left","0");
    finalize(*floating);
    auto n = box(std::move(trigger), std::move(floating));
    n->style.pos = Pos::relative;
    n->style.extra.emplace_back("display","inline-flex");
    finalize(*n); return n;
}
/// `themed()` — the everyday combo: page-bg, primary text, and a smooth colour
/// transition so a LIVE theme switch animates instead of snapping. Put on the
/// same node as `theme(t)` (typically the page root).
inline Mod themed(){ return sty([](Style& s){
    s.extra.emplace_back("background", "var(--wa-bg)");
    s.extra.emplace_back("color", "var(--wa-text)");
    s.extra.emplace_back("transition", "background-color .3s ease, color .3s ease");
}); }
/// `theme_transition()` — add to any themed node so its colours ease when the
/// theme changes (cards, borders). One line for a polished live switch.
inline Mod theme_transition(){ return sty([](Style& s){
    s.extra.emplace_back("transition", "background-color .3s ease, color .3s ease, border-color .3s ease");
}); }

// ── memoization ─ skip rebuilding an expensive subtree when its inputs are same ─
// The diff already skips UNCHANGED subtrees in O(depth) via the subtree hash, so
// you rarely need this. Reach for it only when BUILDING a subtree is itself
// costly (a big list, a heavy chart) and its inputs change rarely. The ergonomic
// `memo(props..., build)` and `component(fn)` live in surface/component.hpp —
// include that for reusable, auto-memoised components.

// ── Combinators ───────────────────────────────────────────────────

/// An empty node that renders NOTHING and takes NO space — `display:none`, no
/// children. This is what a hidden conditional should collapse to: unlike an
/// empty `box()`, it never adds a phantom flex gap between its siblings.
inline NodeRef nothing(){
    auto n = std::make_shared<Node>(); n->kind = Kind::box;
    n->style.extra.emplace_back("display", "none");
    finalize(*n);
    return n;
}
/// `when(cond, node)` — the node when true, nothing (zero-size) when false. So
/// `col(header, when(loading, spinner()), body)` just works with no phantom gap.
inline NodeRef when(bool cond, NodeRef node){
    return cond ? node : nothing();
}
/// `when(cond, a, b)` — a or b.
inline NodeRef when(bool cond, NodeRef a, NodeRef b){ return cond ? a : b; }
/// `when(cond, …]{ return node; })` — lazy: builds the node only if shown, and
/// collapses to `nothing()` (renders nothing, no gap) when hidden — so a
/// conditional child never leaves a phantom space behind it.
template <typename Fn> requires std::is_invocable_r_v<NodeRef, Fn>
NodeRef when(bool cond, Fn build){ return cond ? build() : nothing(); }

/// `show(cond, node)` — alias for when; reads well for visibility toggles.
inline NodeRef show(bool cond, NodeRef node){ return when(cond, std::move(node)); }

// ── routing: render the screen for the current route ────────────────────
/// `screens(id, { {Home, [&]{…}}, {UserDetail, [&]{…}} })` — pick the builder
/// for the current screen id and render it. Replaces the manual
/// when(route==A, …, when(route==B, …)) ladder: a flat, scalable table. Unknown
/// ids render an empty box (add a catch-all id for a 404 screen).
inline NodeRef screens(int id, std::vector<std::pair<int, std::function<NodeRef()>>> table){
    for (auto& [k, build] : table) if (k == id) return build();
    return nothing();
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

/// `overlay(content)` — a fixed full-screen dim layer above the page, content
/// centred. The backdrop fades in; the content is padded off the edges so a
/// dialog never touches the screen on small viewports. Add `tap(Close)` for a
/// click-away, and `stop()` on the panel so content clicks don't close it.
inline NodeRef overlay(NodeRef content){
    auto n = box(std::move(content));
    auto& s = n->style;
    s.pos = Pos::fixed;
    s.has_z = true; s.z = 1000;
    s.flow = Flow::col; s.justify = Justify::center; s.align = Align::center;
    // inset:0 via the extra channel (Len{0} reads as "unset", so top/left/… must
    // go here) — this is what actually makes the layer cover the viewport.
    s.extra.emplace_back("inset", "0");
    s.extra.emplace_back("padding", "clamp(16px, 5vw, 40px)");   // never hug the edges
    s.extra.emplace_back("background", "rgba(4,6,12,.62)");
    s.extra.emplace_back("backdrop-filter", "blur(6px)");
    s.extra.emplace_back("-webkit-backdrop-filter", "blur(6px)");
    s.extra.emplace_back("animation", "wa-fade 200ms ease both");
    finalize(*n);
    return n;
}
/// `modal(cond, content)` — the overlay only when `cond`; renders nothing
/// otherwise (no phantom box in the layout).
inline NodeRef modal(bool open, NodeRef content){
    return open ? overlay(std::move(content)) : nothing();
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
    n->style.extra.emplace_back("overflow", "hidden");   // only scroll_fill() regions scroll
    n->style.extra.emplace_back("padding", "clamp(0px, 3vw, 2.5rem)");
    finalize(*n);
    return n;
}

/// `safe_area` — pad by the device's safe-area insets (iPhone notch / home bar).
/// Put it on a full-bleed page so content isn't hidden under the notch.
inline Mod safe_area(){ return detail::raw_css("padding",
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
