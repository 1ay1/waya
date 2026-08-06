// tests/test_timetravel.cpp — record/replay time-travel debugging.
// Because update() is pure and effects are data, the whole app history is just
// the message log — replay it and land in the exact same state. This proves
// stepping back/forward/jumping, branch truncation, per-step diffs, trace
// export, and the "find the first broken frame" helper.
#include <waya/surface/timetravel.hpp>
#include <iostream>
#include <string>
#include <variant>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* m) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << m << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

// ── an app with an effect and a (deliberately) breakable view ────────────────
struct App {
    struct Model { int n = 0; bool broken = false; };
    struct Inc {}; struct Dec {}; struct SetTo {}; struct Save {}; struct Saved {}; struct Break {};
    using Msg = std::variant<Inc, Dec, SetTo, Save, Saved, Break>;

    static Model init() { return { 0, false }; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, const std::string& value) {
        return std::visit([&](auto&& e) -> std::pair<Model, Cmd<Msg>> {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Inc>)    return { { m.n + 1, m.broken }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, Dec>) return { { m.n - 1, m.broken }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, SetTo>) return { { std::atoi(value.c_str()), m.broken }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, Save>)  return { m, Cmd<Msg>::after(300, Saved{}) };
            else if constexpr (std::is_same_v<T, Break>) return { { m.n, true }, Cmd<Msg>::none() };
            else return { m, Cmd<Msg>::none() };   // Saved
        }, msg);
    }

    static NodeRef view(const Model& m) {
        // when broken, render an image with no alt → validator flags it
        return col(
            text("n=" + std::to_string(m.n)),
            m.broken ? image("/x.png") : text("ok")
        );
    }
};
static_assert(SurfaceProgram<App>);

int main() {
    auto tl = debug::timeline<App>();
    check(tl.model().n == 0, "init at 0");
    check(tl.cursor() == 0 && tl.at_start(), "cursor at start");

    tl.send(App::Inc{}).send(App::Inc{}).send(App::Inc{});
    check(tl.model().n == 3, "three Inc -> 3");
    check(tl.step_count() == 3, "three steps recorded");
    check(tl.at_end(), "cursor at end after sends");

    // ── step back through history ────────────────────────────────────────────
    tl.back();
    check(tl.model().n == 2, "back -> 2");
    tl.back();
    check(tl.model().n == 1, "back -> 1");
    check(!tl.at_start() && !tl.at_end(), "cursor in the middle");

    // ── jump to any point ────────────────────────────────────────────────────
    tl.jump(0);
    check(tl.model().n == 0, "jump(0) -> init");
    tl.jump(3);
    check(tl.model().n == 3, "jump(3) -> latest");

    // ── forward re-applies a stepped-back message ────────────────────────────
    tl.reset();
    check(tl.model().n == 0, "reset -> 0");
    tl.forward().forward();
    check(tl.model().n == 2, "two forwards -> 2");

    // ── branch truncation: acting after stepping back drops the future ───────
    tl.jump(1);                    // n == 1, cursor 1, log still has 3
    tl.send(App::SetTo{}, "42");   // new branch: truncate to 1 then append
    check(tl.model().n == 42, "new branch SetTo 42");
    check(tl.step_count() == 2, "future truncated to 2 steps");
    tl.latest();
    check(tl.model().n == 42, "latest reflects the new branch");

    // ── effects recorded as data, asserted without running ───────────────────
    {
        auto t2 = debug::timeline<App>();
        t2.send(App::Save{}, "", "Save");
        check(t2.last_cmd() == Cmd<App::Msg>::after(300, App::Saved{}),
              "recorded step carries its effect");
    }

    // ── per-step surface diff: what did this message change on screen? ───────
    {
        auto t3 = debug::timeline<App>();
        t3.send(App::Inc{});
        auto p = t3.last_patch();
        check(!p.empty(), "Inc produced a visible patch");
        // model_at / view_at don't move the cursor
        check(t3.model_at(0).n == 0 && t3.cursor() == 1, "model_at preview doesn't move cursor");
    }

    // ── find the first broken frame in a long session ────────────────────────
    {
        auto t4 = debug::timeline<App>();
        t4.send(App::Inc{}).send(App::Inc{}).send(App::Break{}).send(App::Inc{});
        std::size_t bad = t4.first_invalid();
        check(bad == 3, "first invalid view is at step 3 (the Break)");
        // a clean session reports none
        auto t5 = debug::timeline<App>();
        t5.send(App::Inc{}).send(App::Dec{});
        check(t5.first_invalid() == t5.step_count() + 1, "clean session: no invalid frame");
    }

    // ── export a trace for a bug report ──────────────────────────────────────
    {
        auto t6 = debug::timeline<App>();
        t6.send(App::Inc{}, "", "increment").send(App::Break{}, "", "break it");
        std::string tr = t6.export_trace();
        check(has(tr, "init()"), "trace shows init");
        check(has(tr, "increment"), "trace shows labels");
        check(has(tr, "INVALID VIEW"), "trace marks the broken frame");
    }

    std::cout << "test_timetravel: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
