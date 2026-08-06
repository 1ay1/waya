// tests/test_typed.cpp — the maya-faithful style system: typed colors, the
// complete named vocabulary, length literals, and the runtime half of the
// type-state gates (the COMPILE-FAIL half is exercised by tests/fail/typed.cpp
// via ctest WILL_FAIL). If this compiles and runs, the valid dialect works.
#include <waya/surface/live.hpp>
#include <waya/surface/typed.hpp>
#include <waya/color.hpp>
#include <iostream>
#include <string>

using namespace waya::tui;               // the typed dialect
using namespace waya::color;             // typed colours
using namespace waya::surface::literals; // length UDLs

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
static std::string css_of(waya::surface::NodeRef n) { return waya::surface::DomBackend{}.render(*n).css; }

int main() {
    // ── typed colours ────────────────────────────────────────────────────────
    static_assert(indigo.opaque() == 0x6366f1, "named colour constexpr");
    static_assert(rgb(99,102,241).opaque() == 0x6366f1, "rgb triple constexpr");
    check(indigo.css() == "#6366f1", "opaque colour -> hex");
    check(rgba(0,0,0,0.4f).css().rfind("rgba(", 0) == 0, "translucent -> rgba()");
    {
        auto n = Text("x") | fg(indigo);
        check(has(css_of(n), "#6366f1"), "fg(Color) opaque uses interned path");
    }
    {
        auto n = Text("x") | bg(rgba(0,0,0,0.4f));
        check(has(css_of(n), "rgba(0,0,0,0.4"), "bg(Color) translucent preserves alpha");
    }
    // colour ops
    check(indigo.lighten(0.5f).opaque() != indigo.opaque(), "lighten changes value");
    check(indigo.alpha(0.5f).has_alpha(), "alpha() marks translucent");

    // ── length literals ──────────────────────────────────────────────────────
    {
        auto n = Box(Text("x")) | pad(16_px) | w(50_pct) | h(100_vh);
        auto c = css_of(n);
        check(has(c, "padding:16px"), "16_px literal");
        check(has(c, "width:50%"), "50_pct literal");
        check(has(c, "height:100vh") || has(c, "min-height"), "100_vh literal");
    }

    // ── typed builders establish the right context (gates COMPILE) ───────────
    {
        // Row/Col/Grid are containers: gap/justify/align all compile here.
        auto r = Row(Text("a"), Text("b")) | gap(12) | justify_between | align_center;
        auto c = css_of(r);
        check(has(c, "display:flex"), "Row is flex");
        check(has(c, "gap:12px"), "gap on Row compiles + emits");
        check(has(c, "justify-content:space-between"), "justify_between");
        check(has(c, "align-items:center"), "align_center");

        auto g = Grid(Text("a")) | gap(8);
        check(has(css_of(g), "display:grid"), "Grid is grid; gap compiles");

        auto col_ = Col(Text("a")) | gap(4);
        check(has(css_of(col_), "flex-direction:column"), "Col is a column flex");
    }

    // ── the complete named vocabulary (css() should be unnecessary) ──────────
    {
        auto n = Text("HELLO") | uppercase | tabular_nums | tracking(1.5f) | leading(1.4f);
        auto c = css_of(n);
        check(has(c, "text-transform:uppercase"), "uppercase mod");
        check(has(c, "tabular-nums"), "tabular_nums mod");
        check(has(c, "letter-spacing"), "tracking mod");
        check(has(c, "line-height:1.4"), "leading mod");
    }
    {
        auto n = Box(Text("desc")) | line_clamp(2);
        check(has(css_of(n), "-webkit-line-clamp:2"), "line_clamp mod");
    }
    {
        auto n = Box() | no_select | no_pointer;
        auto c = css_of(n);
        check(has(c, "user-select:none"), "no_select mod");
        check(has(c, "pointer-events:none"), "no_pointer mod");
    }

    // ── the tag erases cleanly to a plain NodeRef ────────────────────────────
    {
        waya::surface::NodeRef plain = Row(Text("x"));   // implicit conversion
        check(plain != nullptr && !plain->kids.empty(), "Styled -> NodeRef erasure");
    }

    std::cout << "test_typed: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
