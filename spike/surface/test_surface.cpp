// spike/surface — proving the thesis: you describe WHAT to render with a tiny
// vocabulary; waya owns HOW (HTML, canvas, whatever). The same view renders
// through any backend, unchanged. Powerful enough for anything, simple as hell.
//
// Build: g++ -std=c++26 spike/surface/test_surface.cpp -o /tmp/surf && /tmp/surf

#include "surface.hpp"
#include "backends.hpp"
#include "diff.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace waya::surface;

// ── A real-ish app view. NOTE: not one line mentions HTML, CSS, canvas, div,
//    flex, or onclick. Just: boxes, text, a chart (path), tap→message. ────────
enum Msg { Inc = 0, Reset = 1 };

static NodeRef view(int count, const std::vector<std::pair<float,float>>& chart) {
    return pad(bg(col({
        // header
        bold(size(fg(text("Dashboard"), 0xff3b82f6), 28)),
        // a stat card
        round_(pad(bg(row({
            fg(text("Requests"), 0xff94a3b8),
            fg(bold(text(std::to_string(count))), 0xffe2e8f0),
        }), 0xff1e293b), 12), 12),
        // an actual chart — the "do anything" primitive: a polyline
        round_(bg(path(chart, /*closed=*/false), 0xff0f172a), 8),
        // buttons — the ONLY interactivity concept: tap → message
        row({
            tap(round_(pad(bg(text("+"), 0xff334155), 8), 8), Inc),
            tap(round_(pad(bg(text("reset"), 0xff334155), 8), 8), Reset),
        }),
    }), 0xff0b1020), 24);
}

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }

int main() {
    std::vector<std::pair<float,float>> chart = {{0,50},{40,20},{80,35},{120,5},{160,25}};

    // ── THE PROOF: same view, two backends, ZERO change to the view code ────
    auto surface = view(42, chart);

    std::string dom    = DomBackend::render(*surface);
    std::string canvas = CanvasBackend::render(*surface);

    std::cout << "── DOM backend output ──\n" << dom.substr(0, 400) << "…\n\n";
    std::cout << "── Canvas backend output ──\n" << canvas.substr(0, 400) << "…\n\n";

    // DOM backend produced HTML/CSS...
    CHECK(has(dom, "<div"));
    CHECK(has(dom, "Dashboard"));
    CHECK(has(dom, "data-tap=\"0\""));          // the + button's message
    CHECK(has(dom, "flex-direction:row"));
    CHECK(has(dom, "<svg"));                      // the chart as SVG
    // ...and the user never wrote any of those tokens.

    // Canvas backend produced draw-ops for the SAME surface...
    CHECK(has(canvas, "\"op\":\"text\""));
    CHECK(has(canvas, "Dashboard"));
    CHECK(has(canvas, "\"op\":\"poly\""));        // the chart as a polyline draw
    CHECK(has(canvas, "\"op\":\"rect\""));
    // ...same app, totally different substrate, identical view() code.

    // ── Simplicity check: the whole vocabulary is 4 primitives + attrs ──────
    // (box, text, image, path) — everything above is composed from them.

    // ── Diff produces minimal deltas (the "byte stream" is small) ───────────
    auto s0 = view(42, chart);
    auto s1 = view(43, chart);                    // only the count changed
    Patch p = diff(*s0, *s1);
    std::cout << "── diff (42→43): " << to_json(p) << "\n";
    CHECK(p.size() == 1);                          // exactly one changed node
    CHECK(p[0].op == PatchKind::set_text);
    CHECK(p[0].value == "43");

    // change the chart data → the path node updates, nothing else
    auto s2 = view(43, {{0,10},{40,60},{80,20}});
    Patch p2 = diff(*s1, *s2);
    std::cout << "── diff (chart change): " << p2.size() << " op(s)\n";
    CHECK(p2.size() >= 1);
    CHECK(std::any_of(p2.begin(), p2.end(),
                      [](auto& o){ return o.op == PatchKind::set_path; }));

    // ── Anything: a 5000-point chart is still ONE primitive, ONE node ───────
    {
        std::vector<std::pair<float,float>> big;
        for (int i = 0; i < 5000; ++i) big.push_back({(float)i, (float)(i % 100)});
        auto huge = path(big);
        auto cv = CanvasBackend::render(*huge);
        CHECK(has(cv, "\"op\":\"poly\""));
        // 5000 points, one node, one draw-op — the canvas backend shines here,
        // and the user wrote `path(points)`. No <canvas>, no ctx, no loop.
    }

    std::cout << "\nSurface spike: " << g_pass << " passed, " << g_fail << " failed\n";
    std::cout << "  - one view(), two backends (DOM + canvas), unchanged\n";
    std::cout << "  - vocabulary is 4 primitives; composes into anything\n";
    std::cout << "  - diff streams only what changed\n";
    std::cout << "  - the user never wrote html, css, canvas, div, flex, or onclick\n";
    return g_fail ? 1 : 0;
}
