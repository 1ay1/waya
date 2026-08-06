/// examples/kanban.cpp — the showcase: a live, multi-client kanban board.
///
///   cmake --build build -j && ./build/kanban    # open TWO tabs on :8080
///
/// Everything the framework can do, in one app:
///   • drag & drop     — drag a card between columns (keyed move-diffing keeps
///                       each card's DOM; only the moved node is patched)
///   • keyboard        — type in a column's box and press Enter to add a card
///   • each / when     — declarative lists and conditional UI
///   • broadcast       — every board change syncs to all open tabs, instantly
///   • components      — a card, a column: just functions returning NodeRef
///
/// Not a line of HTML, CSS, onclick, or drag API. Pure `update` + a vocabulary.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

struct Kanban {
    struct Card { long id; std::string text; int col; };
    struct Model {
        std::vector<Card> cards = {
            {1,"Design the vocabulary",0}, {2,"Ship effects",1}, {3,"Ship broadcast",2},
            {4,"Keyboard + drag",0},
        };
        std::string draft[3];      // per-column new-card draft
        long next_id = 5;
    };
    using Msg = int;
    // per-column drafts: DraftCol0..2 ; AddCol0..2 ; Move ; Recv(sync)
    enum { Draft0, Draft1, Draft2, Add0, Add1, Add2, Move, Recv };

    static const char* col_name(int c){ return c==0?"Todo":c==1?"Doing":"Done"; }

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        auto sync = [](const Model& mm){ return Cmd<Msg>::broadcast("board", serialize(mm)); };
        switch (msg) {
            case Draft0: m.draft[0] = value; return { m, Cmd<Msg>::none() };
            case Draft1: m.draft[1] = value; return { m, Cmd<Msg>::none() };
            case Draft2: m.draft[2] = value; return { m, Cmd<Msg>::none() };
            case Add0: case Add1: case Add2: {
                int c = msg - Add0;
                if (m.draft[c].empty()) return { m, Cmd<Msg>::none() };
                m.cards.push_back({m.next_id++, m.draft[c], c});
                m.draft[c].clear();
                return { m, sync(m) };
            }
            case Move: {
                // value = "cardId:targetCol" (dropped card id + destination).
                auto colon = value.find(':');
                if (colon == std::string::npos) return { m, Cmd<Msg>::none() };
                long id = std::atol(value.substr(0, colon).c_str());
                int  to = std::atoi(value.substr(colon + 1).c_str());
                for (auto& c : m.cards) if (c.id == id) c.col = to;
                return { m, sync(m) };
            }
            case Recv: { Model n = deserialize(value); n.draft[0]=m.draft[0]; n.draft[1]=m.draft[1]; n.draft[2]=m.draft[2]; return { n, Cmd<Msg>::none() }; }
        }
        return { m, Cmd<Msg>::none() };
    }

    // Board state travels as a topic payload so every tab stays in sync.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_topic("board", [](std::string){ return Recv; });
    }

    // ── "components": each is just a function returning a NodeRef ──────────────
    static NodeRef card_view(const Card& c) {
        return row(text(c.text) | fg(ink) | font(15))
             | key("card:" + std::to_string(c.id))
             | draggable(std::to_string(c.id))        // the drag payload = card id
             | pad_x(14) | pad_y(12) | round(10) | bg(bg2) | border(1, line)
             | css("cursor", "grab") | transition()
             | on(Hover, border(1, brand));
    }

    static NodeRef column(const Model& m, int c) {
        int add_msg   = Add0 + c;
        int draft_msg = Draft0 + c;
        return col(
            row(
                text(col_name(c)) | fg(muted) | font(13) | weight(Weight::bold)
                    | css("letter-spacing", "0.05em"),
                text(std::to_string(std::count_if(m.cards.begin(), m.cards.end(),
                     [&](const Card& x){ return x.col == c; }))) | fg(faint) | font(13)
            ) | css("justify-content", "space-between"),

            // the cards in this column — keyed, so drag-reorder is a move, not a repaint
            col_(each_keyed(
                [&]{ std::vector<const Card*> cs; for (auto& x : m.cards) if (x.col==c) cs.push_back(&x); return cs; }(),
                [](const Card* x){ return "card:" + std::to_string(x->id); },
                [](const Card* x){ return card_view(*x); }
            )) | gap(8) | css("min-height", "3rem") | css("flex", "1 1 auto"),

            input(m.draft[c]) | placeholder("+ add, press Enter") | on_input(draft_msg)
                | on_enter(add_msg)
                | fg(ink) | bg(bg0) | pad_x(12) | pad_y(9) | round(8) | border(1, line) | font(14)

        ) | gap(12) | pad(14) | round(14) | bg(bg1) | border(1, line)
          | on_drop(Move)                            // drop a card here → Move
          | attr("data-drop-arg", std::to_string(c))  // the target column, sent with the drop
          | role("list") | aria("label", col_name(c))
          | css("width", "16rem") | css("min-height", "20rem")
          | on_phone(css("width", "100%"), css("min-height", "auto"));
    }

    static NodeRef view(const Model& m) {
        return col(
            row(
                text("waya kanban") | fg(ink) | font(28) | weight(Weight::black),
                text("drag cards \u00b7 Enter to add \u00b7 open a 2nd tab \u2192 live sync")
                    | fg(faint) | font(13)
            ) | gap(16) | css("align-items", "baseline"),

            row_(each([]{ return std::vector<int>{0,1,2}; }(),
                     [&](int c){ return column(m, c); }))
                | gap(16) | css("align-items", "flex-start")
                // Mobile: stack the columns vertically and let each fill width.
                // Desktop: keep them side by side, scrolling horizontally if tight.
                | on_phone(waya::surface::column, css("align-items", "stretch"))
                | on_desktop(css("overflow-x", "auto"))

        ) | gap(24) | pad(36) | css("min-height", "100vh") | bg(bg0);
    }

    // ── tiny serializer so the whole board rides one broadcast payload ────────
    static std::string serialize(const Model& m) {
        std::string s;
        for (auto& c : m.cards) {
            s += std::to_string(c.id) + '\x1f' + std::to_string(c.col) + '\x1f' + c.text + '\x1e';
        }
        return s;
    }
    static Model deserialize(const std::string& s) {
        Model m; m.cards.clear(); long maxid = 0;
        std::size_t i = 0;
        while (i < s.size()) {
            auto rec = s.find('\x1e', i); if (rec == std::string::npos) break;
            std::string r = s.substr(i, rec - i); i = rec + 1;
            auto a = r.find('\x1f'); auto b = r.find('\x1f', a+1);
            if (a==std::string::npos || b==std::string::npos) continue;
            long id = std::atol(r.substr(0,a).c_str());
            int col = std::atoi(r.substr(a+1, b-a-1).c_str());
            m.cards.push_back({id, r.substr(b+1), col});
            maxid = std::max(maxid, id);
        }
        m.next_id = maxid + 1;
        return m;
    }
};

int main() {
    static_assert(SurfaceProgram<Kanban>);
    return live<Kanban>({.port = 8080});
}
