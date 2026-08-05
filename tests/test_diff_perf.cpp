/// tests/test_diff_perf.cpp — locks in the maya-fast guarantees: a single
/// change in a big list produces a MINIMAL patch (one op), because per-item
/// subtree hashes let the diff skip everything unchanged in O(1).

#include <waya/waya.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::render;
using namespace waya::vdom;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

struct Row { int id; std::string name; int ms; };

static auto tbl(const std::vector<Row>& r) {
    return table_(tbody_(each(r, [](const Row& x) {
        return tr_(td_(text(x.name)), td_(text(x.ms)));
    })));
}

template <typename V> static VNode vn(const V& n) { StyleSheet s; return to_vnode(n, s); }

int main() {
    std::vector<Row> a, b;
    for (int i = 0; i < 1000; ++i) { a.push_back({i, "s" + std::to_string(i), i}); }
    b = a;

    // ── identical big lists → EMPTY patch (all hashes match) ────────────────
    {
        auto p = diff(vn(tbl(a)), vn(tbl(b)));
        CHECK(p.empty());
    }

    // ── one cell changed in 1000 rows → exactly ONE op ──────────────────────
    {
        b[500].ms = 99999;
        auto va = vn(tbl(a)), vb = vn(tbl(b));
        auto p = diff(va, vb);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_text);
        CHECK(p[0].a == "99999");
        // the patch is a tiny fraction of the full table HTML
        std::string json = to_json(p);
        std::string full = vnode_to_html(vb);
        CHECK(json.size() < 64);
        CHECK(json.size() * 100 < full.size());   // <1% of the page
    }

    // ── per-item hashes are stable and distinct ─────────────────────────────
    {
        auto va = vn(tbl(a));
        // tbody is va.kids[0]; its rows each have their own hash
        const auto& tbody = va.kids[0];
        CHECK(tbody.kids.size() == 1000);
        CHECK(tbody.kids[0].hash != 0);
        CHECK(tbody.kids[0].hash != tbody.kids[1].hash);   // different content
    }

    // ── a row inserted at the end → one insert op, not a full re-diff ───────
    {
        std::vector<Row> c = a;
        c.push_back({1000, "new", 1000});
        auto p = diff(vn(tbl(a)), vn(tbl(c)));
        // exactly one structural op (insert) at the tbody
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::insert);
    }

    std::cout << "test_diff_perf: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
