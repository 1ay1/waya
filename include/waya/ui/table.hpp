#pragma once
/// \file ui/table.hpp
/// Table<Row> — sort / filter / paginate a data table, all as model state.
///
/// `data_table` (widgets.hpp) draws a static grid. A real table is interactive:
/// click a header to sort, type to filter, page through thousands of rows. Each
/// of those is STATE — a sort key + direction, a filter string, a page index —
/// that apps otherwise scatter across the model and re-derive by hand.
///
/// `TableState` holds that little bit of view state as one value; a
/// `TableColumn` describes each column (how to render its cell, how to sort by
/// it, whether it's searchable); and `data_table(rows, columns, state, onSort,
/// onPage)` derives the visible slice and renders sortable headers + a pager.
///
///   struct Model { std::vector<User> users; TableState table; };
///
///   static std::vector<TableColumn<User>> columns(){
///       return {
///           col<User>("Name",  [](const User& u){ return text(u.name); })
///               .sortable([](const User& a, const User& b){ return a.name < b.name; })
///               .searchable([](const User& u){ return u.name; }),
///           col<User>("Score", [](const User& u){ return text(std::to_string(u.score)); })
///               .sortable([](const User& a, const User& b){ return a.score < b.score; }),
///       };
///   }
///   // update: SortBy{col_index}, SetFilter{text}, GoPage{n} mutate m.table
///   // view:
///   data_table(m.users, columns(), m.table, SortBy{}, GoPage{})
///
/// The derivation (filter -> sort -> page) is a pure free function you can test
/// on plain data; the view is that result plus wired headers and a pager.

#include "../surface/node.hpp"
#include "components.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// The interactive state of a table: which column is sorted (and direction),
/// the filter text, and the current page. One value in your model.
struct TableState {
    int  sort_col = -1;        // index of the sorted column (-1 = insertion order)
    bool sort_desc = false;    // descending when true
    std::string filter;        // free-text search across searchable columns
    int  page = 0;             // zero-based page index
    int  page_size = 0;        // rows per page (0 = no pagination)
    bool operator==(const TableState&) const = default;

    /// Toggle sorting on `col`: first click ascending, second descending, and
    /// clicking a NEW column starts ascending. Resets to page 0 (the sorted top).
    void sort_by(int col){
        if (sort_col == col) sort_desc = !sort_desc;
        else { sort_col = col; sort_desc = false; }
        page = 0;
    }
    /// Set the filter and jump to page 0 (the filtered results start fresh).
    void set_filter(std::string q){ filter = std::move(q); page = 0; }
    void go_page(int p){ page = p < 0 ? 0 : p; }
};

/// One column: a header, a cell renderer, and OPTIONAL sort + search accessors.
/// A column with no `less` isn't sortable; with no `text` isn't searched.
template <typename Row>
struct TableColumn {
    std::string header;
    std::function<NodeRef(const Row&)> cell;
    std::function<bool(const Row&, const Row&)> less;   // sort comparator (empty = not sortable)
    std::function<std::string(const Row&)> text;        // searchable text (empty = not searched)

    TableColumn& sortable(std::function<bool(const Row&, const Row&)> cmp){ less = std::move(cmp); return *this; }
    TableColumn& searchable(std::function<std::string(const Row&)> get){ text = std::move(get); return *this; }
    [[nodiscard]] bool is_sortable() const { return (bool)less; }
    [[nodiscard]] bool is_searchable() const { return (bool)text; }
};

/// `col<Row>("Header", cellFn)` — start a column (chain `.sortable`/`.searchable`).
template <typename Row>
inline TableColumn<Row> col(std::string header, std::function<NodeRef(const Row&)> cell){
    return TableColumn<Row>{ std::move(header), std::move(cell), {}, {} };
}

// ── derivation (pure) ─────────────────────────────────────────────────────────
/// The INDICES of `rows` that survive filter + sort, in display order. Pure:
/// same rows + columns + state -> same order. `data_table` calls this; expose it
/// so you can test the pipeline on plain data.
template <typename Row>
inline std::vector<std::size_t> table_order(const std::vector<Row>& rows,
                                            const std::vector<TableColumn<Row>>& cols,
                                            const TableState& st){
    std::vector<std::size_t> idx;
    idx.reserve(rows.size());
    // filter: keep a row if ANY searchable column contains the query (case-insens).
    std::string q; for (char c : st.filter) q += (c>='A'&&c<='Z') ? char(c+32) : c;
    for (std::size_t i = 0; i < rows.size(); ++i){
        if (q.empty()){ idx.push_back(i); continue; }
        bool hit = false;
        for (auto& c : cols){
            if (!c.is_searchable()) continue;
            std::string t; for (char ch : c.text(rows[i])) t += (ch>='A'&&ch<='Z') ? char(ch+32) : ch;
            if (t.find(q) != std::string::npos){ hit = true; break; }
        }
        if (hit) idx.push_back(i);
    }
    // sort by the chosen column's comparator (stable, so ties keep input order).
    if (st.sort_col >= 0 && st.sort_col < (int)cols.size() && cols[st.sort_col].is_sortable()){
        const auto& less = cols[st.sort_col].less;
        std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b){
            return st.sort_desc ? less(rows[b], rows[a]) : less(rows[a], rows[b]);
        });
    }
    return idx;
}

