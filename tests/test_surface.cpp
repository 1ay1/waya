/// tests/test_surface.cpp — the Surface Model in the framework: 4 primitives,
/// substrate-free view, DOM backend, minimal diff, wire format.

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
    return col({
        text("Count") | fg(0x3b82f6) | size(28) | bold,
        text(n) | fg(0xe2e8f0),
        path({{0,50},{40,20},{80,35}}) | fg(0x22d3ee),
        text("+") | pad(8) | bg(0x334155) | tap(Inc),
    }) | gap(16) | pad(24);
}

int main() {
    // ── the vocabulary builds a tree; the user wrote no HTML ────────────────
    auto s = view(42);
    CHECK(s->kind == Kind::box);
    CHECK(s->flow == Flow::col);
    CHECK(s->kids.size() == 4);
    CHECK(s->kids[0]->kind == Kind::text && s->kids[0]->text == "Count");
    CHECK(s->kids[2]->kind == Kind::path && s->kids[2]->points.size() == 3);
    CHECK(s->kids[3]->on_tap == Inc);

    // ── DOM backend produces HTML + interned CSS ────────────────────────────
    {
        DomBackend dom;
        auto out = dom.render(*s);
        CHECK(has(out.html, "<div class=\"ws-"));
        CHECK(has(out.html, "<span"));
        CHECK(has(out.html, "Count"));
        CHECK(has(out.html, "<svg"));                 // the path → svg
        CHECK(has(out.html, "data-tap=\"0\""));        // the tap message
        CHECK(has(out.css, "flex-direction:column"));
        CHECK(has(out.css, "color:#3b82f6"));
        // interning: identical styles share a class
        auto s2 = box({ text("a") | pad(8), text("b") | pad(8) });
        auto o2 = DomBackend{}.render(*s2);
        int rules = 0;
        for (std::size_t p=0; (p=o2.css.find(".ws-",p))!=std::string::npos; ++p) ++rules;
        CHECK(rules <= 3);                              // both text pads collapse
    }

    // ── diff: count change = ONE op via the subtree hash fast-path ──────────
    {
        auto p = diff(view(42), view(43));
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_text);
        CHECK(p[0].s == "43");
        CHECK(p[0].path == "1");                        // second child
    }

    // ── identical surfaces → empty diff ─────────────────────────────────────
    CHECK(diff(view(7), view(7)).empty());

    // ── path change → set_path op ───────────────────────────────────────────
    {
        auto a = path({{0,0},{1,1}});
        auto b = path({{0,0},{2,2}});
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_path);
    }

    // ── wire format: node JSON round-trips the essentials ───────────────────
    {
        auto j = node_json(*view(5));
        CHECK(has(j, "\"k\":0"));                        // box
        CHECK(has(j, "\"t\":\"Count\""));
        CHECK(has(j, "\"tap\":0"));
        CHECK(has(j, "\"p\":[[0,50]"));                  // path points
    }

    // ── patch JSON is compact ───────────────────────────────────────────────
    {
        auto p = diff(view(1), view(2));
        auto j = patch_json(p);
        CHECK(j == "[[0,\"1\",\"2\"]]");
    }

    // ── "anything": a 5000-point chart is ONE node ──────────────────────────
    {
        std::vector<Pt> big;
        for (int i = 0; i < 5000; ++i) big.push_back({(float)i, (float)(i%100)});
        auto huge = path(big);
        CHECK(huge->kind == Kind::path);
        CHECK(huge->points.size() == 5000);
        CHECK(huge->kids.empty());                       // one node, no children
    }

    std::cout << "test_surface: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
