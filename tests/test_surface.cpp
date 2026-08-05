/// tests/test_surface.cpp — the Surface Model: primitives, the full style
/// vocabulary (box model, flex, position, effects, states, universal css),
/// minimal diff, and the wire format.

#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/diff.hpp>
#include <waya/surface/wire.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }

enum { Inc, Reset };

static NodeRef view(int n) {
    return col(
        text("Count") | fg(0x3b82f6) | font(28) | bold,
        text(n) | fg(0xe2e8f0),
        path({{0,50},{40,20},{80,35}}) | stroke(0x22d3ee, 2),
        text("+") | pad(8) | bg(0x334155) | round(8) | tap(Inc)
    ) | gap(16) | pad(24) | center;
}

int main() {
    // ── the vocabulary builds a tree; the user wrote no HTML ────────────────
    auto s = view(42);
    CHECK(s->kind == Kind::box);
    CHECK(s->style.flow == Flow::col);
    CHECK(s->kids.size() == 4);
    CHECK(s->kids[2]->kind == Kind::path && s->kids[2]->points.size() == 3);
    CHECK(s->kids[3]->on_tap == Inc);

    // ── the full style vocabulary reaches CSS ───────────────────────────────
    {
        auto node = box(text("x"))
            | pad(12) | pad_x(20) | margin(8) | w(px(200)) | h(pct(50))
            | round(12) | border(2, 0x334155) | shadow() | opacity(0.9f)
            | justify(Justify::between) | align(Align::center) | wrap
            | absolute(px(10), px(20)) | z(5)
            | css("backdrop-filter", "blur(8px)")            // universal channel
            | on(Hover, bg(0x1e293b))                        // state
            | at(Md, w(fill));                               // breakpoint
        auto css = DomBackend{}.render(*node).css;
        CHECK(has(css, "padding:12px"));
        CHECK(has(css, "width:200px"));
        CHECK(has(css, "height:50%"));
        CHECK(has(css, "border-radius:12px"));
        CHECK(has(css, "border:2px solid"));
        CHECK(has(css, "box-shadow:"));
        CHECK(has(css, "opacity:0.9"));
        CHECK(has(css, "justify-content:space-between"));
        CHECK(has(css, "align-items:center"));
        CHECK(has(css, "flex-wrap:wrap"));
        CHECK(has(css, "position:absolute"));
        CHECK(has(css, "z-index:5"));
        CHECK(has(css, "backdrop-filter:blur(8px)"));         // universal css
        CHECK(has(css, ":hover{"));                           // state
        CHECK(has(css, "@media(min-width:768px)"));           // breakpoint
    }

    // ── DOM backend: HTML + interned classes + svg for paths ────────────────
    {
        auto out = DomBackend{}.render(*s);
        CHECK(has(out.html, "<div class=\"ws-"));
        CHECK(has(out.html, "<span"));
        CHECK(has(out.html, "Count"));
        CHECK(has(out.html, "<svg"));
        CHECK(has(out.html, "data-tap=\"0\""));
        CHECK(has(out.css, "flex-direction:column"));
        // interning: identical styles collapse
        auto s2 = box(text("a") | pad(8), text("b") | pad(8));
        auto css2 = DomBackend{}.render(*s2).css;
        int rules = 0; for (std::size_t p=0;(p=css2.find(".ws-",p))!=std::string::npos;++p) ++rules;
        CHECK(rules <= 3);
    }

    // ── diff: count change = ONE op via the subtree hash fast-path ──────────
    {
        auto p = diff(view(42), view(43));
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_text);
        CHECK(p[0].s == "43");
        CHECK(p[0].path == "1");
    }
    CHECK(diff(view(7), view(7)).empty());

    // ── a style-only change diffs as set_paint ──────────────────────────────
    {
        auto a = text("x") | fg(0x111111);
        auto b = text("x") | fg(0x222222);
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_paint);
    }

    // ── wire: patch JSON carries {css, ops} ─────────────────────────────────
    {
        auto j = patch_json(diff(view(1), view(2)));
        CHECK(has(j, "\"css\":"));
        CHECK(has(j, "\"ops\":[[0,\"1\",\"2\"]]"));
    }

    // ── "anything": a 5000-point chart is ONE node ──────────────────────────
    {
        std::vector<Pt> big; for (int i=0;i<5000;++i) big.push_back({(float)i,(float)(i%100)});
        auto huge = path(big);
        CHECK(huge->points.size() == 5000 && huge->kids.empty());
    }

    std::cout << "test_surface: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
