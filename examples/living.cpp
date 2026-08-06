// examples/living.cpp — reusable components that are FAST and beautifully
// interacting. A todo list where: rows are memoised components (rebuilt only
// when their own data changes), items are KEYED so the diff reconciles by
// identity, and `animated()` makes every add / remove / reorder / filter glide
// (FLIP) instead of snapping. Zero animation state in the Model.
//
//   cmake --build build --target living && ./build/living   # localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/component.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;
using namespace waya::color;

struct Living {
    struct Item { int id; std::string text; bool done = false; int prio; };

    struct Model {
        std::vector<Item> items{
            {1, "Design the type-state gates", false, 2},
            {2, "Ship the component library",  true,  1},
            {3, "Write the FLIP animation",    false, 3},
            {4, "Record the demo",             false, 0},
        };
        int next_id = 5;
        bool hide_done = false;
        int sort = 0;   // 0 = by priority, 1 = by name
    };

    struct Toggle { int id; }; struct Remove { int id; }; struct Add {};
    struct HideDone {}; struct SortBy { int s; }; struct Bump { int id; };
    using Msg = std::variant<Toggle, Remove, Add, HideDone, SortBy, Bump>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Toggle t){ for (auto& i : m.items) if (i.id == t.id) i.done = !i.done; },
            [&](Remove r){ std::erase_if(m.items, [&](auto& i){ return i.id == r.id; }); },
            [&](Add){ m.items.push_back({ m.next_id, "New task #" + std::to_string(m.next_id), false, 2 }); ++m.next_id; },
            [&](HideDone){ m.hide_done = !m.hide_done; },
            [&](SortBy s){ m.sort = s.s; },
            [&](Bump b){ for (auto& i : m.items) if (i.id == b.id) i.prio = (i.prio + 1) % 4; },
        }, msg);
        return m;
    }

    // A row is a COMPONENT: memoised on exactly its data, so it's rebuilt only
    // when this item's fields change — not when a sibling moves or the list
    // re-sorts. Keyed + animated so it glides to its new position.
    static NodeRef row_of(const Item& it) {
        return memo(it.id, it.text, it.done, it.prio, [&]{
            const char* dots = it.prio == 0 ? "\u00b7" : it.prio == 1 ? ":" : it.prio == 2 ? "\u2237" : "\u2237\u00b7";
            return row(
                box(it.done ? icon("check", 16) | fg(emerald) : box() | w(16) | h(16)
                        | css("border","2px solid " + std::string(it.done?"#34d399":"#475569")) | round(999))
                    | pointer | tap(Toggle{ it.id }),
                text(it.text) | fg(it.done ? muted : ink) | (it.done ? strike : noop) | grow(1),
                text(dots) | fg(amber) | mono | pointer | tap(Bump{ it.id }) | title("bump priority"),
                icon_button_i("x", Remove{ it.id }, Variant::ghost)
            ) | gap(12) | center | pad_x(14) | pad_y(12) | round(12)
              | bg(slate800) | border_token()
              | key("todo-" + std::to_string(it.id))   // identity for the keyed diff
              | animated();                              // FLIP: glide on move/add
        });
    }

    static NodeRef view(const Model& m) {
        // sort + filter a copy — pure, deterministic; the diff + FLIP do the rest.
        auto items = m.items;
        if (m.sort == 0) std::sort(items.begin(), items.end(), [](auto& a, auto& b){ return a.prio > b.prio; });
        else             std::sort(items.begin(), items.end(), [](auto& a, auto& b){ return a.text < b.text; });

        std::vector<NodeRef> rows;
        for (auto& it : items) {
            if (m.hide_done && it.done) continue;
            rows.push_back(row_of(it));
        }
        auto list = col(); list->kids = std::move(rows); finalize(*list);

        auto controls = row(
            button("+ Add", Add{}),
            push(),
            button(m.sort == 0 ? "Sort: priority" : "Sort: name", SortBy{ m.sort == 0 ? 1 : 0 }, Variant::secondary),
            button(m.hide_done ? "Show done" : "Hide done", HideDone{}, Variant::ghost)
        ) | gap(8) | center;

        return col(
            col(text("Living list") | font(32) | bold | fg(ink),
                text("Memoised rows, keyed identity, FLIP motion. Add / remove / "
                     "reorder / filter \u2014 every change glides.") | fg(muted) | leading(1.5f)) | gap(6),
            controls,
            list | gap(10)
        ) | gap(20) | pad(28) | max_w(rem(30)) | css("margin", "6vh auto")
          | css("min-height", "100dvh") | theme(midnight());
    }
};

int main() { return live<Living>({ .port = 8080, .title = "waya \u00b7 living list" }); }
