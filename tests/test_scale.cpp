/// tests/test_scale.cpp — the "huge app stays easy" layer: URL router with
/// params/wildcards, and combine()/feature() msg-block composition so features
/// don't collide and the top-level update stays a table.

#include <waya/surface/router.hpp>
#include <waya/surface/scale.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/dom.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }
static std::string css_of(const NodeRef& n){ return DomBackend{}.render(*n).css; }

enum { Home, Users, UserView, UserEdit, Docs, NotFound };

int main() {
    // ═══ ROUTER ════════════════════════════════════════════════════════════════
    auto r = router()
        .at("/",               Home)
        .at("/users",          Users)
        .at("/users/:id",      UserView)
        .at("/users/:id/edit", UserEdit)
        .at("/docs/*",         Docs);
    CHECK(r.size() == 5);

    // exact + literal matches
    CHECK(r.match("/").value == Home && r.match("/").matched);
    CHECK(r.match("/users").value == Users);
    // param capture
    { auto m = r.match("/users/42"); CHECK(m.matched && m.value==UserView && m.param("id")=="42"); }
    { auto m = r.match("/users/7/edit"); CHECK(m.value==UserEdit && m.param("id")=="7"); }
    // query string + trailing slash ignored
    CHECK(r.match("/users/9?tab=x").param("id") == "9");
    CHECK(r.match("/users/9/").param("id") == "9");
    // wildcard tail
    { auto m = r.match("/docs/guide/intro"); CHECK(m.value==Docs && m.param("*")=="guide/intro"); }
    // no match
    CHECK(!r.match("/nope").matched);
    // first-match-wins ordering
    { auto rr = router().at("/a/:x", 1).at("/a/b", 2); CHECK(rr.match("/a/b").value == 1); }
    // missing param is empty, not a crash
    CHECK(r.match("/users").param("id").empty());

    // ═══ screens(): route id → view ═══════════════════════════════════════════
    {
        auto pick = [](int id){
            return screens(id, {
                {Home,     []{ return text("home"); }},
                {Users,    []{ return text("users"); }},
                {NotFound, []{ return text("404"); }},
            });
        };
        CHECK(pick(Home)->text == "home");
        CHECK(pick(Users)->text == "users");
        CHECK(pick(NotFound)->text == "404");
        CHECK(pick(999)->kind == Kind::box);   // unknown → empty box
    }

    // ═══ combine()/feature(): msg-block composition, no collisions ════════════
    {
        struct M { int auth = 0, cart = 0; std::string last; };
        enum : int { AuthBase = 100, CartBase = 200 };
        // each feature sees LOCAL msg ids (0,1,…) relative to its base
        auto auth = feature<M>(AuthBase, [](M& m, int local, const std::string& v) -> Cmd<int> {
            if (local == 0) m.auth++;          // login
            if (local == 1) { m.auth = 0; m.last = v; }  // logout carries a value
            return Cmd<int>::none();
        });
        auto cart = feature<M>(CartBase, [](M& m, int local) -> Cmd<int> {
            if (local == 0) m.cart++;          // add
            return Cmd<int>::emit(CartBase + 5);
        });

        // ownership is by range
        CHECK(auth.owns(100) && auth.owns(199) && !auth.owns(200));
        CHECK(cart.owns(200) && !cart.owns(100));

        // route msg 100 (Auth login local 0) → only auth handles it
        { auto [m,c] = combine(M{}, 100, std::string{}, auth, cart);
          CHECK(m.auth==1 && m.cart==0); CHECK((c==Cmd<int>::none())); }
        // route msg 200 (Cart add local 0) → only cart, and its Cmd is returned
        { auto [m,c] = combine(M{}, 200, std::string{}, auth, cart);
          CHECK(m.cart==1 && m.auth==0); CHECK((c==Cmd<int>::emit(CartBase+5))); }
        // value threads through to the owning feature (Auth logout local 1)
        { auto [m,c] = combine(M{5,0,""}, 101, std::string{"bye"}, auth, cart);
          CHECK(m.auth==0 && m.last=="bye"); }
        // an unowned msg is a harmless no-op (no feature owns 999)
        { auto [m,c] = combine(M{3,4,""}, 999, std::string{}, auth, cart);
          CHECK(m.auth==3 && m.cart==4); CHECK((c==Cmd<int>::none())); }
        // value-less overload
        { auto [m,c] = combine(M{}, 100, auth, cart); CHECK(m.auth==1); }
    }

    std::cout << "test_scale: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
