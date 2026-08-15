#pragma once
/// \file ui/data_grid.hpp
/// data_grid — an editable spreadsheet-style table: sort + filter + paginate
/// (inherited from table.hpp) PLUS click-to-edit cells with text / number /
/// select editors, keyboard commit (Enter) and cancel (Escape).
///
/// A `data_table` renders rows read-only. A data_grid adds a single piece of
/// UI state — WHICH cell is being edited — and, per column, an OPTIONAL editor
/// describing how that cell turns into a live control. Everything else (which
/// column is sorted, the filter, the page) is the very same `TableState`, and
/// the filter/sort pipeline is the very same pure `table_order`, so a grid is a
/// table you can type into.
///
///   struct Person { std::string id, name, role; int age; };
///   struct Model { std::vector<Person> people; TableState ts; GridEdit edit; };
///   struct Sort{int c;}; struct Page{int p;};
///   struct Begin{std::string row,col;}; struct Commit{std::string v;}; struct Cancel{};
///
///   auto cols = std::vector{
///     gcol<Person>("Name", [](auto& p){ return text(p.name); })
///         .sortable([](auto&a,auto&b){ return a.name<b.name; })
///         .searchable([](auto&p){ return p.name; })
///         .edits_text([](auto&p){ return p.name; }),
///     gcol<Person>("Role", [](auto& p){ return text(p.role); })
///         .edits_select({option("eng"),option("design")}, [](auto&p){ return p.role; }),
///     gcol<Person>("Age", [](auto& p){ return text(std::to_string(p.age)); })
///         .edits_number([](auto&p){ return std::to_string(p.age); }),
///   };
///
///   data_grid(m.people, cols, m.ts, m.edit,
///       /*rowId*/  [](const Person& p){ return p.id; },
///       /*onSort*/ [](int c){ return Sort{c}; },
///       /*onPage*/ [](int p){ return Page{p}; },
///       /*onEdit*/ [](std::string r, std::string c){ return Begin{r,c}; },
///       /*onSave*/ [](std::string v){ return Commit{v}; },
///       /*onCancel*/ []{ return Cancel{}; });
///
/// In update() you own the write: on Begin set `m.edit`, on Commit copy the
/// value into the matching row (parse ints yourself) and clear `m.edit`, on
/// Cancel just clear it. The grid never mutates your data — it only shows an
/// editor and reports the new string. Injection-safe: values ride through the
/// same escaped input/select controls as every other form field.

#include "table.hpp"

#include <functional>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── which cell is being edited ────────────────────────────────────────────────
/// The grid's own UI state: the (rowId, columnKey) of the cell under edit, and
/// the draft text the user has typed so far. Empty `row` means nothing is being
/// edited. One value in your model, next to the TableState.
struct GridEdit {
    std::string row;    // row id currently editing ("" = none)
    std::string col;    // column key (its header) currently editing
    std::string draft;  // live text of the editor
    bool operator==(const GridEdit&) const = default;

    [[nodiscard]] bool active() const { return !row.empty(); }
    [[nodiscard]] bool editing(const std::string& r, const std::string& c) const {
        return active() && row == r && col == c;
    }
    /// Enter edit mode on a cell with a starting value.
    void begin(std::string r, std::string c, std::string start){
        row = std::move(r); col = std::move(c); draft = std::move(start);
    }
    /// Live-update the draft as the user types.
    void type(std::string v){ draft = std::move(v); }
    /// Leave edit mode (on commit or cancel).
    void stop(){ row.clear(); col.clear(); draft.clear(); }
};

// ── editor kinds ──────────────────────────────────────────────────────────────
enum class GridEditor : std::uint8_t { none, text, number, select };

/// A grid column: a table column PLUS an optional cell editor. The read view
/// (`cell`), sorting (`sortable`) and search (`searchable`) are exactly the
/// table's; `edits_*` opts the column into inline editing and says how a cell
/// becomes a control and how to read its current value as a string.
template <typename Row>
struct GridColumn {
    TableColumn<Row> base;
    GridEditor editor = GridEditor::none;
    std::function<std::string(const Row&)> value;   // current cell value as text (for the editor)
    std::vector<Opt> options;                       // select editor choices

    GridColumn& sortable(std::function<bool(const Row&, const Row&)> cmp){ base.sortable(std::move(cmp)); return *this; }
    GridColumn& searchable(std::function<std::string(const Row&)> get){ base.searchable(std::move(get)); return *this; }

    /// This cell edits as a plain text field.
    GridColumn& edits_text(std::function<std::string(const Row&)> get){
        editor = GridEditor::text; value = std::move(get); return *this;
    }
    /// This cell edits as a numeric field (type="number").
    GridColumn& edits_number(std::function<std::string(const Row&)> get){
        editor = GridEditor::number; value = std::move(get); return *this;
    }
    /// This cell edits as a dropdown over `opts`.
    GridColumn& edits_select(std::vector<Opt> opts, std::function<std::string(const Row&)> get){
        editor = GridEditor::select; options = std::move(opts); value = std::move(get); return *this;
    }
    [[nodiscard]] bool is_editable() const { return editor != GridEditor::none && (bool)value; }
};

/// `gcol<Row>("Header", cellFn)` — start a grid column (chain `.sortable` /
/// `.searchable` / `.edits_text|number|select`).
template <typename Row>
inline GridColumn<Row> gcol(std::string header, std::function<NodeRef(const Row&)> cell){
    return GridColumn<Row>{ TableColumn<Row>{ std::move(header), std::move(cell), {}, {} }, GridEditor::none, {}, {} };
}

