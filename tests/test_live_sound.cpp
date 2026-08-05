/// tests/test_live_sound.cpp — the "if it compiles, the live UI is consistent"
/// guarantee, made executable. Drive a Program through message sequences and
/// assert the emitted patch always reproduces the freshly-rendered tree.

#include <waya/waya.hpp>
#include <waya/app/verify.hpp>
#include <waya/app/msg.hpp>

#include <iostream>
#include <variant>
#include <vector>

using namespace waya;
using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

// The counter from examples/counter.cpp, in test form.
struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;
    static Model init() { return {}; }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Inc)  { return std::pair{Model{m.n + 1}, Cmd<Msg>::none()}; },
            [&](Dec)  { return std::pair{Model{m.n - 1}, Cmd<Msg>::none()}; },
            [&](Reset){ return std::pair{Model{0},       Cmd<Msg>::none()}; },
        }, msg);
    }
    static auto view(const Model& m) {
        return div_(
            div_(text(std::to_string(m.n))),
            div_(button_(text("-")) | on_msg(Msg{Dec{}}),
                 button_(text("+")) | on_msg(Msg{Inc{}}))
        ) | flex(Dir::col);
    }
};

// A Program whose SHAPE changes (tests replace/insert/remove ops, not just text).
struct Todoish {
    struct Model { std::vector<std::string> items; bool loading = false; };
    struct Add { std::string s; }; struct Clear {}; struct ToggleLoad {};
    using Msg = std::variant<Add, Clear, ToggleLoad>;
    static Model init() { return {}; }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Add a)     { m.items.push_back(a.s); return std::pair{m, Cmd<Msg>::none()}; },
            [&](Clear)     { m.items.clear();        return std::pair{m, Cmd<Msg>::none()}; },
            [&](ToggleLoad){ m.loading = !m.loading; return std::pair{m, Cmd<Msg>::none()}; },
        }, msg);
    }
    static auto view(const Model& m) {
        return div_(
            when(m.loading, span_(text("loading…")), span_(text("ready"))),
            ul_(each(m.items, [](const std::string& s){ return li_(text(s)); }))
        );
    }
};

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

int main() {
    using CM = Counter::Msg;
    // Every step of a mixed sequence must round-trip: apply(diff)==next.
    CHECK(verify_roundtrip<Counter>({
        CM{Counter::Inc{}}, CM{Counter::Inc{}}, CM{Counter::Dec{}},
        CM{Counter::Reset{}}, CM{Counter::Dec{}}, CM{Counter::Inc{}},
    }));

    // Shape-changing program: adds, clears, toggles — exercises insert/remove/
    // replace ops, still round-trips.
    using TM = Todoish::Msg;
    CHECK(verify_roundtrip<Todoish>({
        TM{Todoish::Add{"a"}}, TM{Todoish::Add{"b"}}, TM{Todoish::ToggleLoad{}},
        TM{Todoish::Add{"c"}}, TM{Todoish::Clear{}}, TM{Todoish::ToggleLoad{}},
        TM{Todoish::Add{"x"}},
    }));

    // A long random-ish counter walk never drifts.
    {
        std::vector<CM> seq;
        for (int i = 0; i < 200; ++i)
            seq.push_back(i % 3 == 0 ? CM{Counter::Dec{}}
                        : i % 7 == 0 ? CM{Counter::Reset{}}
                                     : CM{Counter::Inc{}});
        CHECK(verify_roundtrip<Counter>(seq));
    }

    // RESET-HEAVY: the exact bug that hit the running server — return-to-zero
    // after climbing must always emit a patch, never an empty one from a stale
    // ancestor hash. Interleave climbs and resets aggressively.
    {
        std::vector<CM> seq;
        for (int i = 0; i < 50; ++i) {
            seq.push_back(CM{Counter::Inc{}});
            seq.push_back(CM{Counter::Inc{}});
            seq.push_back(CM{Counter::Reset{}});   // back to 0 every 3rd step
            seq.push_back(CM{Counter::Dec{}});
            seq.push_back(CM{Counter::Reset{}});
        }
        CHECK(verify_roundtrip<Counter>(seq));
    }

    std::cout << "test_live_sound: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
