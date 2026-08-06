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

    // ── the HTTP client parses/handles a bad URL without crashing ────────────
    {
        auto r = waya::http::request({ .method="GET", .url="not-a-url", .timeout_ms=200 });
        CHECK(r.status == 0 && r.body.empty());   // graceful, non-crashing
        // https without WAYA_TLS fails cleanly (status 0), never plaintext
        auto s = waya::http::request({ .method="GET", .url="https://example.com/", .timeout_ms=200 });
        (void)s;   // status 0 when built without -DWAYA_TLS; the point is: no crash
    }

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

    // ── BROADCAST: Cmd::broadcast is data (==, map); Sub::on_topic collects ────
    CHECK((Cmd<int>::broadcast("room", "hi") == Cmd<int>::broadcast("room", "hi")));
    CHECK(!(Cmd<int>::broadcast("room", "hi") == Cmd<int>::broadcast("room", "yo")));
    CHECK(!(Cmd<int>::broadcast("a", "x") == Cmd<int>::broadcast("b", "x")));
    {
        // map preserves a broadcast unchanged (the payload isn't a Msg).
        auto lifted = Cmd<int>::broadcast("room", "hi").map([](int m){ return m + 1; });
        CHECK((lifted == Cmd<int>::broadcast("room", "hi")));
    }
    {
        auto s = Sub<int>::batch({Sub<int>::on_topic("room", [](std::string){ return 1; }),
                                  Sub<int>::on_topic("dm",   [](std::string){ return 2; })});
        auto ts = s.topics();
        CHECK(ts.size() == 2);
        CHECK(ts[0]->topic == "room" && ts[0]->on("x") == 1);
        CHECK(ts[1]->topic == "dm"   && ts[1]->on("x") == 2);
    }

    // ── The Hub fans a publish to every subscribed session (incl. sender) ─────
    {
        auto s1 = std::make_shared<detail::Session>(); s1->conn = -1;
        auto s2 = std::make_shared<detail::Session>(); s2->conn = -1;
        auto s3 = std::make_shared<detail::Session>(); s3->conn = -1;
        detail::Hub::instance().set_topics(s1, {"room"});
        detail::Hub::instance().set_topics(s2, {"room"});
        detail::Hub::instance().set_topics(s3, {"other"});   // NOT in room
        detail::Hub::instance().publish("room", "alice\nhi");
        auto d1 = s1->pop(); auto d2 = s2->pop();
        CHECK(d1 && d1->topic == "room" && d1->value == "alice\nhi");
        CHECK(d2 && d2->topic == "room" && d2->value == "alice\nhi");
        // s3 (different topic) got nothing — its queue is empty.
        { std::lock_guard<std::mutex> lk(s3->qm); CHECK(s3->queue.empty()); }
        // Leaving the topic stops delivery.
        detail::Hub::instance().set_topics(s1, {});
        detail::Hub::instance().publish("room", "bob\nyo");
        { std::lock_guard<std::mutex> lk(s1->qm); CHECK(s1->queue.empty()); }
        auto d2b = s2->pop();
        CHECK(d2b && d2b->value == "bob\nyo");   // s2 still receives
        detail::Hub::instance().remove(s2.get());
        detail::Hub::instance().remove(s3.get());
    }

    // ── SSR/HTTP helpers: request path + gzip detection ────────────────────
    CHECK(detail::request_path("GET /about?x=1 HTTP/1.1\r\n") == "/about?x=1");
    CHECK(detail::request_path("GET / HTTP/1.1\r\n") == "/");
    CHECK(detail::request_path("garbage") == "/");
    CHECK(detail::accepts_gzip("GET / HTTP/1.1\r\nAccept-Encoding: gzip, deflate\r\n"));
    CHECK(!detail::accepts_gzip("GET / HTTP/1.1\r\nAccept-Encoding: identity\r\n"));

    // ── ERROR BOUNDARY: a throwing view()/update() is contained, not fatal ────
    {
        struct Boom {
            struct Model { int n=0; };
            using Msg = int;
            static Model init(){ return {}; }
            static Model update(Model m, Msg){ throw std::runtime_error("x"); return m; }
            static NodeRef view(const Model& m){ if(m.n==7) throw std::runtime_error("boom"); return text("ok"); }
        };
        auto good = detail::safe_view<Boom>(Boom::Model{0});
        CHECK(good && good->text == "ok");
        auto bad = detail::safe_view<Boom>(Boom::Model{7});   // would throw
        CHECK(bad != nullptr && bad->kind == Kind::markup);   // error card, not a crash
        bool ok=true;
        auto [m2, c2] = detail::safe_dispatch<Boom>(Boom::Model{3}, 0, std::string{}, ok);
        CHECK(!ok);
        CHECK(m2.n == 3);                                     // model unchanged on throw
        CHECK((c2 == Cmd<int>::none()));
    }

    // ── SSR routing: the path picks the right screen at first-paint time ─────
    {
        struct App {
            struct Model { std::string route="/"; };
            using Msg = int;
            static Model init(){ return {}; }
            static std::pair<Model,Cmd<Msg>> update(Model m, Msg, std::string p){ m.route=p; return {m, Cmd<Msg>::none()}; }
            static Sub<Msg> subscribe(const Model&){ return Sub<Msg>::on_route([](std::string){ return 1; }); }
            static NodeRef view(const Model& m){ return text(m.route); }
        };
        auto [m0, c0] = detail::init_of<App, App::Model, App::Msg>();
        auto sub = detail::subs_of<App, App::Model, App::Msg>(m0);
        auto* rt = sub.route(); CHECK(rt != nullptr);
        bool ok=true;
        auto r = detail::safe_dispatch<App>(std::move(m0), rt->route("/about"), "/about", ok);
        auto node = detail::safe_view<App>(r.first);
        CHECK(node->text == "/about");     // SSR renders the requested route's screen
    }

    std::cerr << (g_fail ? "SOME TESTS FAILED\n" : "all effect tests passed\n");
    std::cerr << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
