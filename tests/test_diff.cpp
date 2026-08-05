/// tests/test_diff.cpp — the DOM diff engine (the maya cell-diff, on the DOM).
/// The correctness backbone: for any two renders, diff(prev,next) is minimal
/// AND sound — applying it to prev's HTML yields next's HTML.

#include <waya/waya.hpp>
#include <waya/render/vwalk.hpp>
#include <waya/render/diff.hpp>

#include <iostream>
#include <algorithm>
#include <string>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;
using namespace waya::render;
using namespace waya::vdom;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

// Soundness is checked with the framework's canonical `waya::vdom::apply` — the
// SAME implementation the live runtime and the JS client mirror. No private
// re-implementation here: one applier, one source of truth.

template <typename V>
static VNode vn(const V& node) { StyleSheet s; return to_vnode(node, s); }

template <typename V>
static std::string html_of(const V& node) { StyleSheet s; return vnode_to_html(to_vnode(node, s)); }

int main() {
    // ── identical renders → empty patch ─────────────────────────────────────
    {
        auto a = vn(div_(text("hello")));
        auto b = vn(div_(text("hello")));
        CHECK(diff(a, b).empty());
    }

    // ── text change → exactly one set_text op ───────────────────────────────
    {
        auto a = vn(h1_(text("Count: 0")));
        auto b = vn(h1_(text("Count: 1")));
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_text);
        CHECK(p[0].a == "Count: 1");
    }

    // ── the patch is TINY vs full HTML (the whole point) ────────────────────
    {
        auto a = vn(div_(h1_(text("Count: 0")), p_(text("some long paragraph here"))));
        auto b = vn(div_(h1_(text("Count: 1")), p_(text("some long paragraph here"))));
        std::string json = to_json(diff(a, b));
        std::string full = vnode_to_html(b);
        CHECK(json.size() < full.size() / 2);   // patch is a fraction of the page
    }

    // ── attribute change → set_attr op ──────────────────────────────────────
    {
        auto a = vn(div_(text("x")) | attr<"data-n", "0">);
        auto b = vn(div_(text("x")) | attr<"data-n", "5">);
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_attr);
        CHECK(p[0].a == "data-n" && p[0].b == "5");
    }

    // ── attribute removed → remove_attr ─────────────────────────────────────
    {
        auto a = vn(div_(text("x")) | attr<"role", "tab">);
        auto b = vn(div_(text("x")));
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::remove_attr);
        CHECK(p[0].a == "role");
    }

    // ── tag change → replace ────────────────────────────────────────────────
    {
        auto a = vn(div_(span_(text("x"))));
        auto b = vn(div_(p_(text("x"))));
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::replace);
    }

    // ── soundness: apply(diff(a,b), a) yields b (attr/text ops) ─────────────
    {
        auto a = vn(div_(h1_(text("A")), span_(text("x")) | attr<"data-k", "1">));
        auto b = vn(div_(h1_(text("B")), span_(text("x")) | attr<"data-k", "2">));
        VNode work = a;
        apply(work, diff(a, b));
        CHECK(vnode_to_html(work) == vnode_to_html(b));
    }

    // ── JSON wire format is well-formed and compact ─────────────────────────
    {
        auto a = vn(h1_(text("0")));
        auto b = vn(h1_(text("1")));
        std::string j = to_json(diff(a, b));
        CHECK(j == "[[0,\"0\",\"1\"]]");
    }

    // ── deep path addressing ────────────────────────────────────────────────
    {
        auto a = vn(div_(div_(div_(span_(text("old"))))));
        auto b = vn(div_(div_(div_(span_(text("new"))))));
        auto p = diff(a, b);
        CHECK(p.size() == 1);
        CHECK(p[0].path == "0.0.0.0");   // 4 levels deep
        CHECK(p[0].a == "new");
    }

    std::cout << "test_diff: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
