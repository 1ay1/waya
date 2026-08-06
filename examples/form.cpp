/// examples/form.cpp — the form controls + keyed-list reordering, live.
///
///   cmake --build build -j && ./build/form     # http://localhost:8080
///
/// Everything here is pure `update` returning (Model, Cmd). It exercises every
/// rich input (text, textarea, checkbox, radio, select) and a KEYED list you
/// can reorder — waya emits `move` ops, so the rows slide without re-rendering
/// (their DOM, and any focus/scroll, survives the reorder).

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace waya::surface;

struct Form {
    struct Task { int id; std::string label; };
    struct Model {
        std::string name;
        std::string bio;
        bool        subscribe = false;
        std::string plan = "free";       // radio group
        std::string theme = "dark";      // select
        std::vector<Task> tasks = {{1,"ship effects"},{2,"ship inputs"},{3,"ship keyed diff"}};
    };
    using Msg = int;
    enum { SetName, SetBio, ToggleSub, SetPlan, SetTheme, Up, Down };

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        switch (msg) {
            case SetName:   m.name = value; break;
            case SetBio:    m.bio = value; break;
            case ToggleSub: m.subscribe = (value == "true"); break;
            case SetPlan:   m.plan = value; break;
            case SetTheme:  m.theme = value; break;
            case Up:   rotate(m.tasks, -1); break;   // move first task to the end
            case Down: rotate(m.tasks, +1); break;   // move last task to the front
        }
        return { m, Cmd<Msg>::none() };
    }

    static void rotate(std::vector<Task>& v, int dir) {
        if (v.size() < 2) return;
        if (dir < 0) std::rotate(v.begin(), v.begin()+1, v.end());
        else         std::rotate(v.begin(), v.end()-1, v.end());
    }

    static NodeRef field(std::string lbl, NodeRef control) {
        return col(
            text(std::move(lbl)) | fg(0x94a3b8) | font(13) | semibold,
            std::move(control)
        ) | gap(6);
    }
    static NodeRef box_field(NodeRef control) { return control; }

    static NodeRef view(const Model& m) {
        auto txt = [](std::string v, int msg, std::string ph) {
            return input(std::move(v)) | placeholder(std::move(ph)) | on_input(msg)
                 | fg(0xe2e8f0) | bg(0x0b1220) | pad_x(14) | pad_y(10)
                 | round(10) | border(1, 0x1f2937) | font(15);
        };
        auto pill = [](std::string lbl, int msg, std::uint32_t c) {
            return button(std::move(lbl)) | fg(0xffffff) | bg(c) | font(15) | semibold
                 | pad_x(18) | pad_y(10) | round(999) | tap(msg)
                 | transition() | on(Hover, opacity(0.85f));
        };

        // The keyed task list — each row carries key(id). Reordering the model
        // makes waya emit move ops; the row DOM is preserved.
        std::vector<NodeRef> rows;
        for (auto& t : m.tasks) {
            rows.push_back(
                row(
                    text("#" + std::to_string(t.id)) | fg(0x64748b) | font(13),
                    text(t.label) | fg(0xe2e8f0) | font(15)
                ) | key(std::to_string(t.id))
                  | gap(10) | pad_x(14) | pad_y(10) | bg(0x0b1220)
                  | round(10) | border(1, 0x1f2937) | transition()
            );
        }
        auto listbox = box(); listbox->kids = std::move(rows);
        listbox->style.flow = Flow::col;
        finalize(*listbox);
        listbox = listbox | gap(8);

        auto form_card = col(
            text("Profile") | fg(0xe2e8f0) | font_fluid(26, 32) | weight(Weight::black),

            field("Name", txt(m.name, SetName, "Ada Lovelace")),
            field("Bio",  textarea(m.bio) | placeholder("A short bio…") | on_input(SetBio)
                        | fg(0xe2e8f0) | bg(0x0b1220) | pad_x(14) | pad_y(10)
                        | round(10) | border(1, 0x1f2937) | font(15)
                        | css("min-height", "5rem") | css("resize", "vertical")),

            field("Plan", row(
                radio("plan","free", m.plan=="free") | on_change(SetPlan), text("Free") | fg(0xcbd5e1),
                radio("plan","pro",  m.plan=="pro")  | on_change(SetPlan), text("Pro")  | fg(0xcbd5e1)
            ) | gap(8) | center),

            field("Theme", select({option("dark","Dark"),option("light","Light"),option("auto","Auto")}, m.theme)
                         | on_change(SetTheme) | fg(0xe2e8f0) | bg(0x0b1220)
                         | pad_x(12) | pad_y(10) | round(10) | border(1, 0x1f2937)),

            row(
                checkbox(m.subscribe) | on_change(ToggleSub),
                text("Email me updates") | fg(0xcbd5e1) | font(15)
            ) | gap(10) | center,

            text("Tasks (keyed — reordering slides rows, no re-render)")
                | fg(0x94a3b8) | font(13) | semibold,
            listbox,
            row(pill("▲ rotate up", Up, 0x334155), pill("▼ rotate down", Down, 0x6366f1)) | gap(10) | wrap | center

        ) | gap(20) | pad_fluid(20, 40) | round(20) | bg(0x111827) | shadow() | border(1, 0x1f2937);

        return page(0x0b1020, centered(30, form_card) | center);
    }
};

int main() {
    static_assert(SurfaceProgram<Form>);
    return live<Form>({.port = 8080});
}
