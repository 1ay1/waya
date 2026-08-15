#pragma once
/// \file ui/kanban.hpp
/// kanban — a drag-and-drop board of columns, each a stack of cards.
///
/// `reorderable` reorders ONE list. A kanban is the 2-D generalisation: N
/// columns, each an ordered list of cards, and a card can be dragged BETWEEN
/// columns (To-do → Doing → Done) as well as within one. The drag primitives
/// deliver `"<payload>:<drop-arg>"`, so a card's payload encodes its
/// `"srcCol.srcIndex"` and each column body tags its own `destCol` — on drop
/// the app learns exactly which card went where, and `apply_kanban_move`
/// splices the two lists for you.
///
///   struct Card { std::string id, title; };
///   struct Board { std::vector<KanbanColumn<Card>> columns; };
///   struct Moved { int fromCol, fromIdx, toCol; };
///
///   // update:
///   [&](Moved mv){ apply_kanban_move(m.board.columns, mv.fromCol, mv.fromIdx, mv.toCol);
///                  return {m, Cmd::none()}; }
///
///   // view:
///   kanban(m.board.columns,
///       [](const Card& c){ return card(text(c.title)); },   // render one card
///       [](int fc, int fi, int tc){ return Moved{fc, fi, tc}; });
///
/// The move is pure data (`apply_kanban_move` is unit-testable on plain
/// vectors); dropping onto a column appends to its end. Nothing here mutates
/// your board — the view only reports the drop as a Msg.

#include "../surface/node.hpp"
#include "components.hpp"

#include <functional>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── model ─────────────────────────────────────────────────────────────────────
/// One column: a title and its ordered cards. `T` is your card type (any value).
template <typename T>
struct KanbanColumn {
    std::string title;
    std::vector<T> cards;
    std::uint32_t accent = 0x6366f1;   // the column's header/tint colour
    bool operator==(const KanbanColumn&) const = default;
};

/// `kcolumn("To do", {card1, card2}, accent)` — build a column.
template <typename T>
inline KanbanColumn<T> kcolumn(std::string title, std::vector<T> cards = {}, std::uint32_t accent = 0x6366f1){
    return { std::move(title), std::move(cards), accent };
}

// ── the pure move ─────────────────────────────────────────────────────────────
/// Move the card at `columns[fromCol][fromIdx]` to the END of `columns[toCol]`.
/// No-op on any out-of-range index. When moving WITHIN the same column, the card
/// lands at the end (a "send to bottom" — parse a target index if you want mid
/// drops). Pure: test it on plain vectors.
template <typename T>
inline void apply_kanban_move(std::vector<KanbanColumn<T>>& columns,
                              int fromCol, int fromIdx, int toCol){
    int nc = (int)columns.size();
    if (fromCol < 0 || fromCol >= nc || toCol < 0 || toCol >= nc) return;
    auto& src = columns[fromCol].cards;
    if (fromIdx < 0 || fromIdx >= (int)src.size()) return;
    T moved = std::move(src[fromIdx]);
    src.erase(src.begin() + fromIdx);
    columns[toCol].cards.push_back(std::move(moved));
}

namespace kanban_detail {
/// Parse a `"srcCol.srcIdx:destCol"` drop payload into {fromCol, fromIdx,
/// toCol}. Returns {-1,-1,-1} on anything malformed (a bad drag drops safely).
struct Move3 { int fromCol = -1, fromIdx = -1, toCol = -1; };
inline Move3 parse_move(const std::string& payload){
    auto colon = payload.rfind(':');
    if (colon == std::string::npos) return {};
    std::string left = payload.substr(0, colon);      // "srcCol.srcIdx"
    std::string right = payload.substr(colon + 1);    // "destCol"
    auto dot = left.find('.');
    if (dot == std::string::npos) return {};
    auto digits = [](const std::string& s){
        if (s.empty()) return false;
        for (char c : s) if (c < '0' || c > '9') return false;
        return true;
    };
    std::string a = left.substr(0, dot), b = left.substr(dot + 1);
    if (!digits(a) || !digits(b) || !digits(right)) return {};
    return { std::stoi(a), std::stoi(b), std::stoi(right) };
}
} // namespace kanban_detail

// ── view ──────────────────────────────────────────────────────────────────────
/// `kanban(columns, renderCard, onMove, minColW)` — the board. `renderCard(T)`
/// draws one card's inner content; each card is made draggable (payload
/// `"col.index"`), each column body a drop zone (tagged with its index). A drop
/// fires `onMove(fromCol, fromIdx, toCol)` — feed that to `apply_kanban_move`.
/// Columns lay out as equal-width flex tracks that scroll horizontally if the
/// board is wider than the viewport.
template <typename T, typename RenderCard, typename OnMove>
inline NodeRef kanban(const std::vector<KanbanColumn<T>>& columns,
                      RenderCard renderCard, OnMove onMove, float minColW = 260){
    using Msg = decltype(onMove(0, 0, 0));
    std::vector<NodeRef> cols;
    cols.reserve(columns.size());

    for (int ci = 0; ci < (int)columns.size(); ++ci){
        const auto& kc = columns[ci];
        std::vector<NodeRef> cards;
        cards.reserve(kc.cards.size());
        for (int i = 0; i < (int)kc.cards.size(); ++i){
            std::string payload = std::to_string(ci) + "." + std::to_string(i);
            cards.push_back(
                box(renderCard(kc.cards[i]))
                    | draggable(payload)
                    | grab
                    | detail::raw_css("margin-bottom", "8px")
                    | key("kb-" + payload));
        }
        // the column body is the drop zone: dropping here appends to column ci.
        auto body = box();
        body->kids = std::move(cards);
        body->style.flow = Flow::col;
        finalize(*body);
        body = body
            | grows | detail::raw_css("min-height", "40px")
            | drop_target(std::to_string(ci), [onMove](std::string p) -> Msg {
                  auto mv = kanban_detail::parse_move(p);
                  return onMove(mv.fromCol, mv.fromIdx, mv.toCol);
              })
            | role("list") | aria_label(kc.title + " cards");

        auto header = row(
                box() | w(8) | h(8) | round(999) | detail::raw_css("background", detail::hexstr(kc.accent)),
                text(kc.title) | fg_text | semibold | text_size(13.5f),
                box() | grows,
                text(std::to_string(kc.cards.size()))
                    | fg_muted | text_size(12)
                    | pad_x(7) | pad_y(2) | round(999)
                    | detail::raw_css("background", "var(--wa-raised, rgba(255,255,255,.06))"))
            | items_center | gap(8) | pad_y(4) | pad_x(2)
            | detail::raw_css("margin-bottom", "10px");

        cols.push_back(
            col(header, body)
                | pad(12) | round(12)
                | detail::raw_css("background", "var(--wa-bg, rgba(0,0,0,.20))")
                | detail::raw_css("border", "1px solid var(--wa-line, rgba(255,255,255,.08))")
                | detail::raw_css("flex", "1 1 " + detail::numstr(minColW) + "px")
                | detail::raw_css("min-width", detail::numstr(minColW) + "px")
                | role("group") | aria_label(kc.title));
    }

    auto board = box();
    board->kids = std::move(cols);
    board->style.flow = Flow::row;
    finalize(*board);
    return board
        | gap(14) | w_full | items_start
        | detail::raw_css("overflow-x", "auto")
        | role("list") | aria_label("Kanban board");
}

} // namespace waya::ui
