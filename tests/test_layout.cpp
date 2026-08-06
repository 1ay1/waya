/// tests/test_layout.cpp — the layout system: intrinsically responsive
/// primitives (no media queries), correct flex defaults, sizing intent.

#include <waya/surface/layout.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/diff.hpp>
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

    // ── path (chart) SCALES to its container — has a viewBox + width:100%, so
    //    it can't overflow its card (the chart-spill bug is fixed) ───────────
    {
        auto html = DomBackend{}.render(*(path({{0,50},{100,10},{520,25}}) | stroke(0x22d3ee,2))).html;
        CHECK(has(html, "<svg"));
        CHECK(has(html, "viewBox=\""));                 // scales from a coordinate box
        CHECK(has(html, "preserveAspectRatio=\"none\""));
        CHECK(has(html, "width:100%"));                 // fills the container
        CHECK(has(html, "non-scaling-stroke"));         // line stays crisp
    }

    // ── ZStack: stack() overlays children in one centred grid cell ─────────
    {
        auto c = css_of(stack(text("bg"), text("top")));
        CHECK(has(c, "display:grid"));
        CHECK(has(c, "place-items:center"));            // children centred
        CHECK(has(c, ">*{grid-area:1/1}"));             // every child shares the cell (overlay)
    }

    // ── responsive shell helpers: page scrolls, app_shell is bounded ───────
    {
        auto c = css_of(page(0x0b1020, text("x")));
        CHECK(has(c, "min-height:100dvh"));             // fills viewport (dynamic vh)
        CHECK(has(c, "clamp("));                        // fluid padding
        CHECK(!has(c, "max-height"));                   // page GROWS + scrolls (not capped)
    }
    {
        auto c = css_of(app_shell(0x0b1020, text("x")));
        CHECK(has(c, "height:100dvh"));                 // bounded to the screen
        CHECK(has(c, "max-height:100dvh"));
        CHECK(has(c, "overflow:hidden"));               // only inner scroll_fill scrolls
    }
    {
        // scroll_fill takes leftover space and scrolls internally (composer-pin)
        auto c = css_of(box(text("row")) | scroll_fill());
        CHECK(has(c, "flex:1 1 auto"));
        CHECK(has(c, "min-height:0"));                  // so the child can shrink → overflow engages
        CHECK(has(c, "overflow-y:auto"));
        CHECK(has(c, "-webkit-overflow-scrolling:touch"));
    }
    {
        // centered caps width + centres, passes the height budget through
        auto c = css_of(centered(42, text("x")));
        CHECK(has(c, "max-width:42rem"));
        CHECK(has(c, "margin-inline:auto"));
        CHECK(has(c, "min-height:0"));
    }

    // ── fluid typography + spacing: clamp() so big text/pad shrink on phones ─
    {
        auto c = css_of(text("9") | font_fluid(40, 76));
        CHECK(has(c, "font-size:clamp(40"));
        CHECK(has(c, "76"));                            // desktop cap
        CHECK(has(c, "vw"));                            // scales with viewport
    }
    {
        auto c = css_of(box() | pad_fluid(16, 56));
        CHECK(has(c, "padding:clamp(16"));
        CHECK(has(c, "56"));
    }

    // ── polish layer: motion / elevation / glass / typography / theme ───────
    {
        // motion mods reference the shell's wa-* keyframes via `animation`
        CHECK(has(css_of(box() | spin()),      "animation:wa-spin"));
        CHECK(has(css_of(box() | pulse()),     "animation:wa-pulse"));
        CHECK(has(css_of(box() | fade_up()),   "animation:wa-fade-up"));
        CHECK(has(css_of(box() | pop_in()),    "animation:wa-pop"));
        CHECK(has(css_of(box() | animate("wa-fade", 250)), "wa-fade 250ms"));
        CHECK(has(css_of(box() | shimmer()),   "animation:wa-shimmer"));
        CHECK(has(css_of(box() | delay(120)),  "animation-delay:120ms"));
    }
    {
        // elevation is a shadow scale; glow/ring/glass are modern surface effects
        CHECK(has(css_of(box() | elevation(3)), "box-shadow:"));
        CHECK(has(css_of(box() | glow(0x6366f1)), "box-shadow:"));
        CHECK(has(css_of(box() | ring(0x22d3ee, 2)), "box-shadow:0 0 0 2px"));
        auto g = css_of(box() | glass());
        CHECK(has(g, "backdrop-filter:blur"));
    }
    {
        // typography presets set size/weight, still overridable afterwards
        CHECK(has(css_of(text("x") | display), "font-size:32px"));
        CHECK(has(css_of(text("x") | heading), "font-weight:700") || has(css_of(text("x") | heading), "font-size:24px"));
        CHECK(has(css_of(text("x") | label),   "text-transform:uppercase"));
        CHECK(has(css_of(text("x") | mono),    "ui-monospace"));
        // override wins: display then a bigger font
        CHECK(has(css_of(text("x") | display | font(50)), "font-size:50px"));
    }
    {
        // theme tokens: theme() emits CSS vars at the root; token mods read them
        auto root = css_of(box() | theme(Theme{}));
        CHECK(has(root, "--wa-primary:"));
        CHECK(has(root, "--wa-surface:"));
        CHECK(has(css_of(box() | bg_surface), "var(--wa-surface)"));
        CHECK(has(css_of(text("x") | fg_primary), "var(--wa-primary)"));
        CHECK(has(css_of(box() | border_token()), "1px solid var(--wa-line)"));
        // presets emit distinct palettes
        CHECK(has(css_of(box() | theme(Theme::light())), "--wa-bg:#f8fafc"));
        CHECK(has(css_of(box() | theme(Theme::ocean())), "--wa-primary:#14b8a6"));
        // tint() recolours just the accent
        CHECK(has(css_of(box() | theme(Theme::dark().tint(0x22c55e))), "--wa-primary:#22c55e"));
        // themed() paints bg+text from vars with a smooth transition (live switch)
        auto th = css_of(box() | themed());
        CHECK(has(th, "background:var(--wa-bg)") && has(th, "color:var(--wa-text)"));
        CHECK(has(th, "transition:background-color"));
        // a LIVE theme switch is ONE op (root set_paint); children read vars
        auto a = box(text("x") | fg_text, box() | bg_surface) | theme(Theme::dark()) | themed();
        auto b = box(text("x") | fg_text, box() | bg_surface) | theme(Theme::light()) | themed();
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_paint && p[0].path == "");
    }

    // ── alignment defaults: a ROW vertically-centres, a COL doesn't force it ─
    {
        CHECK(has(css_of(row(text("a"), text("b"))), "align-items:center"));
        // an explicit align wins over the default
        CHECK(has(css_of(row(text("a")) | align(Align::start)), "align-items:flex-start"));
        // col keeps the CSS default (stretch) — no forced center
        CHECK(!has(css_of(col(text("a"), text("b"))), "align-items:center"));
    }

    // ── fixed grids: real column tracks so cells ALIGN across rows ──────────
    {
        CHECK(has(css_of(box() | grid_cols(3)), "grid-template-columns:repeat(3,minmax(0,1fr))"));
        CHECK(has(css_of(box() | grid_template("2fr 1fr")), "grid-template-columns:2fr 1fr"));
        CHECK(has(css_of(box() | col_span(2)), "grid-column:span 2"));
        auto c = css_of(columns(3, text("a"), text("b"), text("c")));
        CHECK(has(c, "display:grid"));
        CHECK(has(c, "repeat(3,minmax(0,1fr))"));
    }

    // ── rendering-correctness regressions ───────────────────────────
    {
        // transforms COMPOSE (scale + rotate) instead of the last winning
        auto c = css_of(box() | scale(1.1f) | rotate(45));
        CHECK(has(c, "transform:scale(1.1) rotate(45deg)"));
        CHECK(!has(c, "transform:scale(1.1);transform:"));   // not two declarations
    }
    {
        // filters compose too (blur + another filter)
        auto c = css_of(box() | blur(2) | css("filter","brightness(1.2)"));
        CHECK(has(c, "filter:blur(2px) brightness(1.2)"));
    }
    {
        // fg on a CONTAINER emits color (it inherits to text descendants)
        CHECK(has(css_of(box(text("x")) | fg(0xff0000)), "color:#ff0000"));
    }
    {
        // underline + strike COMBINE into one text-decoration
        auto c = css_of(text("x") | underline | strike);
        CHECK(has(c, "text-decoration:underline line-through"));
        CHECK(!has(c, "text-decoration:underline;text-decoration:"));
    }
    {
        // box-shadow is additive (multiple shadows are valid, comma-joined)
        auto c = css_of(box() | css("box-shadow","0 1px 2px #000") | css("box-shadow","0 0 8px #f00"));
        CHECK(has(c, "box-shadow:0 1px 2px #000, 0 0 8px #f00"));
    }

    std::cout << "test_layout: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