/// Total pages for a filtered set of `total` rows under `st`.
inline int table_page_count(int total, const TableState& st){
    if (st.page_size <= 0) return 1;
    return (total + st.page_size - 1) / st.page_size;
}

// ── view ──────────────────────────────────────────────────────────────────────
namespace table_detail {
inline NodeRef sort_glyph(bool active, bool desc){
    if (!active) return text("\xe2\x86\x95") | fg_muted | text_size(11) | detail::raw_css("opacity",".4");
    return text(desc ? "\xe2\x86\x93" : "\xe2\x86\x91") | text_size(11);   // ↓ / ↑
}
}

/// `data_table(rows, columns, state, onSort, onPage)` — the interactive table.
/// `onSort(colIndex)` and `onPage(pageIndex)` are your Msgs (mappers int->Msg).
/// Sortable headers are clickable; if `state.page_size>0` a pager renders below.
template <typename Row, typename OnSort, typename OnPage>
inline NodeRef data_table(const std::vector<Row>& rows, const std::vector<TableColumn<Row>>& cols,
                          const TableState& st, OnSort onSort, OnPage onPage){
    auto order = table_order(rows, cols, st);
    int total = (int)order.size();
    int pages = table_page_count(total, st);
    int page = st.page < 0 ? 0 : (st.page >= pages ? pages - 1 : st.page);
    std::size_t start = st.page_size > 0 ? (std::size_t)page * st.page_size : 0;
    std::size_t end   = st.page_size > 0 ? std::min<std::size_t>(start + st.page_size, order.size()) : order.size();

    std::vector<NodeRef> cells;
    // header row: sortable columns are tappable and show the sort glyph.
    for (int ci = 0; ci < (int)cols.size(); ++ci){
        auto& c = cols[ci];
        auto head = row(text(c.header) | semibold | fg_muted | text_size(12.5f),
                        box() | grows,
                        c.is_sortable() ? table_detail::sort_glyph(st.sort_col==ci, st.sort_desc) : nothing())
            | items_center | gap(6) | pad_y(10)
            | line_b(0.12f);
        if (c.is_sortable())
            head = head | pointer | tap(onSort(ci)) | role("button")
                        | aria("sort", st.sort_col==ci ? (st.sort_desc ? "descending" : "ascending") : "none");
        cells.push_back(std::move(head));
    }
    // visible rows
    for (std::size_t k = start; k < end; ++k){
        const Row& r = rows[order[k]];
        for (auto& c : cols)
            cells.push_back(box(c.cell(r)) | pad_y(10) | fg_text | items_center | horizontal
                | line_b(0.06f));
    }
    auto grid = box(); grid->kids = std::move(cells); grid->style.flow = Flow::grid;
    grid->style.extra.emplace_back("grid-template-columns", "repeat(" + std::to_string(cols.size()) + ",minmax(0,auto))");
    grid->style.extra.emplace_back("column-gap", "24px");
    finalize(*grid);
    auto table = grid | w_full;

    if (st.page_size <= 0 || pages <= 1) return table;

    // pager: Prev / "n of m" / Next
    auto prev = text("\xe2\x80\xb9 Prev") | (page>0 ? fg_text : fg_muted)   // ‹ Prev
        | (page>0 ? (pointer | tap(onPage(page-1))) : Mod{})
        | text_size(13) | aria_label("Previous page");
    auto next = text("Next \xe2\x80\xba") | (page<pages-1 ? fg_text : fg_muted) // Next ›
        | (page<pages-1 ? (pointer | tap(onPage(page+1))) : Mod{})
        | text_size(13) | aria_label("Next page");
    auto info = text(std::to_string(page+1) + " of " + std::to_string(pages))
        | fg_muted | text_size(13);
    auto pager = row(prev, box() | grows, info, box() | grows, next)
        | items_center | w_full | pad_y(10);
    return col(table, pager) | w_full | gap(4);
}

} // namespace waya::ui
