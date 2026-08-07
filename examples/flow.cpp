/// examples/flow.cpp — FLOW: a kanban board that feels native. Move a card and
/// it GLIDES to its new column instead of blinking, because rows are keyed and
/// the diff emits a MOVE, not a re-render. Clean columns, priority accents, a
/// progress header, crisp SVG controls. State is a plain vector<Card>.
///
///   waya run flow             # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Board {
    enum Lane { Todo = 0, Doing = 1, Done = 2 };

    static constexpr std::uint32_t bg     = 0x0a0d16;
    static constexpr std::uint32_t col_bg = 0x0d1119;
    static constexpr std::uint32_t card_c = 0x131926;
    static constexpr std::uint32_t line   = 0x1e2634;
    static constexpr std::uint32_t ink    = 0xeef2f8;
    static constexpr std::uint32_t body_c = 0x8b98af;
    static constexpr std::uint32_t faint  = 0x566579;
    static constexpr std::uint32_t good   = 0x2ee6a6;

    struct Card { int id; std::string title; int col; int prio; };  // prio 0 low 1 med 2 high

    struct Model {
        std::vector<Card> cards = {
            { 1, "Design the landing hero",     Todo,  2 },
            { 2, "WebSocket diff transport",    Doing, 2 },
            { 3, "Keyed list reconciler",       Done,  1 },
            { 4, "Gradient token system",       Todo,  1 },
            { 5, "Ship the CLI picker",         Doing, 0 },
            { 6, "Write six polished demos",    Todo,  2 },
            { 7, "Server-side rendering core",  Done,  2 },
            { 8, "Docs: getting started",       Todo,  0 },
        };
        int next_id = 9;
    };

    struct Advance { int id; }; struct Back { int id; }; struct Add { int col; };
    using Msg = std::variant<Advance, Back, Add>;

    static Model init() { return {}; }
    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Advance a) { for (auto& c : m.cards) if (c.id == a.id) c.col = (c.col + 1) % 3; },
            [&](Back b)    { for (auto& c : m.cards) if (c.id == b.id) c.col = (c.col + 2) % 3; },
            [&](Add a) {
                static const char* seed[] = { "New task", "Follow up", "Investigate", "Polish pass", "Refactor" };
                m.cards.push_back({ m.next_id, seed[m.next_id % 5], a.col, m.next_id % 3 });
                m.next_id++;
            },
        }, msg);
        return m;
    }

    static const char* lane_name(int c) { static const char* n[] = { "Backlog", "In progress", "Done" }; return n[c]; }
    static std::uint32_t lane_dot(int c) { static const std::uint32_t d[] = { 0x566579, 0x6d7cff, 0x2ee6a6 }; return d[c]; }
    static std::uint32_t prio_color(int p) { static const std::uint32_t c[] = { 0x566579, 0xf0b429, 0xff6b81 }; return c[p]; }
    static const char* prio_name(int p) { static const char* n[] = { "Low", "Medium", "High" }; return n[p]; }

    static Mod border(std::uint32_t c = line) { return detail::raw_css("border", "1px solid " + detail::hexstr(c)); }

    static NodeRef ctrl(std::string ico, Msg msg) {
        return box(icon(ico, 15) | fg(body_c))
             | square(28) | center | round(7) | pointer
             | on(Hover, detail::raw_css("background", "rgba(255,255,255,.06)"))
             | on(Hover, fg(ink))
             | tap(msg) | transition("background-color .15s ease");
    }

    static NodeRef card_view(const Card& c) {
        return col(
            text(c.title) | fg(ink) | font(14) | weight(Weight::semibold) | leading(1.4f),
            row(row(box() | circle(6) | detail::raw_css("background", detail::hexstr(prio_color(c.prio))),
                    text(prio_name(c.prio)) | fg(body_c) | font(12) | weight(Weight::medium)) | gap(6) | items_center,
                text("#" + std::to_string(c.id)) | fg(faint) | font(12) | tabular_nums,
                box() | grow(),
                ctrl("chevron-left", Back{ c.id }),
                ctrl("chevron-right", Advance{ c.id })) | gap(4) | items_center
        ) | gap(12) | pad(14) | round(12)
          | detail::raw_css("background", "#131926")
          | border()
          | detail::raw_css("border-left", "3px solid " + detail::hexstr(prio_color(c.prio)))
          | on(Hover, detail::raw_css("border-color", "#2a3446"))
          | transition("border-color .15s ease, transform .15s ease")
          | on(Hover, detail::raw_css("transform", "translateY(-2px)"))
          | key(std::to_string(c.id));
    }

    static NodeRef column(const Model& m, int lane) {
        std::vector<NodeRef> cards; int count = 0;
        for (auto& c : m.cards) if (c.col == lane) { cards.push_back(card_view(c)); count++; }

        auto head = row(
            row(box() | circle(8) | detail::raw_css("background", detail::hexstr(lane_dot(lane))),
                text(lane_name(lane)) | fg(ink) | font(14) | weight(Weight::bold)) | gap(9) | items_center,
            box() | grow(),
            text(std::to_string(count)) | fg(body_c) | font(12) | weight(Weight::bold) | tabular_nums
                | pad_x(8) | pad_y(2) | round(6) | detail::raw_css("background", "rgba(255,255,255,.05)")
        ) | items_center;

        auto add = row(icon("plus", 15) | fg(faint), text("Add card") | font(13) | fg(faint) | weight(Weight::medium))
                 | gap(7) | center | pad_y(11) | round(10)
                 | detail::raw_css("border", "1px dashed #232c3d")
                 | pointer | on(Hover, detail::raw_css("background", "rgba(255,255,255,.03)"))
                 | tap(Add{ lane }) | transition("background-color .15s ease");

        return col(head, col_(std::move(cards)) | gap(10), add)
             | gap(14) | pad(14) | round(14) | w(300)
             | detail::raw_css("background", "#0d1119") | border(0x181f2c)
             | detail::raw_css("align-self", "flex-start");
    }

    static NodeRef view(const Model& m) {
        int done = 0; for (auto& c : m.cards) if (c.col == Done) done++;
        int total = (int)m.cards.size();
        int pct = total ? 100 * done / total : 0;

        auto header = row(
            row(box(icon("home", 18) | fg(0xffffff)) | square(34) | center | round(9)
                    | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)"),
                col(text("Flow") | fg(ink) | font(20) | weight(Weight::black)
                        | detail::raw_css("letter-spacing", "-0.02em"),
                    text("Product roadmap") | fg(faint) | font(13)) | gap(1)) | gap(12) | items_center,
            box() | grow(),
            col(row(text(std::to_string(done) + " of " + std::to_string(total) + " complete")
                        | fg(body_c) | font(13) | weight(Weight::medium) | tabular_nums,
                    box() | grow(),
                    text(std::to_string(pct) + "%") | fg(good) | font(13) | weight(Weight::bold)) | items_center | w(220),
                box(box() | h(6) | round(999)
                        | detail::raw_css("width", std::to_string(pct) + "%")
                        | detail::raw_css("background", "linear-gradient(90deg,#2ee6a6,#00d4ff)")
                        | transition("width .4s ease"))
                    | w(220) | h(6) | round(999) | detail::raw_css("background", "rgba(255,255,255,.08)")
                    | detail::raw_css("overflow", "hidden")) | gap(8)
        ) | items_center | w_full | wrap | gap(20);

        auto board = row(column(m, Todo), column(m, Doing), column(m, Done))
                   | gap(16) | items_start | wrap;

        return col(header, board)
             | gap(28) | pad(32) | max_w(1080) | center_x | min_h(100_vh)
             | detail::raw_css("background",
                 "radial-gradient(1000px 500px at 20% -10%, rgba(109,124,255,.10), transparent 60%), #0a0d16")
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Flow \u00b7 board";
        mt.description = "A keyed kanban board \u2014 cards glide between columns via move-diffing.";
        return mt;
    }
};

int main() { return live<Board>({ .port = 8080, .page_bg = 0x0a0d16, .title = "Flow" }); }
