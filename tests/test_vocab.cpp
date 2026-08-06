// tests/test_vocab.cpp — the "never touch css() for common things" guarantee.
// Every everyday web pattern (sticky headers, pinned badges, carousels, edge
// offsets, transforms, clamps) has a named mod that emits the right CSS. If this
// passes, an app view shouldn't need the raw css() escape hatch.
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }
static bool has(NodeRef n, const std::string& needle) { return css_of(n).find(needle) != std::string::npos; }

int main() {
    // ── positioning: sticky header, corner pins, edge offsets ────────────────
    check(has(box() | sticky_top(),   "position:sticky") && has(box() | sticky_top(), "top:0"), "sticky_top()");
    check(has(box() | sticky_top(64), "top:64px"), "sticky_top(offset)");
    check(has(box() | sticky_bottom(),"position:sticky") && has(box() | sticky_bottom(), "bottom:0"), "sticky_bottom()");
    { auto n = box() | pin_top_right(8);
      check(has(n, "position:absolute") && has(n, "top:8px") && has(n, "right:8px"), "pin_top_right"); }
    { auto n = box() | pin_bottom_left(4);
      check(has(n, "bottom:4px") && has(n, "left:4px"), "pin_bottom_left"); }
    check(has(box() | top(0),    "top:0"),     "top(0) emits explicit 0");
    check(has(box() | bottom(4), "bottom:4px"),"bottom(px)");
    check(has(box() | left(rem(1)), "left:1rem"), "left(Len rem)");
    check(has(box() | right(10), "right:10px"), "right(float)");
    check(has(box() | z(50), "z-index:50"), "z(n)");
    check(has(box() | positioned, "position:relative"), "positioned() anchor");

    // ── scroll: axis scroll + hidden scrollbar ───────────────────────────────
    check(has(box() | scroll_x, "overflow-x:auto"), "scroll_x");
    check(has(box() | scroll_y, "overflow-y:auto"), "scroll_y");
    check(has(box() | no_scrollbar, "scrollbar-width:none"), "no_scrollbar");

    // ── transforms ───────────────────────────────────────────────────────────
    check(has(box() | translate(10),   "translate(10px,0px)"), "translate(x)");
    check(has(box() | translate(10, 5),"translate(10px,5px)"), "translate(x,y)");
    check(has(box() | rotate(45), "rotate(45deg)"), "rotate");
    check(has(box() | scale(1.1f), "scale(1.1"), "scale");
    check(has(box() | backdrop_blur(10), "backdrop-filter:blur(10px)"), "backdrop_blur");

    // ── size clamps now accept bare numbers (consistency with pad/w) ─────────
    check(has(box() | min_w(200), "min-width:200px"), "min_w(float)");
    check(has(box() | max_w(600), "max-width:600px"), "max_w(float)");
    check(has(box() | min_h(100), "min-height:100px"), "min_h(float)");
    check(has(box() | max_h(400), "max-height:400px"), "max_h(float)");
    check(has(box() | min_w(rem(15)), "min-width:15rem"), "min_w(Len) still works");

    // ── the acid test: a sticky frosted nav with ZERO css() ──────────────────
    {
        auto nav = row(text("Brand") | bold, text("Login"))
            | pad_x(24) | pad_y(14) | gap(20) | center
            | sticky_top() | z(50) | backdrop_blur(10);
        auto c = css_of(nav);
        check(c.find("position:sticky") != std::string::npos &&
              c.find("z-index:50")      != std::string::npos &&
              c.find("backdrop-filter") != std::string::npos,
              "sticky frosted nav built with no css()");
    }

    std::cout << "test_vocab: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
