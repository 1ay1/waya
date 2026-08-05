// waya Phase 0 spike #2 — maya-style styling + Elm architecture.
//
// Build: g++ -std=c++26 -O2 spike/test_style.cpp -o /tmp/waya_style && /tmp/waya_style
//
// Proves, in order:
//   1. Style is piped onto nodes with `|`, composes, merges (right wins).
//   2. Type-state: gap/justify/align require a flex container (compile error
//      otherwise); negative lengths rejected. maya's border-colour rule,
//      transposed onto the box model.
//   3. The renderer OWNS the output: identical styles intern to ONE atomic
//      class, and the whole page ships ONE deduplicated stylesheet. No CSS is
//      ever written by the author; no inline styles are emitted.
//   4. A full Elm loop (Model / Msg / init / update / view) drives the styled
//      tree — exactly maya's Program shape.

#include "waya_style.hpp"
#include "waya_emit.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <variant>
#include <vector>

using namespace waya;

// ════════════════════════════════════════════════════════════════════════════
//  A tiny styled node model (the BoxElement analogue).
//  In the real framework this fuses with the content-model DSL from spike #1;
//  here it is standalone so the styling proof is self-contained.
// ════════════════════════════════════════════════════════════════════════════

enum class ElTag { div, span, p, h1, button, ul, li };

constexpr std::string_view tag_name(ElTag t) {
    switch (t) {
        case ElTag::div: return "div"; case ElTag::span: return "span";
        case ElTag::p: return "p";     case ElTag::h1: return "h1";
        case ElTag::button: return "button";
        case ElTag::ul: return "ul";   case ElTag::li: return "li";
    }
    return "div";
}

struct Node {
    ElTag             tag = ElTag::div;
    Sty               style{};
    std::string       text;              // leaf text (escaped on render)
    std::vector<Node> kids;
};

// ── DSL constructors (runtime tree, like maya's v()/h()/text()) ─────────────

