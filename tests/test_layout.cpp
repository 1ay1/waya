/// tests/test_layout.cpp — layout components: row/col/grid/sidebar/spacer/…
/// The "any layout is easy" guarantee: common layouts are one call, responsive,
/// and still fully composable with the style pipes.

#include <waya/waya.hpp>

#include <iostream>
#include <string>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)
static bool has(const std::string& h, std::string_view n) { return h.find(n) != std::string::npos; }

int main() {
    // ── row: horizontal, wraps ──────────────────────────────────────────────
    {
        auto r = waya::render::render(row(div_(text("a")), div_(text("b"))));
        CHECK(has(r.css, "display:flex"));
        CHECK(has(r.css, "flex-direction:row"));
        CHECK(has(r.css, "flex-wrap:wrap"));
    }
    // ── col: vertical stack ─────────────────────────────────────────────────
    {
        auto r = waya::render::render(col(div_(text("a")), div_(text("b"))));
        CHECK(has(r.css, "flex-direction:column"));
    }
    // ── spacer: flex-grow filler ────────────────────────────────────────────
    {
        auto r = waya::render::render(row(div_(text("l")), spacer(), div_(text("r"))));
        CHECK(has(r.css, "flex-grow:1"));
    }
    // ── grid + cols(n) ──────────────────────────────────────────────────────
    {
        auto r = waya::render::render(grid(div_(text("a")), div_(text("b"))) | cols(3));
        CHECK(has(r.css, "display:grid"));
        CHECK(has(r.css, "repeat(3, minmax(0,1fr))"));
    }
    // ── grid_auto: responsive auto-fit, no breakpoints ──────────────────────
    {
        auto r = waya::render::render(grid_auto(240_px, div_(text("a"))));
        CHECK(has(r.css, "repeat(auto-fit, minmax(240px, 1fr))"));
    }
    // ── sidebar: rail + fluid main, stacks when narrow ──────────────────────
    {
        auto r = waya::render::render(sidebar(div_(text("nav")), div_(text("main")), 260_px));
        CHECK(has(r.css, "flex:1 1 260px"));   // the rail
        CHECK(has(r.css, "flex-wrap:wrap"));    // stacks when narrow
        CHECK(has(r.html, "nav"));
        CHECK(has(r.html, "main"));
    }
    // ── center ──────────────────────────────────────────────────────────────
    {
        auto r = waya::render::render(center(div_(text("x"))));
        CHECK(has(r.css, "justify-content:center"));
        CHECK(has(r.css, "align-items:center"));
    }
    // ── cluster: wrapping chips, items centred ──────────────────────────────
    {
        auto r = waya::render::render(cluster(span_(text("t1")), span_(text("t2"))));
        CHECK(has(r.css, "flex-wrap:wrap"));
        CHECK(has(r.css, "align-items:center"));
    }
    // ── divider ─────────────────────────────────────────────────────────────
    {
        auto r = waya::render::render(divider());
        CHECK(has(r.html, "<hr"));
        CHECK(has(r.css, "border-top:1px solid currentColor"));
    }

    // ── composability: layout components still take style pipes ─────────────
    {
        auto r = waya::render::render(
            row(div_(text("a"))) | gap(24_px) | pad(16_px) | bg(0x111827));
        CHECK(has(r.css, "gap:24px"));
        CHECK(has(r.css, "padding:16px"));
        CHECK(has(r.css, "background:#111827"));
        CHECK(has(r.css, "flex-direction:row"));   // the preset survives the pipes
    }

    std::cout << "test_layout: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
