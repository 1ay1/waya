// tests/test_harness.cpp — the first-class unit-test harness + fetch Response.
// Proves an app can be driven end-to-end (init/update/view) with no runtime,
// and that Cmd::fetch_full surfaces the HTTP status instead of dropping it.
#include <waya/surface/test.hpp>
#include <waya/surface/effect.hpp>
#include <iostream>
#include <string>
#include <variant>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* m) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << m << "\n"; } }

// ── a tiny but complete Program to drive ─────────────────────────────────────
struct Counter {
    struct Model { int n = 0; bool saved = false; };
    struct Inc {}; struct Dec {}; struct SetTo {}; struct Save {}; struct Saved {};
    using Msg = std::variant<Inc, Dec, SetTo, Save, Saved>;

    static Model init() { return { 0, false }; }

    // full shape: value + effects
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, const std::string& value) {
        return std::visit([&](auto&& e) -> std::pair<Model, Cmd<Msg>> {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, Inc>)   return { { m.n + 1, false }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, Dec>) return { { m.n - 1, false }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, SetTo>) return { { std::atoi(value.c_str()), false }, Cmd<Msg>::none() };
            else if constexpr (std::is_same_v<T, Save>) return { m, Cmd<Msg>::after(300, Saved{}) };
            else return { { m.n, true }, Cmd<Msg>::none() };   // Saved
        }, msg);
    }

    static NodeRef view(const Model& m) {
        return col(
            text("count: " + std::to_string(m.n)) | key("label"),
            row(
                button("−") | tap(Dec{}),
                button("+") | tap(Inc{})
            ),
            // a text-labelled button + a wired input, so the harness can drive
            // them by label/placeholder through the real token wiring.
            button("Save changes") | tap(Save{}),
            input(std::to_string(m.n)) | placeholder("set count") | on_input([](std::string){ return SetTo{}; }),
            m.saved ? text("saved!") : text("")
        );
    }
};
static_assert(SurfaceProgram<Counter>);

int main() {
    // ── harness drives init/update/view purely ───────────────────────────────
    auto app = test::harness<Counter>();
    check(app.model().n == 0, "init model");

    app.send(Counter::Inc{}).send(Counter::Inc{}).send(Counter::Inc{});
    check(app.model().n == 3, "three Inc → n==3");
    check(app.text_contains("count: 3"), "view reflects model");

    app.send(Counter::Dec{});
    check(app.model().n == 2, "Dec → n==2");

    // Msg WITH an input value (as a wired input would deliver)
    app.send(Counter::SetTo{}, "42");
    check(app.model().n == 42, "SetTo with value 42");

    // ── effects are recorded and asserted WITHOUT running them ───────────────
    app.send(Counter::Save{});
    check(app.last_cmd() == Cmd<Counter::Msg>::after(300, Counter::Saved{}),
          "Save schedules an after(300, Saved) effect");

    app.send(Counter::Saved{});
    check(app.model().saved, "Saved sets the flag");
    check(app.text_contains("saved!"), "view shows saved!");

    // ── structural queries over the rendered view ────────────────────────────
    check(app.count(Kind::button) == 3, "three buttons rendered (−, +, Save)");
    check(app.find_key("label") != nullptr, "keyed node addressable");
    check(app.valid(), "rendered view is structurally sound");
    check(app.validate().empty(), "no violations reported");

    // send_all for scenario setup
    auto app2 = test::harness<Counter>();
    app2.send_all({ Counter::Inc{}, Counter::Inc{}, Counter::Dec{} });
    check(app2.model().n == 1, "send_all applies in order");

    // ── Cmd::fetch_full carries the HTTP Response (status), not just a body ───
    {
        using Msg = Counter::Msg;
        // build a full-response fetch; confirm it stores as a Fetch with on_response
        bool saw_response = false; int seen_status = -1;
        auto cmd = Cmd<Msg>::fetch_full("http://x/api", [&](Cmd<Msg>::Response r) -> Msg {
            saw_response = true; seen_status = r.status; return Counter::Inc{};
        });
        // simulate the runtime delivering a 404 to on_response
        std::visit([&](auto&& alt) {
            using T = std::decay_t<decltype(alt)>;
            if constexpr (std::is_same_v<T, typename Cmd<Msg>::Fetch>) {
                check((bool)alt.on_response, "fetch_full sets on_response, not on_done");
                check(!alt.on_done, "fetch_full leaves on_done empty");
                typename Cmd<Msg>::Response r{ 404, "not found", {} };
                (void)alt.on_response(r);
            } else check(false, "fetch_full built a Fetch");
        }, cmd.alt());
        check(saw_response && seen_status == 404, "on_response receives the 404 status");
    }
    // Response helpers
    {
        Cmd<Counter::Msg>::Response r{ 200, "{}", {{"Content-Type","application/json"}} };
        check(r.ok(), "200 is ok()");
        check(r.header("content-type") == "application/json", "header lookup case-insensitive");
        Cmd<Counter::Msg>::Response bad{ 0, "", {} };
        check(!bad.ok(), "status 0 (never completed) is not ok()");
    }

    // ── INTERACTION: click/fill drive the app through the REAL wiring ────────
    // This is what `send()` can't do: prove the button you rendered is actually
    // wired to the Msg you think, and that a text input dispatches on_input.
    {
        auto a = test::harness<Counter>();
        // click by label resolves "+"/"−" -> the tapped token -> the Msg
        a.click("+").click("+").click("−");
        check(a.model().n == 1, "click(+/−) drives the counter through its wiring");
        // the labelled Save button dispatches Save{} — which returns a timer
        // Cmd (Saved fires later). click() proves the WIRING; the effect is the
        // app's own (after 300ms). Drive the follow-up Msg to complete it.
        check(!a.model().saved, "not saved before the click");
        a.click("Save");                        // substring match on "Save changes"
        check(!a.model().saved, "click(Save) dispatched Save (timer pending, not yet saved)");
        a.send(Counter::Saved{});               // the timer's Msg lands
        check(a.model().saved, "Saved sets the flag — the full click→effect→Msg loop");
        // can_click distinguishes a live button from an absent one
        check(a.can_click("Save"), "can_click finds the live Save button");
        check(!a.can_click("Delete"), "can_click is false for an absent label");
        // fill types into the wired input -> on_input -> SetTo
        a.fill("42");
        check(a.model().n == 42, "fill() fires the input's on_input handler");
        // clicking a label that isn't a wired target throws (loud test failure)
        bool threw = false;
        try { a.click("nonexistent button"); } catch (const std::exception&) { threw = true; }
        check(threw, "click on an unwired label throws");
    }

    std::cout << "test_harness: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
