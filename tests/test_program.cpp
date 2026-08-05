/// tests/test_program.cpp — the Elm architecture: Model/Msg/update are pure and
/// testable with `==`, no server, no runtime. This is the whole point of the
/// Program shape (maya's core insight, ported).

#include <waya/app/program.hpp>
#include <waya/waya.hpp>

#include <iostream>
#include <variant>

using namespace waya;
using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

// ── A tiny counter Program ──────────────────────────────────────────────────
struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Inc)   { return std::pair{Model{m.n + 1}, Cmd<Msg>::none()}; },
            [&](Dec)   { return std::pair{Model{m.n - 1}, Cmd<Msg>::none()}; },
            [&](Reset) { return std::pair{Model{0},       Cmd<Msg>::none()}; },
        }, msg);
    }

    static auto view(const Model& m) {
        return div_(
            h1_(text("Count: " + std::to_string(m.n))),
            button_(text("−")),
            button_(text("+"))
        );
    }
};

static_assert(Program<Counter>, "Counter must satisfy the Program concept");

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

int main() {
    // update is a pure function — drive it directly, assert on Model.
    auto [m0, c0] = program_init<Counter>();
    CHECK(m0.n == 0);
    CHECK(c0 == Cmd<Counter::Msg>::none());

    auto [m1, c1] = Counter::update(m0, Counter::Inc{});
    CHECK(m1.n == 1);
    auto [m2, c2] = Counter::update(m1, Counter::Inc{});
    CHECK(m2.n == 2);
    auto [m3, c3] = Counter::update(m2, Counter::Dec{});
    CHECK(m3.n == 1);
    auto [m4, c4] = Counter::update(m3, Counter::Reset{});
    CHECK(m4.n == 0);

    // view is pure too — same model, same HTML.
    auto h_a = waya::render::render(Counter::view(Counter::Model{5})).html;
    auto h_b = waya::render::render(Counter::view(Counter::Model{5})).html;
    CHECK(h_a == h_b);
    CHECK(h_a.find("Count: 5") != std::string::npos);

    // Cmd equality works (makes effect assertions possible).
    CHECK((Cmd<int>::none()  == Cmd<int>::none()));
    CHECK((Cmd<int>::quit()  == Cmd<int>::quit()));
    CHECK(!(Cmd<int>::none() == Cmd<int>::quit()));
    CHECK((Cmd<int>::navigate("/a") == Cmd<int>::navigate("/a")));
    CHECK(!(Cmd<int>::navigate("/a") == Cmd<int>::navigate("/b")));

    // A Program that returns effects: init with a startup Cmd.
    struct WithInit {
        struct Model { int x = 0; };
        using Msg = std::variant<int>;
        static std::pair<Model, Cmd<Msg>> init() {
            return { Model{7}, Cmd<Msg>::navigate("/start") };
        }
        static std::pair<Model, Cmd<Msg>> update(Model m, Msg) { return {m, Cmd<Msg>::none()}; }
        static auto view(const Model&) { return div_(text("x")); }
    };
    static_assert(Program<WithInit>);
    auto [wm, wc] = program_init<WithInit>();
    CHECK(wm.x == 7);
    CHECK((wc == Cmd<WithInit::Msg>::navigate("/start")));

    std::cout << "test_program: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
