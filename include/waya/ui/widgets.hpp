#pragma once
/// \file ui/widgets.hpp
/// Stateful components — the ones whose look depends on app state (open/closed,
/// a value, a selection). They take that state as a plain argument and emit the
/// right nodes + wired messages, so the STATE still lives in your Model and the
/// component stays a pure function. No hidden state, no lifecycle — the Elm way.
///
///   // in Model: bool menu_open; float volume; int tab; std::string sort_col;
///   menu(m.menu_open, ToggleMenu{}, trigger, {item("Profile", GoProfile{}), …})
///   toggle(m.dark, SetDark{})
///   progress(m.pct)
///   slider(m.volume, 0, 100, SetVolume{})
///   accordion(m.open_panel, {{"Billing", billing_body}, …}, OpenPanel{})

#include "components.hpp"
#include "icons.hpp"

#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// a percentage Len without needing the style literal
inline Len pct_(float v) { return {v, Unit::pct}; }

// ─────────────────────────────────────────────────────────────────────────────
// toggle / switch — a styled on/off control
// ─────────────────────────────────────────────────────────────────────────────

/// `toggle(on, msg)` — an iOS-style switch. Sends `msg` on change; you flip the
/// bool in `update`. Themed track + knob; the knob slides on state.
template <typename Msg>
NodeRef toggle(bool on, Msg msg) {
    auto knob = box() | w(18) | h(18) | round(999)
        | detail::raw_css("background", "#fff")
        | detail::raw_css("box-shadow", "0 1px 3px rgba(0,0,0,.4)")
        | detail::raw_css("transform", on ? "translateX(20px)" : "translateX(0)")
        | transition("transform .18s cubic-bezier(.2,.7,.2,1)");
    return box(knob)
        | w(44) | h(24) | round(999) | pad(3) | pointer
        | detail::raw_css("background", on ? "var(--wa-primary, #6366f1)" : "var(--wa-line, #334155)")
        | transition("background-color .18s ease")
        | role("switch") | aria("checked", on ? "true" : "false")
        | tap(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// progress — determinate bar
// ─────────────────────────────────────────────────────────────────────────────

/// `progress(pct)` — a determinate progress bar, 0..100. `tone` colours the fill.
inline NodeRef progress(float pct, Tone tone = Tone::primary) {
    float p = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    auto [fill, _] = impl::tone_colors(tone); (void)_;
    // integer percent so the CSS reads "42%" not "42.000000%"
    long pw = (long)(p + 0.5f);
    auto bar = box() | h(8) | round(999)
        | detail::raw_css("width", std::to_string(pw) + "%")
        | detail::raw_css("background", fill)
        | transition("width .3s ease");
    return box(bar) | w(pct_(100)) | h(8) | round(999)
        | detail::raw_css("background", "var(--wa-line, rgba(255,255,255,.12))")
        | detail::raw_css("overflow", "hidden")
        | role("progressbar") | aria("valuenow", std::to_string((int)p))
        | aria("valuemin", "0") | aria("valuemax", "100");
}

// ─────────────────────────────────────────────────────────────────────────────
// slider — a themed range input
// ─────────────────────────────────────────────────────────────────────────────

/// `slider(value, min, max, to_msg)` — a range control. `to_msg` is a mapper
/// `(std::string newValue) -> Msg`, so the dragged value actually reaches your
/// update (parse it there). Registers its track/thumb CSS once.
template <typename ToMsg>
NodeRef slider(float value, float min, float max, ToMsg to_msg, float step = 1) {
    assets().css(
        "input.wa-range{-webkit-appearance:none;appearance:none;height:6px;border-radius:999px;"
        "background:var(--wa-line,#334155);outline:none;width:100%}"
        "input.wa-range::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:18px;height:18px;"
        "border-radius:999px;background:var(--wa-primary,#6366f1);cursor:pointer;border:2px solid #fff;"
        "box-shadow:0 1px 4px rgba(0,0,0,.4)}"
        "input.wa-range::-moz-range-thumb{width:16px;height:16px;border-radius:999px;"
        "background:var(--wa-primary,#6366f1);cursor:pointer;border:2px solid #fff}");
    auto n = input(detail::numstr(value));
    n->input_type = "range";
    return n | attr("class", "wa-range")
        | attr("min", detail::numstr(min))
        | attr("max", detail::numstr(max))
        | attr("step", detail::numstr(step))
        | on_input(std::move(to_msg));   // mapper: the value string -> Msg
}

// ─────────────────────────────────────────────────────────────────────────────
// menu / dropdown — open state in the Model
// ─────────────────────────────────────────────────────────────────────────────

/// One menu row. `menu_item("Profile", GoProfile{})`.
template <typename Msg>
NodeRef menu_item(std::string label, Msg msg, std::string_view icon_name = "") {
    auto row_ = icon_name.empty()
        ? box(text(label))
        : box(icon(icon_name, 16) | fg_muted, text(label));
    return row_ | horizontal | gap(10) | center
        | pad_x(12) | pad_y(9) | round(8) | pointer | fg_text
        | text_size(14) | detail::raw_css("white-space", "nowrap")
        | on(Hover, detail::raw_css("background", "var(--wa-raised, rgba(255,255,255,.06))"))
        | tap(msg);
}

/// `menu(open, trigger, items…)` — a dropdown: the trigger, plus a frosted panel
/// of items shown when `open`. Wire the trigger's tap to a toggle message.
inline NodeRef menu(bool open, NodeRef trigger, std::vector<NodeRef> items,
                    std::string place = "bottom-right") {
    NodeRef panel = box();
    if (open) {
        panel = box(); panel->kids = std::move(items); panel->style.flow = Flow::col; finalize(*panel);
        panel = panel | frost(14) | round(12) | pad(6) | elevation(4)
              | detail::raw_css("min-width", "12rem") | pop_in(150);
    }
    return anchored(std::move(trigger), std::move(panel), place);
}

// ─────────────────────────────────────────────────────────────────────────────
// accordion — one open panel at a time
// ─────────────────────────────────────────────────────────────────────────────

/// `accordion(open_id, {{ "Title", body }…}, on_toggle)` — collapsible sections.
/// `open_id` is the index of the expanded panel (-1 for none); `on_toggle(i)`
/// maps a header click to a Msg. Chevron rotates; body expands.
template <typename ToMsg>
NodeRef accordion(int open_id, std::vector<std::pair<std::string, NodeRef>> panels,
                  ToMsg on_toggle) {
    std::vector<NodeRef> rows;
    for (std::size_t i = 0; i < panels.size(); ++i) {
        bool open = ((int)i == open_id);
        auto header = row(
            text(panels[i].first) | semibold | fg_text,
            push(),
            icon("chevron-down", 18) | fg_muted
                | detail::raw_css("transform", open ? "rotate(180deg)" : "rotate(0)")
                | transition("transform .2s ease"))
            | center | pad_y(14) | pad_x(4) | pointer | tap(on_toggle((int)i));
        auto body = open
            ? (box(panels[i].second) | pad_y(4) | pad_x(4) | detail::raw_css("padding-bottom", "14px") | fade_in(160))
            : box();
        rows.push_back(col(header, body) | line_b(0.10f));
    }
    auto acc = box(); acc->kids = std::move(rows); acc->style.flow = Flow::col; finalize(*acc);
    return acc;
}

// ─────────────────────────────────────────────────────────────────────────────
// data_table — a real aligned grid with a header row
// ─────────────────────────────────────────────────────────────────────────────

/// A table column: a header label and a cell-builder for a row value.
template <typename Row>
struct Column {
    std::string header;
    std::function<NodeRef(const Row&)> cell;
};

/// `data_table(rows, columns)` — a themed, aligned CSS-grid table. Every column
/// shares a track so cells line up down the page. Columns are typed: each maps a
/// Row to a cell node.
template <typename Row>
NodeRef data_table(const std::vector<Row>& rows, std::vector<Column<Row>> cols) {
    std::vector<NodeRef> cells;
    // header
    for (auto& c : cols)
        cells.push_back(text(c.header) | semibold | fg_muted
            | text_size(12.5f) | detail::raw_css("letter-spacing", ".03em")
            | pad_y(10) | line_b(0.12f));
    // rows
    for (auto& r : rows)
        for (auto& c : cols)
            cells.push_back(box(c.cell(r)) | pad_y(10) | fg_text
                | line_b(0.06f)
                | detail::raw_css("align-items", "center") | horizontal);
    auto g = box(); g->kids = std::move(cells); g->style.flow = Flow::grid;
    g->style.extra.emplace_back("grid-template-columns",
        "repeat(" + std::to_string(cols.size()) + ",minmax(0,auto))");
    g->style.extra.emplace_back("column-gap", "24px");
    finalize(*g);
    return g | w(pct_(100));
}

} // namespace waya::ui
