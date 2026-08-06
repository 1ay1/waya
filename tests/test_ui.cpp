// tests/test_ui.cpp — the official component library. Every component is built
// on the public core, so these checks also prove the core is expressive enough
// to carry a real component library (theme tokens, grid, assets registry).
#include <waya/surface/live.hpp>
#include <waya/ui.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;
using namespace waya::ui;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) {
    if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; }
}
static bool has(const std::string& hay, const std::string& n) { return hay.find(n) != std::string::npos; }
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }
static std::string html_of(NodeRef n) { return DomBackend{}.render(*n).html; }

int main() {
    assets().clear();

    // ── theme presets are plain Theme values bound via the core token system ──
    { auto n = box() | theme(midnight());
      check(has(css_of(n), "--wa-primary"), "theme() binds tokens"); }

    // ── button variants ─────────────────────────────────────────────────────
    check(has(css_of(button("Go", 1)), "cursor:pointer"), "button is clickable");
    check(has(html_of(button("Save", 1)), "Save"), "button renders label");
    { auto d = css_of(button("Del", 1, Variant::danger));
      check(has(d, "--wa-danger") || has(d, "#ef4444"), "danger variant uses danger colour"); }
    check(has(html_of(icon_button("x", 1)), "x"), "icon_button renders glyph");

    // ── card / link / divider ────────────────────────────────────────────────
    check(has(css_of(card(text("hi"))), "border-radius"), "card has radius");
    check(has(css_of(card(text("hi"))), "--wa-surface"), "card uses surface token");
    check(has(css_of(link("Home")), "--wa-primary"), "link uses primary token");
    check(has(css_of(divider()), "height:1px"), "divider is a hairline");
    check(has(css_of(divider(true)), "width:1px"), "vertical divider");

    // ── field wraps a control with a label ───────────────────────────────────
    { auto f = field("Email", input("x") | input_skin(), "helper");
      check(has(html_of(f), "Email"), "field renders label");
      check(has(html_of(f), "helper"), "field renders hint"); }

    // ── badges / dot / avatar ────────────────────────────────────────────────
    check(has(html_of(badge("new")), "new"), "badge renders text");
    check(has(css_of(badge("ok", Tone::success)), "--wa-success"), "badge tone");
    check(has(css_of(dot(Tone::danger)), "border-radius:999px"), "dot is round");
    check(has(html_of(avatar("AB")), "AB"), "avatar renders initials");
    check(has(css_of(avatar("AB")), "border-radius:999px"), "avatar is round");

    // ── spinner + skeleton register their own keyframes (the core asset seam) ─
    { auto s = spinner();
      check(has(assets().style_css(), "@keyframes wa-ui-spin"), "spinner registers keyframe");
      check(has(css_of(s), "animation:wa-ui-spin"), "spinner is animated"); }
    { auto sk = skeleton(px(200), px(16));
      check(has(assets().style_css(), "@keyframes wa-ui-shimmer"), "skeleton registers keyframe"); }

    // ── tabs highlights the active tab ───────────────────────────────────────
    { auto t = tabs(1, {{0,"A"},{1,"B"}}, +[](int i){ return i; });
      check(has(html_of(t), "A") && has(html_of(t), "B"), "tabs render labels");
      check(has(css_of(t), "border-bottom"), "tabs have an underline rail"); }

    // ── floating layers compose on core primitives ───────────────────────────
    check(has(html_of(popover(true, text("t"), col(link("Item")))), "Item"), "open popover shows panel");
    check(html_of(popover(false, text("t"), col(link("Item")))).find("Item") == std::string::npos,
          "closed popover hides panel");
    check(has(html_of(dialog(true, 9, text("Sure?"))), "Sure?"), "open dialog shows content");
    check(html_of(dialog(false, 9, text("Sure?"))).find("Sure?") == std::string::npos, "closed dialog hides");
    { auto tl = toast_layer({ toast("Saved", Tone::success) });
      check(has(html_of(tl), "Saved"), "toast_layer renders toasts");
      check(has(css_of(tl), "position:fixed"), "toast_layer is fixed"); }
    { auto tt = tooltip(text("hover"), "tip");
      check(has(html_of(tt), "tip"), "tooltip renders text");
      check(has(assets().style_css(), ".wa-tip-wrap"), "tooltip registers group-hover css"); }

    std::cout << "test_ui: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
