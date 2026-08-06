// examples/palette.cpp — a command palette (Cmd+K), the "wait, C++ did that?"
// demo. A global keyboard shortcut opens a centered, autofocused, filterable
// command list with arrow-key navigation and Enter to run — all from three pure
// functions and the mod vocabulary. No JS, no client state.
//
//   cmake --build build --target palette && ./build/palette   # localhost:8080

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;
using namespace waya::color;

struct Palette {
    struct Cmd { std::string label; std::string icon; std::string ran; };

    struct Model {
        bool open = false;
        std::string query;
        int sel = 0;
        std::string last;   // the command that was run
        std::vector<Cmd> commands{
            {"New file",        "plus",     ""},
            {"Open settings",   "settings", ""},
            {"Toggle theme",    "star",     ""},
            {"Search docs",     "search",   ""},
            {"Invite teammate", "user",     ""},
            {"Sign out",        "external", ""},
        };
        std::vector<Cmd> filtered() const {
            std::vector<Cmd> out;
            for (auto& c : commands) {
                std::string l = c.label, q = query;
                std::transform(l.begin(), l.end(), l.begin(), ::tolower);
                std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                if (q.empty() || l.find(q) != std::string::npos) out.push_back(c);
            }
            return out;
        }
    };

    struct Open {}; struct Close {}; struct Type { std::string q; };
    struct Move { int d; }; struct Run {};
    using Msg = std::variant<Open, Close, Type, Move, Run>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Open)  { m.open = true; m.query = ""; m.sel = 0; },
            [&](Close) { m.open = false; },
            [&](const Type& t) { m.query = t.q; m.sel = 0; },
            [&](const Move& mv) {
                int n = (int)m.filtered().size();
                if (n) m.sel = (m.sel + mv.d + n) % n;
            },
            [&](Run) {
                auto f = m.filtered();
                if (!f.empty() && m.sel < (int)f.size()) m.last = f[m.sel].label;
                m.open = false;
            },
        }, msg);
        return m;
    }

    static NodeRef view(const Model& m) {
        auto trigger = row(
            text("Press ") | fg(muted),
            text("⌘K") | fg(ink) | bg(rgba(255,255,255,0.08f)) | pad_x(8) | pad_y(3) | round(6) | mono,
            text(" to open the command palette") | fg(muted)
        ) | gap(6) | center
          // the global shortcut lives on a mounted node; opens from anywhere.
          | on_shortcut("mod+k", Open{});

        auto last = m.last.empty()
            ? box()
            : row(icon("check", 18) | fg(emerald), text("Ran: " + m.last) | fg(ink))
                | gap(8) | center | pad(12) | round(10) | bg(slate800) | fade_up(200);

        NodeRef modal = box();
        if (m.open) {
            auto f = m.filtered();
            std::vector<NodeRef> rows;
            for (int i = 0; i < (int)f.size(); ++i) {
                bool on = i == m.sel;
                rows.push_back(
                    row(icon(f[i].icon, 18) | fg(on ? ink : muted),
                        text(f[i].label) | fg(on ? ink : muted) | (on ? semibold : medium))
                    | gap(12) | center | pad_x(12) | pad_y(10) | round(8)
                    | (on ? bg(rgba(99,102,241,0.18f)) : bg(transparent))
                    | tap(Run{}));   // (sel already set by hover would need Move; click runs)
            }
            auto list = box(); list->kids = std::move(rows); list->style.flow = Flow::col; finalize(*list);

            auto panel = col(
                row(icon("search", 18) | fg(muted),
                    input(m.query) | placeholder("Type a command...")
                        | autofocus() | on_input([](std::string v){ return Type{v}; })
                        | grow(1) | css("background","transparent") | css("border","none")
                        | css("outline","none") | fg(ink) | font(16))
                    | gap(10) | center | pad(14)
                    | css("border-bottom","1px solid rgba(255,255,255,.08)"),
                list | pad(8) | scroll_y | max_h(340)
            )
            | w(rem(34)) | css("max-width","92vw") | round(16) | clip
            | bg(slate900) | css("border","1px solid rgba(255,255,255,.1)")
            | elevation(4) | pop_in(160)
            | stop()
            // arrow keys move the selection, Enter runs, Esc closes.
            | on_key("ArrowDown", Move{ +1 }) | on_key("ArrowUp", Move{ -1 })
            | on_enter(Run{}) | on_escape(Close{});

            modal = overlay(panel) | tap(Close{});
        }

        return col(
            col(text("\u2318 Command Palette") | font(40) | bold | fg(ink),
                text("A Cmd+K palette \u2014 global shortcut, autofocus, arrow-key nav, "
                     "live filter. Pure functions, zero client state.") | fg(muted) | max_w(rem(34)) | leading(1.6f),
                trigger,
                last) | gap(20) | center,
            modal
        ) | center | gap(0) | h(vh(100)) | pad(32)
          | bg(rgba(11,16,32,1.0f)) | theme(midnight());
    }
};

int main() { return live<Palette>({ .port = 8080, .title = "waya \u00b7 \u2318K palette" }); }
