/// tests/test_effect.cpp — the surface effect system: Cmd<Msg> and Sub<Msg> as
/// data (maya's insight, transposed to the browser). These are pure values, so
/// the whole effect layer is testable with `==` — no socket, no server, no
/// timers actually firing. That is the point: `update` returns a *description*
/// of I/O, and here we assert the description is exactly what we expect.

#include <waya/surface/effect.hpp>
#include <waya/surface/live.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)

enum Ev { Tick = 1, Load = 2, Loaded = 3, Go = 4 };

int main() {
    // ── Cmd equality: the reason update() is testable ────────────────────────
    CHECK((Cmd<int>::none()  == Cmd<int>::none()));
    CHECK((Cmd<int>::quit()  == Cmd<int>::quit()));
    CHECK(!(Cmd<int>::none() == Cmd<int>::quit()));
    CHECK((Cmd<int>::emit(Tick)  == Cmd<int>::emit(Tick)));
    CHECK(!(Cmd<int>::emit(Tick) == Cmd<int>::emit(Load)));
    CHECK((Cmd<int>::after(1000ms, Tick) == Cmd<int>::after(1000ms, Tick)));
    CHECK(!(Cmd<int>::after(1000ms, Tick) == Cmd<int>::after(500ms, Tick)));
    CHECK(!(Cmd<int>::after(1000ms, Tick) == Cmd<int>::after(1000ms, Load)));
    CHECK((Cmd<int>::after(1000, Tick) == Cmd<int>::after(1000ms, Tick))); // long overload
    CHECK((Cmd<int>::navigate("/a") == Cmd<int>::navigate("/a")));
    CHECK(!(Cmd<int>::navigate("/a") == Cmd<int>::navigate("/b")));
    CHECK(!(Cmd<int>::navigate("/a", true) == Cmd<int>::navigate("/a", false)));
    CHECK((Cmd<int>::push_url("/x") == Cmd<int>::push_url("/x")));

    // ── batch normalization: flatten nested, strip None, collapse singletons ─
    CHECK((Cmd<int>::batch({}) == Cmd<int>::none()));                 // empty → none
    CHECK((Cmd<int>::batch({Cmd<int>::none()}) == Cmd<int>::none())); // all-none → none
    CHECK((Cmd<int>::batch({Cmd<int>::emit(Tick)}) == Cmd<int>::emit(Tick))); // singleton unwrap
    {
        // nested batch flattens; order preserved; None stripped
        auto a = Cmd<int>::batch({Cmd<int>::emit(Tick), Cmd<int>::none(),
                                  Cmd<int>::batch({Cmd<int>::emit(Load), Cmd<int>::navigate("/z")})});
        auto b = Cmd<int>::batch({Cmd<int>::emit(Tick), Cmd<int>::emit(Load), Cmd<int>::navigate("/z")});
        CHECK((a == b));
        // variadic batch == vector batch
        auto c = Cmd<int>::batch(Cmd<int>::emit(Tick), Cmd<int>::emit(Load), Cmd<int>::navigate("/z"));
        CHECK((a == c));
    }

    // ── Cmd::map — embed a child's Msg into a parent's ───────────────────────
    {
        auto child = Cmd<int>::batch({Cmd<int>::emit(1), Cmd<int>::after(50ms, 2), Cmd<int>::navigate("/n")});
        auto lifted = child.map([](int m){ return m + 100; });
        auto want = Cmd<int>::batch({Cmd<int>::emit(101), Cmd<int>::after(50ms, 102), Cmd<int>::navigate("/n")});
        CHECK((lifted == want));  // navigate is untouched by the mapper
    }

    // ── Sub equality via collected timers + route ────────────────────────────
    CHECK(Sub<int>::none().timers().empty());
    CHECK(Sub<int>::none().is_none());
    {
        auto s = Sub<int>::every(1000ms, Tick);
        CHECK(s.timers().size() == 1);
        CHECK(s.timers()[0].interval == 1000ms);
        CHECK(s.timers()[0].msg == Tick);
        CHECK(s.route() == nullptr);
    }
    {
        // batch collects every timer, flattens nested, drops None
        auto s = Sub<int>::batch({Sub<int>::every(16ms, Tick), Sub<int>::none(),
                                  Sub<int>::batch({Sub<int>::every(1000ms, Load)})});
        CHECK(s.timers().size() == 2);
        // long overload equivalence
        CHECK(Sub<int>::every(16, Tick).timers()[0].interval == 16ms);
    }
    {
        // on_route is discoverable and maps a path to a Msg
        auto s = Sub<int>::batch({Sub<int>::every(1000ms, Tick),
                                  Sub<int>::on_route([](std::string p){ return p == "/x" ? Load : Go; })});
        CHECK(s.timers().size() == 1);
        auto* r = s.route();
        CHECK(r != nullptr);
        CHECK(r->route("/x") == Load);
        CHECK(r->route("/y") == Go);
    }
    {
        // Sub::map lifts the emitted Msg
        auto lifted = Sub<int>::every(20ms, 5).map([](int m){ return m * 10; });
        CHECK(lifted.timers()[0].msg == 50);
    }

    // ── dispatch / init helpers pick the right update shape ───────────────────
    // Program A: pure Model-returning update (simple apps stay simple).
    struct A {
        struct Model { int n = 0; };
        using Msg = int;
        static Model init() { return {7}; }
        static Model update(Model m, Msg msg) { return {m.n + msg}; }
    };
    {
        auto [m0, c0] = detail::init_of<A, A::Model, A::Msg>();
        CHECK(m0.n == 7);
        CHECK((c0 == Cmd<int>::none()));   // no init effect
        auto [m1, c1] = detail::dispatch<A>(A::Model{7}, 5, std::string{});
        CHECK(m1.n == 12);
        CHECK((c1 == Cmd<int>::none()));   // pure update → none effect
    }

    // Program B: effectful update returning (Model, Cmd), + init effect.
    struct B {
        struct Model { int n = 0; };
        using Msg = int;
        static std::pair<Model, Cmd<Msg>> init() { return {Model{0}, Cmd<Msg>::emit(Tick)}; }
        static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
            if (msg == Tick) return { {m.n + 1}, Cmd<Msg>::after(1000ms, Tick) };
            if (msg == Go)   return { m, Cmd<Msg>::navigate("/next") };
            return { m, Cmd<Msg>::none() };
        }
    };
    {
        auto [m0, c0] = detail::init_of<B, B::Model, B::Msg>();
        CHECK(m0.n == 0);
        CHECK((c0 == Cmd<int>::emit(Tick)));               // startup command
        auto [m1, c1] = detail::dispatch<B>(B::Model{0}, int{Tick}, std::string{});
        CHECK(m1.n == 1);
        CHECK((c1 == Cmd<int>::after(1000ms, Tick)));       // clock keeps ticking
        auto [m2, c2] = detail::dispatch<B>(B::Model{1}, int{Go}, std::string{});
        CHECK((c2 == Cmd<int>::navigate("/next")));         // routing effect
    }

    // Program C: value-carrying effectful update (inputs) + subscribe.
    struct C {
        struct Model { std::string text; bool live = false; };
        using Msg = int;
        static Model init() { return {}; }
        static std::pair<Model, Cmd<Msg>> update(Model m, Msg, std::string value) {
            return { {value, m.live}, Cmd<Msg>::none() };
        }
        static Sub<Msg> subscribe(const Model& m) {
            return m.live ? Sub<Msg>::every(500ms, Tick) : Sub<Msg>::none();
        }
    };
    {
        auto [m1, c1] = detail::dispatch<C>(C::Model{}, 1, std::string{"hello"});
        CHECK(m1.text == "hello");                          // value threaded through
        CHECK((detail::subs_of<C, C::Model, C::Msg>(C::Model{"", false}).is_none()));
        CHECK((detail::subs_of<C, C::Model, C::Msg>(C::Model{"", true}).timers().size() == 1));
    }

    // ── http_get parses the URL without crashing on garbage ──────────────────
    CHECK(detail::http_get("not-a-url").empty());           // graceful, non-crashing

    // ── ROUTING: on_route maps a path to a Msg, and the runtime feeds the SAME
    //    path to update() as its value. (Regression: the value used to be
    //    dropped, so route-driven screens never changed.) We reproduce the
    //    runtime's route branch: pick the Msg via on_route, dispatch with the
    //    path as value.
    {
        struct R {
            struct Model { std::string route = "/"; };
            using Msg = int;
            static Model init() { return {}; }
            static std::pair<Model, Cmd<Msg>> update(Model m, Msg, std::string path) {
                m.route = path; return { m, Cmd<Msg>::none() };
            }
            static Sub<Msg> subscribe(const Model&) {
                return Sub<Msg>::on_route([](std::string){ return 9; });
            }
        };
        auto sub = detail::subs_of<R, R::Model, R::Msg>(R::Model{});
        auto* rt = sub.route();
        CHECK(rt != nullptr);
        std::string path = "/about";
        auto [m1, c1] = detail::dispatch<R>(R::Model{}, rt->route(path), path);
        CHECK(m1.route == "/about");   // the path reached update as the value
    }

    std::cerr << (g_fail ? "SOME TESTS FAILED\n" : "all effect tests passed\n");
    std::cerr << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
