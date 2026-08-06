// tests/test_depth.cpp — the maturity/polish tier: global keyboard shortcuts,
// accessibility (sr_only/aria/focus), interaction feedback (group/ripple).
// These are what separate a toy from a framework you'd ship a product on.
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static std::string out_of(NodeRef n) { auto o = DomBackend{}.render(*n); return o.html + "\n" + o.css; }
static bool has(NodeRef n, const std::string& s) { return out_of(n).find(s) != std::string::npos; }

int main() {
    // ── global keyboard shortcuts ────────────────────────────────────────────
    check(has(box() | on_shortcut("mod+k", 1), "data-ev-shortcut=\""), "on_shortcut renders shortcut attr");
    check(has(box() | hotkey("?", 2), "data-ev-shortcut=\""), "hotkey renders shortcut attr");
    // the combo spec rides in the attr value after the token
    check(out_of(box() | on_shortcut("mod+k", 1)).find("mod+k") != std::string::npos, "combo spec present");
    // focused-node keyboard still works
    check(has(input("") | on_enter(3), "data-ev-keydown=\""), "on_enter wires keydown");
    check(out_of(input("") | on_key("ArrowDown", 4)).find("ArrowDown") != std::string::npos, "on_key filter present");

    // ── accessibility ────────────────────────────────────────────────────────
    check(has(text("skip") | sr_only, "clip:rect(0,0,0,0)"), "sr_only visually hidden");
    check(has(text("skip") | sr_only, "position:absolute"), "sr_only positioned off-screen");
    check(has(box() | aria_label("Close menu"), "aria-label=\"Close menu\""), "aria_label");
    check(has(box() | role("dialog"), "role=\"dialog\""), "role");
    check(has(box() | aria("expanded", "true"), "aria-expanded=\"true\""), "aria(k,v)");

    // ── focus management ─────────────────────────────────────────────────────
    check(has(input("") | autofocus(), "autofocus"), "autofocus attr");
    check(has(input("") | focus_ring(0x6366f1), "outline"), "focus_ring emits outline on focus");
    check(has(input("") | focus_ring(), ":focus"), "focus_ring uses :focus state");
    check(has(box() | focus_within(css("border", "1px solid red")), ":focus-within"), "focus_within selector");

    // ── interaction feedback ─────────────────────────────────────────────────
    check(has(box() | group(), "wa-group"), "group() marks the parent");
    check(has(box() | group_hidden(), "data-wa-gh"), "group_hidden marks the child");
    check(has(box() | group_hidden(), "opacity:0"), "group_hidden starts hidden");
    check(has(box() | ripple(0x818cf8), "data-wa-ripple"), "ripple marks the node");
    check(has(box() | ripple(), "data-wa-ripple-color"), "ripple carries a colour");
    // ripple registers its keyframe on the asset registry
    { assets().clear(); auto n = box() | ripple(); (void)out_of(n);
      check(assets().style_css().find("wa-ripple") != std::string::npos, "ripple registers its keyframe"); }

    std::cout << "test_depth: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