// ── view ──────────────────────────────────────────────────────────────────────
/// `data_grid(rows, cols, tableState, editState, rowId, onSort, onPage, onEdit,
///            onType, onDone, onCancel)` — the editable table.
///
///   onEdit(rowId, colKey) -> Msg   (enter edit mode; you set m.edit.begin(...))
///   onType(newValueText)  -> Msg   (a keystroke; store it: m.edit.type(v))
///   onDone()              -> Msg   (Enter / change / blur committed the draft)
///   onCancel()            -> Msg   (Escape abandoned the edit)
///
/// The split matters: `onType` streams every keystroke into `m.edit.draft`
/// (so the model always holds the live value), and `onDone` is the discrete
/// "apply that draft to the row" signal. A cell whose column `is_editable()`
/// shows a subtle affordance and, on click, swaps its text for a focused
/// control. Non-editable columns render read-only exactly like `data_table`.
template <typename Row, typename RowId, typename OnSort, typename OnPage,
          typename OnEdit, typename OnType, typename OnDone, typename OnCancel>
inline NodeRef data_grid(const std::vector<Row>& rows, const std::vector<GridColumn<Row>>& cols,
                         const TableState& st, const GridEdit& edit, RowId rowId,
                         OnSort onSort, OnPage onPage, OnEdit onEdit, OnType onType, OnDone onDone, OnCancel onCancel){
    // reuse the table pipeline: build plain TableColumns for order derivation.
    std::vector<TableColumn<Row>> tcols;
    tcols.reserve(cols.size());
    for (auto& c : cols) tcols.push_back(c.base);

    auto order = table_order(rows, tcols, st);
    int total = (int)order.size();
    int pages = table_page_count(total, st);
    int page = st.page < 0 ? 0 : (st.page >= pages ? pages - 1 : st.page);
    std::size_t start = st.page_size > 0 ? (std::size_t)page * st.page_size : 0;
    std::size_t end   = st.page_size > 0 ? std::min<std::size_t>(start + st.page_size, order.size()) : order.size();

    std::vector<NodeRef> cells;
    // header row (identical to data_table).
    for (int ci = 0; ci < (int)cols.size(); ++ci){
        auto& c = cols[ci].base;
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

    // body cells: editable columns swap to a control when this cell is active.
    for (std::size_t k = start; k < end; ++k){
        const Row& r = rows[order[k]];
        std::string rid = rowId(r);
        for (auto& gc : cols){
            const std::string& ckey = gc.base.header;
            NodeRef inner;
            if (gc.is_editable() && edit.editing(rid, ckey)){
                // active editor: focused control reporting keystrokes as the draft.
                std::string cur = edit.draft;
                if (gc.editor == GridEditor::select){
                    inner = select(gc.options, cur) | input_skin() | autofocus()
                          | on_change(onType)              // pick streams the value
                          | on_blur(onDone())              // and commits on close
                          | on_escape(onCancel())
                          | detail::raw_css("width","100%");
                } else {
                    auto ctrl = input(cur) | input_skin() | autofocus()
                              | on_input(onType)             // live draft to update()
                              | on_enter(onDone())           // Enter commits the current draft
                              | on_escape(onCancel())        // Escape abandons
                              | on_blur(onDone())            // click-away commits
                              | detail::raw_css("width","100%") | detail::raw_css("box-sizing","border-box");
                    if (gc.editor == GridEditor::number) ctrl = ctrl | type("number");
                    inner = ctrl;
                }
            } else if (gc.is_editable()){
                // read view with an affordance; click enters edit mode on this cell.
                inner = box(gc.base.cell(r)) | pointer | tap(onEdit(rid, ckey))
                      | detail::raw_css("cursor","text")
                      | detail::raw_css("border-radius","4px")
                      | hover_bg(0xffffff, 0.05f)
                      | title("Click to edit");
            } else {
                inner = gc.base.cell(r);   // plain read-only cell
            }
            cells.push_back(box(std::move(inner)) | pad_y(8) | pad_x(4) | fg_text | items_center | horizontal
                | line_b(0.06f));
        }
    }

    auto grid = box(); grid->kids = std::move(cells); grid->style.flow = Flow::grid;
    grid->style.extra.emplace_back("grid-template-columns", "repeat(" + std::to_string(cols.size()) + ",minmax(0,auto))");
    grid->style.extra.emplace_back("column-gap", "24px");
    finalize(*grid);
    auto table = grid | w_full;

    if (st.page_size <= 0 || pages <= 1) return table;

    auto prev = text("\xe2\x80\xb9 Prev") | (page>0 ? fg_text : fg_muted)
        | (page>0 ? (pointer | tap(onPage(page-1))) : Mod{})
        | text_size(13) | aria_label("Previous page");
    auto next = text("Next \xe2\x80\xba") | (page<pages-1 ? fg_text : fg_muted)
        | (page<pages-1 ? (pointer | tap(onPage(page+1))) : Mod{})
        | text_size(13) | aria_label("Next page");
    auto info = text(std::to_string(page+1) + " of " + std::to_string(pages))
        | fg_muted | text_size(13);
    auto pager = row(prev, box() | grows, info, box() | grows, next)
        | items_center | w_full | pad_y(10);
    return col(table, pager) | w_full | gap(4);
}

} // namespace waya::ui
