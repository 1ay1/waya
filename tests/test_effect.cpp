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

    // ── browser effects are data: equality + functor map ──────────────────
    CHECK((Cmd<int>::set_title("Inbox (3)") == Cmd<int>::set_title("Inbox (3)")));
    CHECK(!(Cmd<int>::set_title("a") == Cmd<int>::set_title("b")));
    CHECK((Cmd<int>::scroll_to("chat-end") == Cmd<int>::scroll_to("chat-end")));
    CHECK(!(Cmd<int>::scroll_to("x", true) == Cmd<int>::scroll_to("x", false)));
    CHECK((Cmd<int>::focus("email") == Cmd<int>::focus("email")));
    CHECK(!(Cmd<int>::focus("email") == Cmd<int>::blur()));
    CHECK((Cmd<int>::blur() == Cmd<int>::blur()));
    CHECK((Cmd<int>::copy("https://x") == Cmd<int>::copy("https://x")));
    CHECK((Cmd<int>::download("r.csv", "a,b\n", "text/csv")
        == Cmd<int>::download("r.csv", "a,b\n", "text/csv")));
    CHECK(!(Cmd<int>::download("r.csv", "a") == Cmd<int>::download("r.csv", "b")));
    // localStorage persistence is a Cmd; restore is a Sub — both plain data.
    CHECK((Cmd<int>::store("theme", "dark") == Cmd<int>::store("theme", "dark")));
    CHECK(!(Cmd<int>::store("theme", "dark") == Cmd<int>::store("theme", "light")));
    CHECK(!(Cmd<int>::store("k", "v") == Cmd<int>::store_clear("k")));
    // map() carries browser effects through unchanged (they hold no Msg)
    {
        auto up = [](int m){ return (long)m * 2; };
        CHECK((Cmd<int>::set_title("t").map(up) == Cmd<long>::set_title("t")));
        CHECK((Cmd<int>::scroll_to("a", false).map(up) == Cmd<long>::scroll_to("a", false)));
        CHECK((Cmd<int>::focus("f").map(up) == Cmd<long>::focus("f")));
        CHECK((Cmd<int>::blur().map(up) == Cmd<long>::blur()));
        CHECK((Cmd<int>::copy("c").map(up) == Cmd<long>::copy("c")));
        CHECK((Cmd<int>::download("f", "d", "m").map(up) == Cmd<long>::download("f", "d", "m")));
        // and inside a batch, alongside a Msg-carrying alternative
        auto b = Cmd<int>::batch(Cmd<int>::set_title("t"), Cmd<int>::emit(Tick)).map(up);
        CHECK((b == Cmd<long>::batch(Cmd<long>::set_title("t"), Cmd<long>::emit((long)Tick * 2))));
    }
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
    {
        // on_storage is discoverable (from a batch) and maps (key,value)->Msg;
        // it restores persisted state on connect.
        auto s = Sub<int>::batch({Sub<int>::every(1000ms, Tick),
                                  Sub<int>::on_storage([](std::string k, std::string v){ return (int)(k.size() + v.size()); })});
        auto* st = s.storage();
        CHECK(st != nullptr);
        CHECK(st->on("theme", "dark") == 9);           // 5 + 4
        auto lifted = s.map([](int m){ return m + 100; });
        CHECK(lifted.storage()->on("k", "v") == 102);   // (1+1)+100
        CHECK(Sub<int>::every(16, Tick).storage() == nullptr);
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

    // ── perform(): browser-effect Cmds emit the exact @-control frames ──────
    {
        int sv[2];
        CHECK(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        auto s = std::make_shared<detail::Session>(); s->conn = sv[0];
        auto read_text = [&]() -> std::string {
            char buf[4096];
            ssize_t n = ::recv(sv[1], buf, sizeof buf, 0);
            if (n <= 0) return {};
            // These are SERVER->client frames, which are unmasked per RFC 6455
            // (only client->server frames are masked). ws::decode() intentionally
            // rejects unmasked frames (it's the inbound/server-side decoder), so
            // parse the short server frame directly for inspection.
            auto u = [&](std::size_t i){ return (unsigned char)buf[i]; };
            if (n < 2 || (u(0) & 0x0F) != 0x1) return {};      // want a text frame
            std::size_t len = u(1) & 0x7F, pos = 2;
            if (len == 126){ if(n<4) return {}; len = (u(2)<<8)|u(3); pos = 4; }
            if ((std::size_t)n < pos + len) return {};
            return std::string(buf + pos, len);
        };
        using C = Cmd<int>;
        detail::perform(s, C::set_title("Inbox (3)"));
        CHECK(read_text() == "@title|Inbox (3)");
        detail::perform(s, C::scroll_to("bottom"));
        CHECK(read_text() == "@scroll|1|bottom");
        detail::perform(s, C::scroll_to("row-9", false));
        CHECK(read_text() == "@scroll|0|row-9");
        detail::perform(s, C::focus("email"));
        CHECK(read_text() == "@focus|email");
        detail::perform(s, C::blur());
        CHECK(read_text() == "@blur|");
        detail::perform(s, C::copy("https://x/y"));
        CHECK(read_text() == "@copy|https://x/y");
        // download: name/mime are '|'-sanitised, payload is base64 of the bytes
        detail::perform(s, C::download("a|b.csv", "hi", "text|csv"));
        CHECK(read_text() == "@dl|a_b.csv|text_csv|aGk=");
        s->alive = false;
        ::close(sv[0]); ::close(sv[1]);
    }

    // ── Viewport: the display's self-report as data ──────────────────────
    {
        Viewport vp{390, 844, true, "America/New_York"};
        CHECK(vp.phone() && !vp.tablet() && !vp.desktop());
        CHECK((Viewport{800, 600}).tablet());
        CHECK((Viewport{1440, 900}).desktop());
        CHECK((vp == Viewport{390, 844, true, "America/New_York"}));
        CHECK(!(vp == Viewport{391, 844, true, "America/New_York"}));

        // Sub::on_viewport is collectable (from a batch) and maps functorially
        auto s1 = Sub<int>::batch({ Sub<int>::every(std::chrono::milliseconds(16), Tick),
                                    Sub<int>::on_viewport([](Viewport v){ return v.width; }) });
        auto* vh = s1.viewport();
        CHECK(vh != nullptr);
        CHECK(vh->on(Viewport{1024, 768}) == 1024);
        auto mapped = s1.map([](int m){ return (long)m + 1; });
        auto* mvh = mapped.viewport();
        CHECK(mvh != nullptr && mvh->on(Viewport{500, 500}) == 501L);
        CHECK(Sub<int>::every(std::chrono::milliseconds(16), Tick).viewport() == nullptr);
    }

    // ── owner-loop plumbing: Deliver env/sync flags round-trip the queue ────
    {
        auto s = std::make_shared<detail::Session>(); s->conn = -1;
        s->push_env("1440|900|1|Europe/Berlin");
        s->push_sync();
        auto d1 = s->pop();
        CHECK(d1 && d1->is_env && d1->value == "1440|900|1|Europe/Berlin");
        auto d2 = s->pop();
        CHECK(d2 && d2->is_sync && !d2->is_env);
        CHECK(s->visible.load());        // sessions start visible
        s->alive = false;
    }

    // ── reconcile_subs fast path: unchanged subs skip the reconcile ─────────
    {
        auto s = std::make_shared<detail::Session>(); s->conn = -1;
        // a timer + a topic. First reconcile applies them and stamps the fp.
        auto sub = Sub<int>::batch(Sub<int>::every(100, Tick),
                                   Sub<int>::on_topic("room", [](std::string){ return Tick; }));
        CHECK(s->sub_fingerprint == 0);              // never reconciled
        detail::reconcile_subs<int>(s, sub);
        std::uint64_t fp1 = s->sub_fingerprint;
        CHECK(fp1 != 0);                             // fingerprint stamped
        CHECK(s->timers.size() == 1);                // the timer thread started
        // Reconciling the SAME sub again is a no-op: fp unchanged, timer kept.
        auto* run_before = s->timers[0].run.get();
        detail::reconcile_subs<int>(s, sub);
        CHECK(s->sub_fingerprint == fp1);            // same fingerprint
        CHECK(s->timers.size() == 1 && s->timers[0].run.get() == run_before);  // untouched
        // Changing the sub (drop the topic) DOES re-reconcile: fp changes.
        detail::reconcile_subs<int>(s, Sub<int>::every(100, Tick));
        CHECK(s->sub_fingerprint != fp1);            // fingerprint moved
        CHECK(s->timers.size() == 1);                // timer survives (same key)
        // Empty subs clear everything and change the fp again.
        detail::reconcile_subs<int>(s, Sub<int>::none());
        CHECK(s->timers.empty());
        detail::Hub::instance().remove(s.get());
        s->alive = false;
        for (auto& t : s->timers) *t.run = false;
    }

    // ── Scheduler: ONE thread services all timed effects (after + every) ────
    //    replaces the old per-Cmd::after / per-subscription-timer std::thread,
    //    which spawned O(sessions x timers) sleeping OS threads. Counters are
    //    heap-allocated (shared_ptr) because the Scheduler dispatches callbacks
    //    on the Pool — a just-cancelled timer can have one in-flight callback,
    //    which must not touch a freed stack local.
    {
        using namespace std::chrono_literals;
        auto& S = detail::Scheduler::instance();

        // after() fires exactly once, roughly on time.
        auto once = std::make_shared<std::atomic<int>>(0);
        S.after(40, [once]{ once->fetch_add(1); });
        std::this_thread::sleep_for(120ms);
        CHECK(once->load() == 1);

        // every() repeats until its run flag is cleared, and STAYS stopped.
        auto ticks = std::make_shared<std::atomic<int>>(0);
        auto run = std::make_shared<std::atomic<bool>>(true);
        S.every(25, [ticks]{ ticks->fetch_add(1); }, run);
        std::this_thread::sleep_for(130ms);
        run->store(false);
        int at_cancel = ticks->load();
        CHECK(at_cancel >= 3);                          // fired several times
        std::this_thread::sleep_for(90ms);
        CHECK(ticks->load() <= at_cancel + 1);          // cancel actually stops it

        // scale: many timers share the one scheduler thread (not N threads).
        auto total = std::make_shared<std::atomic<int>>(0);
        std::vector<std::shared_ptr<std::atomic<bool>>> runs;
        for (int i = 0; i < 500; ++i){ auto r = std::make_shared<std::atomic<bool>>(true);
            runs.push_back(r); S.every(20, [total]{ total->fetch_add(1); }, r); }
        std::this_thread::sleep_for(90ms);
        for (auto& r : runs) r->store(false);
        CHECK(total->load() >= 500);                    // all 500 fired at least once
        std::this_thread::sleep_for(60ms);              // let any in-flight callbacks drain
    }

    std::cerr << (g_fail ? "SOME TESTS FAILED\n" : "all effect tests passed\n");
    std::cerr << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