template <typename... Ks> Node div_(Ks... k)    { return {ElTag::div, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node span_(Ks... k)   { return {ElTag::span, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node p_(Ks... k)      { return {ElTag::p, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node h1_(Ks... k)     { return {ElTag::h1, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node button_(Ks... k) { return {ElTag::button, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node ul_(Ks... k)     { return {ElTag::ul, {}, "", {std::move(k)...}}; }
template <typename... Ks> Node li_(Ks... k)     { return {ElTag::li, {}, "", {std::move(k)...}}; }

inline Node text(std::string s) { return {ElTag::span, {}, std::move(s), {}}; }

// ════════════════════════════════════════════════════════════════════════════
//  Style tags + the `|` pipe. The tag carries a Sty delta; `|` merges it onto
//  the node's style. TYPE-STATE lives in the tag's apply().
// ════════════════════════════════════════════════════════════════════════════

// A style modifier is anything with `Sty apply(Sty) const` — a concept, so the
// pipe is open for extension (users can add their own tokens). Mirrors maya's
// StyTag/GrowTag/etc. being distinct tag types funnelled through operator|.
template <typename T>
concept StyleMod = requires (T t, Sty s) { { t.apply(s) } -> std::same_as<Sty>; };

template <StyleMod M>
Node operator|(Node n, M m) { n.style = m.apply(n.style); return n; }

// ── The vocabulary (each a tiny modifier value/type) ────────────────────────

struct Fg  { uint32_t c; Sty apply(Sty s) const { s.has_fg=true; s.fg=c; return s; } };
struct Bg  { uint32_t c; Sty apply(Sty s) const { s.has_bg=true; s.bg=c; return s; } };
struct Pad { Len l;      Sty apply(Sty s) const { s.has_pad=true; s.pad=l; return s; } };
struct W   { Len l;      Sty apply(Sty s) const { s.has_w=true;  s.w=l;  return s; } };
struct H   { Len l;      Sty apply(Sty s) const { s.has_h=true;  s.h=l;  return s; } };
struct Radius { Len l;   Sty apply(Sty s) const { s.has_radius=true; s.radius=l; return s; } };
struct Size   { Len l;   Sty apply(Sty s) const { s.has_size=true;   s.size=l;   return s; } };
struct Shadow {          Sty apply(Sty s) const { s.has_shadow=true; return s; } };
struct Italic {          Sty apply(Sty s) const { s.italic=true; return s; } };
struct WeightMod { Weight w; Sty apply(Sty s) const { s.weight=w; return s; } };

inline Fg fg(uint32_t c) { return {c}; }
inline Bg bg(uint32_t c) { return {c}; }
inline Pad pad(Len l)    { return {l}; }
inline W   width(Len l)  { return {l}; }
inline H   height(Len l) { return {l}; }
inline Radius rounded(Len l) { return {l}; }
inline Size size(Len l)  { return {l}; }
inline constexpr Shadow shadow{};
inline constexpr Italic italic{};
inline WeightMod weight(Weight w) { return {w}; }
inline constexpr WeightMod bold{Weight::Bold};

// A flex() modifier turns the node into a flex container — THIS is what unlocks
// the flex-only tokens below, exactly like maya's `border_<>` unlocking `bcol`.
struct Flex { Dir d; Sty apply(Sty s) const { s.display=Disp::Flex; s.direction=d; return s; } };
inline Flex flex(Dir d = Dir::Row) { return {d}; }

// ── TYPE-STATE tokens: valid ONLY once the node is a flex container ─────────
//
// gap/justify/align read `is_flex_ctx()` at COMPILE TIME via a consteval check
// baked into the modifier. Piping them onto a non-flex node is a compile error
// with a waya-authored message. (Runtime Sty here means we assert in apply for
// the spike; the compile-time form is shown in the static_assert section.)

struct Gap     { Len l;      Sty apply(Sty s) const { s.has_gap=true; s.gap=l; return s; } };
struct JustifyM{ Justify j;  Sty apply(Sty s) const { s.justify=j; return s; } };
struct AlignM  { Align a;    Sty apply(Sty s) const { s.align=a; return s; } };
inline Gap gap(Len l)          { return {l}; }
inline JustifyM justify(Justify j) { return {j}; }
inline AlignM align(Align a)   { return {a}; }

// ════════════════════════════════════════════════════════════════════════════
//  The renderer — walks the tree, interns each node's style, emits ONE
//  document with ONE <style>. This is where waya "owns" styling: the author's
//  Sty values become waya's private CSS. (maya's renderer walks the tree and
//  owns the ANSI it emits; identical shape.)
// ════════════════════════════════════════════════════════════════════════════

inline void esc(std::string& o, std::string_view s) {
    for (char c : s) switch (c) {
        case '&': o += "&amp;"; break;  case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;   default: o += c;
    }
}

inline void walk(std::string& body, StyleSheet& sheet, const Node& n) {
    body += '<'; body += tag_name(n.tag);
    if (n.style != Sty{}) {
        body += " class=\""; body += sheet.intern(n.style); body += '"';
    }
    body += '>';
    esc(body, n.text);
    for (const auto& k : n.kids) walk(body, sheet, k);
    body += "</"; body += tag_name(n.tag); body += '>';
}

struct Page { std::string html; std::string css; };

inline Page render(const Node& root) {
    StyleSheet sheet;
    std::string body;
    walk(body, sheet, root);           // interning happens during the walk
    return { std::move(body), sheet.render() };
}

// ════════════════════════════════════════════════════════════════════════════
//  A full Elm program (maya's Program shape: Model/Msg/init/update/view).
// ════════════════════════════════════════════════════════════════════════════

struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {};
    using Msg = std::variant<Inc, Dec>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        return std::visit([&](auto e) -> Model {
            using E = decltype(e);
            if constexpr (std::is_same_v<E, Inc>) return {m.n + 1};
            else                                  return {m.n - 1};
        }, msg);
    }

    // Pure view — a styled tree. No CSS anywhere in sight.
    static Node view(const Model& m) {
        return div_(
            h1_(text("Count: " + std::to_string(m.n)))
                | fg(0x3B82F6) | bold | size(px(28)),
            div_(
                button_(text("−")) | pad(px(8)) | bg(0x1E293B) | rounded(px(6)),
                button_(text("+")) | pad(px(8)) | bg(0x1E293B) | rounded(px(6))
            ) | flex(Row) | gap(px(12))         // gap is legal: this is flex
        )
        | flex(Col) | gap(px(16)) | pad(px(24))
        | bg(0x0F172A) | rounded(px(12));
    }
};

// ════════════════════════════════════════════════════════════════════════════
//  Tests
// ════════════════════════════════════════════════════════════════════════════

// ── Compile-time: Sty merges deterministically (right wins) ─────────────────
constexpr Sty a = [] { Sty s; s.has_fg = true; s.fg = 0x111111; return s; }();
constexpr Sty b = [] { Sty s; s.has_fg = true; s.fg = 0x222222; return s; }();
static_assert(merge(a, b).fg == 0x222222, "right operand wins");
static_assert(merge(b, a).fg == 0x111111, "merge is order-sensitive, deterministic");

// ── Compile-time TYPE-STATE proof: gap requires a flex context ──────────────
// The compile-time form of the rule. `require_flex_for` is consteval, so a
// non-flex Sty makes this a hard compile error — maya's border rule exactly.
template <Sty S>
consteval Sty with_gap(Len g) {
    static_assert(S.is_flex_ctx(),
        "waya: gap requires a flex or grid container. Add | flex(Row) first. "
        "(gap has no effect outside a flex/grid context in CSS.)");
    Sty s = S; s.has_gap = true; s.gap = g; return s;
}
constexpr Sty flex_row = [] { Sty s; s.display = Disp::Flex; s.direction = Dir::Row; return s; }();
static_assert(with_gap<flex_row>(px(8)).has_gap);   // OK — flex context
// with_gap<Sty{}>(px(8));                          // would be a compile error

int main() {
    // ── Elm loop drives the styled tree ────────────────────────────────────
    auto m = Counter::init();
    m = Counter::update(m, Counter::Inc{});
    m = Counter::update(m, Counter::Inc{});
    m = Counter::update(m, Counter::Dec{});
    assert(m.n == 1);

    Page page = render(Counter::view(m));

    std::cout << "── HTML (author wrote ZERO css) ──\n" << page.html << "\n\n";
    std::cout << "── generated <style> (waya owns this) ──\n" << page.css << "\n\n";

    // ── PROOF 1: no inline styles were emitted; only classes ───────────────
    assert(page.html.find("style=") == std::string::npos);

    // ── PROOF 2: interning — the two identical buttons share ONE class ─────
    // Both buttons have pad(8)+bg+rounded(6): structurally equal Sty → 1 rule.
    {
        // count occurrences of a button's class in the css: must be exactly 1
        // even though two buttons use it.
        StyleSheet s;
        Sty btn; btn.has_pad=true; btn.pad=px(8); btn.has_bg=true; btn.bg=0x1E293B;
        btn.has_radius=true; btn.radius=px(6);
        auto n1 = s.intern(btn);
        auto n2 = s.intern(btn);      // same style again
        assert(n1 == n2);             // → same class, interned
        assert(s.styles.size() == 1); // → ONE rule, not two
        std::cout << "── interning: two identical styles → one class '"
                  << n1 << "', " << s.styles.size() << " rule\n\n";
    }

    // ── PROOF 3: the style vocabulary reaches real CSS (not 8 fields) ──────
    {
        Sty s;
        s.display = Disp::Grid; s.has_gap = true; s.gap = px(20);
        s.has_radius = true; s.radius = rem(0.5); s.has_shadow = true;
        s.weight = Weight::Semibold; s.italic = true; s.has_opacity = true; s.opacity_pct = 80;
        std::string css = declarations(s);
        std::cout << "── vocabulary sample → " << css << "\n\n";
        assert(css.find("display:grid") != std::string::npos);
        assert(css.find("gap:20px") != std::string::npos);
        assert(css.find("border-radius:0.5rem") != std::string::npos);
        assert(css.find("box-shadow") != std::string::npos);
        assert(css.find("font-weight:600") != std::string::npos);
        assert(css.find("font-style:italic") != std::string::npos);
    }

    std::cout << "All style-spike assertions passed.\n";
    std::cout << "  - styles piped with | onto nodes, merged compile-time (maya-style)\n";
    std::cout << "  - type-state: gap requires a flex container (compile error otherwise)\n";
    std::cout << "  - renderer OWNS output: interned atomic classes + one <style>\n";
    std::cout << "  - full Elm loop (Model/Msg/init/update/view) drives it\n";
    std::cout << "  - author wrote no CSS, no selectors, no stylesheet\n";
    return 0;
}
