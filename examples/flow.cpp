/// examples/flow.cpp — FLOW: a kanban board. Move a card between Todo / Doing /
/// Done and it glides to its new column instead of blinking — because the rows
/// are KEYED, so waya's diff emits a MOVE, not a re-render, and the client FLIPs
/// it into place. Live per-column counts, a tiny progress ring, and hover
/// affordances. All state is a plain vector<Card> in the Model.
///
///   waya run flow             # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Board {
    enum Lane { Todo = 0, Doing = 1, Done = 2 };

    struct Card {
        int           id;
        std::string   title;
        int           col;
        std::uint32_t hue;
    };

    struct Model {
        std::vector<Card> cards = {
            { 1, "Design the landing hero", Todo,  0x22d3ee },
            { 2, "Wire the WebSocket diff", Doing, 0xa78bfa },
            { 3, "Keyed list reconciler",   Done,  0x34d399 },
            { 4, "Gradient token system",   Todo,  0xf472b6 },
            { 5, "Ship the CLI picker",     Doing, 0xfbbf24 },
            { 6, "Write six pretty demos",  Todo,  0x60a5fa },
            { 7, "Server-side rendering",   Done,  0x34d399 },
        };
        int next_id = 8;
    };

    // move card `id` one column right (wraps Done -> Todo)
    struct Advance { int id; };
    struct Back    { int id; };
    struct Add     { int col; };
    using Msg = std::variant<Advance, Back, Add>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Advance a) {
                for (auto& c : m.cards) if (c.id == a.id) c.col = (c.col + 1) % 3;
            },
            [&](Back b) {
                for (auto& c : m.cards) if (c.id == b.id) c.col = (c.col + 2) % 3;
            },
            [&](Add a) {
                static const char* seed[] = {
                    "New idea", "Follow up", "Investigate", "Polish pass", "Refactor" };
                m.cards.push_back({ m.next_id, seed[m.next_id % 5], a.col,
                                    0x818cf8 + (std::uint32_t)(m.next_id * 0x151a2b) });
                m.next_id++;
            },
        }, msg);
        return m;
    }

    static const char* col_name(int c) {
        static const char* n[] = { "Todo", "Doing", "Done" };
        return n[c];
    }
    static std::uint32_t col_accent(int c) {
        static const std::uint32_t a[] = { 0x64748b, 0x6366f1, 0x34d399 };
        return a[c];
    }

    static NodeRef card_view(const Card& c) {
        // KEY is the card id -> the diff tracks it across columns as a MOVE.
        return col(
            row(box() | circle(9) | bg(c.hue) | glow(c.hue, 10),
                text(c.title) | fg(ink) | font(14) | semibold | leading(1.3f),
                box() | grow()) | gap(10) | items_start,
            row(text("#" + std::to_string(c.id)) | fg(faint) | font(12) | tabular_nums,
                box() | grow(),
                // back / forward controls
                text("\u2039") | font(18) | fg(muted) | pad_x(8) | round(7)
                    | interactive() | hover_bg(0xffffff, 0.08f) | tap(Back{ c.id }),
                text("\u203A") | font(18) | fg(muted) | pad_x(8) | round(7)
                    | interactive() | hover_bg(0xffffff, 0.08f) | tap(Advance{ c.id }))
                | gap(4) | items_center
        ) | gap(10) | pad(14) | round(14)
          | detail::raw_css("background",
              "linear-gradient(180deg, rgba(255,255,255,.05), rgba(255,255,255,.02))")
          | hairline(0xffffff, 0.10f)
          | detail::raw_css("border-left", "3px solid " + detail::hexstr(c.hue))
          | hover_lift(3)
          | transition("transform .18s cubic-bezier(.2,.7,.2,1), box-shadow .18s ease")
          | key(std::to_string(c.id));
    }

    static NodeRef column(const Model& m, int lane) {
        std::vector<NodeRef> cards;
        int count = 0;
        for (auto& c : m.cards) if (c.col == lane) { cards.push_back(card_view(c)); count++; }

        auto head = row(
            row(box() | circle(8) | bg(col_accent(lane)),
                text(col_name(lane)) | fg(ink) | font(15) | semibold) | gap(9) | items_center,
            box() | grow(),
            text(std::to_string(count)) | fg(muted) | font(13) | semibold | tabular_nums
                | pad_x(9) | pad_y(2) | pill | tint(0xffffff, 0.08f)
        ) | items_center;

        auto add = row(text("+") | font(16) | fg(muted),
                       text("Add card") | font(13) | fg(muted) | semibold)
                 | gap(8) | center | pad_y(10) | round(11)
                 | detail::raw_css("border", "1px dashed rgba(255,255,255,.16)")
                 | interactive() | hover_bg(0xffffff, 0.05f) | tap(Add{ lane });

        // the keyed list of cards; MOVE animation comes for free
        auto list = col_(std::move(cards)) | gap(12);

        return col(head, list, add) | gap(14) | pad(16) | round(18) | w(300)
             | detail::raw_css("background", "rgba(255,255,255,.025)")
             | hairline(0xffffff, 0.07f) | grow();
    }

    static NodeRef view(const Model& m) {
        int done = 0;
        for (auto& c : m.cards) if (c.col == Done) done++;
        int total = (int)m.cards.size();
        float pct = total ? 100.f * done / total : 0;

        auto header = row(
            row(box(text("\u25C8") | font(20) | fg(0xffffff))
                    | square(40) | center | round(11)
                    | gradient(0x8b5cf6, 0x6366f1, 135) | glow(0x8b5cf6, 18),
                col(text("Flow") | fg(ink) | font(22) | weight(Weight::black) | leading(1.f),
                    text("keyed board \u00b7 cards glide, not blink") | fg(faint) | font(12))
                    | gap(1)) | gap(12) | items_center,
            box() | grow(),
            col(row(text(std::to_string(done) + "/" + std::to_string(total) + " done")
                        | fg(muted) | font(13) | semibold | tabular_nums,
                    box() | grow(),
                    text(std::to_string((int)pct) + "%") | fg(0x34d399) | font(13) | semibold)
                    | gap(12) | items_center | w(200),
                box(box() | h(6) | round(999)
                        | detail::raw_css("width", std::to_string((int)pct) + "%")
                        | gradient(0x34d399, 0x22d3ee, 90)
                        | transition("width .4s ease"))
                    | w(200) | h(6) | round(999) | tint(0xffffff, 0.10f)
                    | clip_content) | gap(7)
        ) | items_center | w_full;

        auto board = row(column(m, Todo), column(m, Doing), column(m, Done))
                   | gap(18) | items_start | wrap;

        return col(header, board)
             | gap(24) | pad(28) | max_w(1120) | center_x | min_h(100_vh)
             | radial(0x1a1030, 15, -10, 0x0b1020)
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Flow";
        mt.description = "A keyed kanban board \u2014 cards glide between columns via move-diffing.";
        return mt;
    }
};

int main() {
    return live<Board>({ .port = 8080, .title = "waya \u00b7 flow" });
}
