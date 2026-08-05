/// tests/test_layout.cpp — the layout system: intrinsically responsive
/// primitives (no media queries), correct flex defaults, sizing intent.

#include <waya/surface/layout.hpp>
#include <waya/surface/dom.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }
static std::string css_of(NodeRef n){ return DomBackend{}.render(*n).css; }

int main() {
    // ── flex correctness: every flex container gets min-width:0 so children can
    //    shrink (fixes overflow / truncation / wrapping) ─────────────────────
    {
        CHECK(has(css_of(row(text("x"))), "min-width:0"));
        CHECK(has(css_of(col(text("x"))), "min-width:0"));
    }

    // ── grid: intrinsic auto-fit, never overflows on narrow (min(...,100%)) ─
    {
        auto c = css_of(grid(px(240), box(text("a")), box(text("b"))));
        CHECK(has(c, "display:grid"));
        CHECK(has(c, "repeat(auto-fit,minmax(min(240px,100%),1fr))"));
    }

    // ── cluster: wraps by itself, items centred ─────────────────────────────
    {
        auto c = css_of(cluster(text("a"), text("b")));
        CHECK(has(c, "flex-wrap:wrap"));
        CHECK(has(c, "align-items:center"));
    }

    // ── switcher: flips row↔column with NO media query (the calc trick) ─────
    {
        auto c = css_of(switcher(px(400), box(text("x")), box(text("y"))));
        CHECK(has(c, "flex-wrap:wrap"));
        CHECK(has(c, "calc((400px - 100%) * 999)"));   // the intrinsic switch
    }

    // ── sidebar: rail basis + fluid main that wraps when narrow ─────────────
    {
        auto c = css_of(sidebar(box(text("nav")), box(text("main")), rem(16)));
        CHECK(has(c, "flex-basis:16rem"));
        CHECK(has(c, "flex:999 1 auto"));               // main grows hard
        CHECK(has(c, "flex-wrap:wrap"));
    }

    // ── center_col: max width + auto inline margins (readable measure) ──────
    {
        auto c = css_of(center_col(text("prose")));
        CHECK(has(c, "max-width:760px"));
        CHECK(has(c, "margin-inline:auto"));
    }

    // ── hero: full viewport height, vertically centred ──────────────────────
    {
        auto c = css_of(hero(text("welcome")));
        CHECK(has(c, "min-height:100vh"));
        CHECK(has(c, "justify-content:center"));
    }

    // ── sizing intent: flexible = grow; spacer is a growing box ─────────────
    {
        auto r = row(text("l"), spacer(), text("r"));
        auto c = css_of(r);
        CHECK(has(c, "flex:1 1 auto"));                 // the spacer grows
        auto f = css_of(box(text("x")) | flexible);
        CHECK(has(f, "flex:1 1 auto"));
    }

    // ── fluid type: clamp(), responsive with no breakpoint ──────────────────
    {
        auto c = css_of(text("Big") | fluid_font(24, 48));
        CHECK(has(c, "clamp("));
        CHECK(has(c, "48px"));
    }

    // ── input renders coloured text (the black-input bug is fixed) ──────────
    {
        auto c = css_of(input("hi") | fg(0xe2e8f0) | font(16));
        CHECK(has(c, "color:#e2e8f0"));
        CHECK(has(c, "font-size:16px"));
    }

    std::cout << "test_layout: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
