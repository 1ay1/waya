// examples/dash.cpp — a live dashboard showcasing the component library:
// icons, tabs, toggle, progress, a data table, and inline charts — all driven
// by the Elm loop, all streaming minimal deltas on each interaction.
//
//   cmake --build build --target dash && ./build/dash   # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;

struct Dash {
    struct Person { int id; std::string name; std::string role; float score; };

    struct Model {
        int tab = 0;
        bool live = true;
        int  sel = -1;
        float load = 42;
        std::vector<Person> people{
            {1, "Ada Lovelace",   "Admin",  92},
            {2, "Linus Torvalds", "Member", 71},
            {3, "Grace Hopper",   "Admin",  88},
            {4, "Alan Turing",    "Member", 64},
        };
        std::vector<float> traffic{12, 30, 18, 44, 27, 51, 39, 60, 48};
    };

    struct Tab { int i; }; struct ToggleLive {}; struct Pick { int id; };
    struct SetLoad { std::string v; }; struct Tick {};
    using Msg = std::variant<Tab, ToggleLive, Pick, SetLoad, Tick>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        std::visit(overload{
            [&](Tab t)        { m.tab = t.i; },
            [&](ToggleLive)   { m.live = !m.live; },
            [&](Pick p)       { m.sel = (m.sel == p.id ? -1 : p.id); },
            [&](SetLoad s)    { m.load = (float)std::atof(s.v.c_str()); },
            [&](Tick)         { m.traffic.erase(m.traffic.begin());
                                m.traffic.push_back((float)((int)(m.traffic.back() * 1.3f + 7) % 70 + 10)); },
        }, msg);
        return { m, Cmd<Msg>::none() };
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.live ? Sub<Msg>::every(1200, Tick{}) : Sub<Msg>::none();
    }

    static NodeRef stat(std::string label, std::string value, std::string_view ic, Tone tone) {
        return card(
            row(icon(ic, 18) | fg_primary, text(label) | fg_muted | css("font-size", "13px")) | gap(8) | center,
            text(value) | font(30) | bold,
            badge("+4.2%", tone)
        ) | gap(8) | w(pct_(100));
    }

    static NodeRef overview(const Model& m) {
        return col(
            row(stat("Users", "128", "user", Tone::success),
                stat("Revenue", "$9.2k", "star", Tone::primary),
                stat("Alerts", "3", "bell", Tone::warning)) | gap(16),
            card(
                row(text("Traffic") | semibold, push(), toggle(m.live, ToggleLive{})) | center,
                line_chart(m.traffic) | stroke(0x22d3ee, 2) | w(pct_(100)) | h(90)
            ) | gap(12),
            card(
                row(text("Server load") | semibold, push(), text(std::to_string((int)m.load) + "%") | fg_muted) | center,
                progress(m.load, m.load > 80 ? Tone::danger : Tone::primary),
                slider(m.load, 0, 100, [](std::string v){ return SetLoad{ v }; })
            ) | gap(12)
        ) | gap(16);
    }

    static NodeRef team(const Model& m) {
        return card(
            data_table<Person>(m.people, {
                { "Name", [&](const Person& p){
                    return row(avatar(p.name.substr(0,1) + std::string(1, p.name[p.name.find(' ')+1])),
                               text(p.name)) | gap(10) | center; } },
                { "Role", [](const Person& p){
                    return badge(p.role, p.role == "Admin" ? Tone::primary : Tone::neutral); } },
                { "Score", [](const Person& p){ return bars({p.score, p.score*0.7f, p.score}) | w(70) | h(22) | fg(0x8b5cf6); } },
                { "", [](const Person& p){ return icon_button_i("chevron-right", Pick{ p.id }); } },
            })
        );
    }

    static NodeRef view(const Model& m) {
        auto body = m.tab == 0 ? overview(m) : team(m);
        return col(
            row(icon("home", 22) | fg_primary, text("waya dash") | font(20) | bold,
                push(),
                badge(m.live ? "live" : "paused", m.live ? Tone::success : Tone::neutral)) | center,
            tabs(m.tab, {{0, "Overview"}, {1, "Team"}}, [](int i){ return Tab{ i }; }),
            body
        ) | gap(20) | pad(28) | css("max-width", "820px") | css("margin", "0 auto")
          | css("min-height", "100dvh") | theme(midnight());
    }
};

int main() { return live<Dash>({ .port = 8080, .title = "waya · dash" }); }
