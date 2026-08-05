/// tests/test_memo.cpp — subtree memoisation (maya's component cache): a keyed
/// list re-runs a row's view callback ONLY when the row's key changes.

#include <waya/waya.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace waya;
using namespace waya::dsl;
using namespace waya::style;
using namespace waya::render;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

struct Row { int id; std::string name; int ms; };

int main() {
    // CacheId basics: distinct inputs → distinct ids; same → same.
    CHECK(cache_id("row", 1, 23) == cache_id("row", 1, 23));
    CHECK(cache_id("row", 1, 23) != cache_id("row", 12, 3));   // no tag collision
    CHECK(cache_id("row", 1) != cache_id("row", 2));
    CHECK(!cache_id("x").empty());

    std::vector<Row> rows;
    for (int i = 0; i < 100; ++i) rows.push_back({i, "s" + std::to_string(i), i});

    int calls = 0;
    auto make = [&] {
        return table_(tbody_(each_keyed(rows,
            [](const Row& r) { return cache_id("row", r.id, r.ms); },
            [&](const Row& r) { ++calls; return tr_(td_(text(r.name)), td_(text(r.ms))); })));
    };

    MemoCache cache;
    active_memo = &cache;

    // Frame 1: cold cache → every row's view runs.
    calls = 0; { StyleSheet s; auto v = to_vnode(make(), s); (void)v; } cache.rotate();
    CHECK(calls == 100);

    // Frame 2: nothing changed → ZERO view callbacks (all cached).
    calls = 0; { StyleSheet s; auto v = to_vnode(make(), s); (void)v; } cache.rotate();
    CHECK(calls == 0);

    // Frame 3: change one row → exactly ONE view callback.
    rows[50].ms = 99999;
    calls = 0; { StyleSheet s; auto v = to_vnode(make(), s); (void)v; } cache.rotate();
    CHECK(calls == 1);

    // Frame 4: append a row → one new callback (the appended row).
    rows.push_back({100, "new", 100});
    calls = 0; { StyleSheet s; auto v = to_vnode(make(), s); (void)v; } cache.rotate();
    CHECK(calls == 1);

    active_memo = nullptr;

    // Without a cache (plain SSR) every row builds — memo is a no-op, not a bug.
    calls = 0; { StyleSheet s; auto v = to_vnode(make(), s); (void)v; }
    CHECK(calls == 101);

    // Correctness: memoised output equals non-memoised output.
    {
        MemoCache c; active_memo = &c;
        StyleSheet s1; auto memoed = to_vnode(make(), s1); c.rotate();
        active_memo = nullptr;
        StyleSheet s2; auto plain = to_vnode(make(), s2);
        CHECK(vdom::vnode_to_html(memoed) == vdom::vnode_to_html(plain));
    }

    std::cout << "test_memo: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
