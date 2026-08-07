// tests/test_component.cpp — reusable components: fast (memoised) and correct
// (memo + keyed diff produce minimal, aliasing-free deltas).
#include <waya/surface/live.hpp>
#include <waya/surface/component.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }

int main() {
    // ── memo skips the build when props are unchanged ────────────────────────
    {
        int builds = 0;
        auto make = [&](int id, std::string name){
            return memo(id, name, [&]{ ++builds; return text(name) | pad(8); });
        };
        auto a = make(1, "Ada");   // build
        auto b = make(1, "Ada");   // cached
        auto c = make(1, "Bob");   // prop changed -> build
        check(builds == 2, "memo builds only on prop change");
        check(a == b, "memo returns the same node when props unchanged");
        check(a != c, "memo returns a fresh node when props change");
    }

    // ── float / bool / enum / Color props all discriminate ───────────────────
    {
        int builds = 0;
        auto f = [&](float v){ return memo(v, [&]{ ++builds; return box(); }); };
        f(1.0f); f(1.0f); f(2.0f);
        check(builds == 2, "float prop discriminates");
    }
    {
        int builds = 0;
        auto f = [&](bool v){ return memo(v, [&]{ ++builds; return box(); }); };
        f(true); f(true); f(false);
        check(builds == 2, "bool prop discriminates");
    }

    // ── component() wrapper: auto-memoised by props ──────────────────────────
    {
        int builds = 0;
        auto Avatar = component([&](std::string url, int size){
            ++builds; return image(url) | w(size) | h(size) | round(999);
        });
        Avatar("/a.png", 40); Avatar("/a.png", 40); Avatar("/b.png", 40); Avatar("/a.png", 64);
        check(builds == 3, "component memoises by its props");
    }

    // ── memo + keyed diff produce minimal, correct deltas ────────────────────
    struct It { int id; std::string t; bool done; };
    auto rowof = [](const It& it){
        return memo(it.id, it.t, it.done, [&]{
            return row(text(it.t + (it.done ? " (done)" : ""))) | key("r" + std::to_string(it.id));
        });
    };
    auto mklist = [&](std::vector<It> v){
        std::vector<NodeRef> r; for (auto& i : v) r.push_back(rowof(i));
        auto l = col(); l->kids = std::move(r); finalize(*l); return l;
    };

    std::vector<It> base{{1,"A",false},{2,"B",false},{3,"C",false}};
    { NodeRef a = mklist(base), b = mklist(base);
      check(diff(a, b).empty(), "identical memoised list -> zero ops"); }
    { NodeRef a = mklist(base);
      NodeRef b = mklist({{1,"A",false},{2,"B",true},{3,"C",false}});
      auto p = diff(a, b);
      check(p.size() == 1, "one item toggled -> exactly one op"); }
    { NodeRef a = mklist(base);
      NodeRef b = mklist({{3,"C",false},{1,"A",false},{2,"B",false}});   // reorder
      auto p = diff(a, b);
      check(p.size() == 1 && p[0].op == Op::move, "keyed reorder -> one move op"); }
    { NodeRef a = mklist(base);
      NodeRef b = mklist({{1,"A",false},{2,"B",false},{3,"C",false},{4,"D",false}}); // append
      auto p = diff(a, b);
      check(p.size() == 1 && p[0].op == Op::insert, "keyed append -> one insert op"); }
    { NodeRef a = mklist(base);
      NodeRef b = mklist({{1,"A",false},{3,"C",false}});                 // remove middle
      auto p = diff(a, b);
      check(p.size() == 1 && p[0].op == Op::remove, "keyed remove -> one remove op"); }

    // ── animated() marks a node for FLIP by its key ──────────────────────────
    {
        auto n = row(text("x")) | key("k7") | animated();
        auto html = DomBackend{}.render(*n).html;
        check(html.find("data-wa-flip=\"k7\"") != std::string::npos, "animated() emits FLIP key");
    }

    // ── list(): memoised keyed container ─────────────────────────────────────
    {
        struct It { int id; std::string t; };
        auto keyf = [](const It& i){ return std::to_string(i.id); };
        auto viewf = [](const It& i){ return row(text(i.t)); };
        std::vector<It> base{{1,"A"},{2,"B"},{3,"C"}};

        detail::begin_msg_capture(); detail::memo_begin_frame();
        NodeRef a = list(0, base, keyf, viewf);
        detail::begin_msg_capture(); detail::memo_begin_frame();
        NodeRef b = list(0, base, keyf, viewf);
        check(a.get() == b.get(), "list(): unchanged data returns the SAME cached container");
        check(diff(a, b).empty(), "list(): unchanged -> zero diff ops");

        detail::begin_msg_capture(); detail::memo_begin_frame();
        std::vector<It> changed{{1,"A"},{2,"B2"},{3,"C"}};
        NodeRef c = list(0, changed, keyf, viewf);
        check(c.get() != a.get(), "list(): changed data rebuilds the container");
        auto p = diff(a, c);
        check(p.size() == 1, "list(): one row changed -> exactly one op");
    }

    // ── list_versioned(): O(1) skip when version is unchanged ────────────────
    {
        struct It { int id; std::string t; };
        auto keyf = [](const It& i){ return std::to_string(i.id); };
        int builds = 0;
        auto viewf = [&](const It& i){ ++builds; return row(text(i.t)); };
        std::vector<It> data{{1,"A"},{2,"B"}};

        detail::begin_msg_capture(); detail::memo_begin_frame();
        NodeRef a = list_versioned(0, /*version*/5, data, keyf, viewf);
        int after_first = builds;
        detail::begin_msg_capture(); detail::memo_begin_frame();
        NodeRef b = list_versioned(0, /*version*/5, data, keyf, viewf);   // same version
        check(builds == after_first, "list_versioned(): same version does NOT rebuild rows (O(1))");
        check(a.get() == b.get(), "list_versioned(): same version returns cached container");

        detail::begin_msg_capture(); detail::memo_begin_frame();
        NodeRef c = list_versioned(0, /*version*/6, data, keyf, viewf);   // bumped version
        check(builds > after_first, "list_versioned(): bumped version rebuilds");
        check(c.get() != a.get(), "list_versioned(): bumped version returns a fresh container");
    }

    std::cout << "test_component: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
