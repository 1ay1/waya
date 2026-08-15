#pragma once
/// \file ui/virtual_list.hpp
/// virtual_list — render only the visible window of a huge list.
///
/// waya rebuilds the whole node tree every frame, and the diff makes that cheap
/// — but building 100,000 rows still costs 100,000 allocations per frame even
/// though the user sees ~20. `virtual_list` renders only the visible slice: the
/// rows in view plus a small overscan, with a top and bottom spacer sized to
/// keep the scrollbar honest. The browser scrolls natively; the server reports
/// the scroll offset (from an on_scroll Msg) and only builds what's on screen.
///
///   struct Model { int scroll_top = 0; std::vector<Row> rows; };
///
///   // update: the scroll container reports its scrollTop as the event value
///   [&](Scrolled s) { m.scroll_top = std::atoi(s.value.c_str()); return {m, Cmd::none()}; }
///
///   // view: a fixed-height scroll box; virtual_list windows the rows
///   scroll_window(360,  Scrolled{},          // 360px tall, reports scroll
///       virtual_list(m.scroll_top, 360, /*row_h=*/48, m.rows.size(),
///           [&](int i){ return row_view(m.rows[i]); }))
///
/// So a million-row table costs ~30 built nodes per frame, not a million. The
/// windowing math (`VirtualRange`) is pure and unit-testable; `virtual_list`
/// assembles the spacers + visible rows; `scroll_window` is the scroll container
/// that reports its offset. All three are just nodes.

#include "../surface/node.hpp"

#include <functional>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// The slice of a virtualized list to actually build, for a given scroll state.
/// Pure — compute it, test it, then build only `[first, last)`.
struct VirtualRange {
    int first = 0;        // first row index to render (inclusive)
    int last  = 0;        // one-past-last row index (exclusive)
    int top_pad = 0;      // px of spacer above `first` (rows 0..first)
    int bottom_pad = 0;   // px of spacer below `last`  (rows last..count)
    [[nodiscard]] int count() const { return last - first; }
};

/// Compute which rows are visible. `scroll_top`/`viewport_h`/`row_h` in px,
/// `total` = row count, `overscan` = extra rows above+below for smooth scroll.
inline VirtualRange virtual_range(int scroll_top, int viewport_h, int row_h,
                                  int total, int overscan = 4){
    if (row_h <= 0 || total <= 0) return { 0, 0, 0, 0 };
    if (scroll_top < 0) scroll_top = 0;
    int first = scroll_top / row_h - overscan;
    if (first < 0) first = 0;
    int visible = viewport_h / row_h + 2 * overscan + 1;
    int last = first + visible;
    if (last > total) last = total;
    if (first > total) first = total;
    return { first, last, first * row_h, (total - last) * row_h };
}

/// `virtual_list(scroll_top, viewport_h, row_h, total, build)` — a column of only
/// the visible rows, top/bottom-padded so the scrollbar reflects the full list.
/// `build(i)` renders row `i` (called only for on-screen indices). Every row is
/// keyed by index so the diff reuses DOM as you scroll. Rows MUST be `row_h`
/// tall (set it on them) for the math to line up.
template <typename Build>
inline NodeRef virtual_list(int scroll_top, int viewport_h, int row_h, int total,
                            Build build, int overscan = 4){
    auto r = virtual_range(scroll_top, viewport_h, row_h, total, overscan);
    std::vector<NodeRef> kids;
    kids.reserve(r.count() + 2);
    // top spacer: reserves the height of the rows above the window.
    if (r.top_pad > 0) kids.push_back(box() | h((float)r.top_pad) | w_full | key("vl-top"));
    for (int i = r.first; i < r.last; ++i)
        kids.push_back(build(i) | h((float)row_h) | key("vl-" + std::to_string(i)));
    if (r.bottom_pad > 0) kids.push_back(box() | h((float)r.bottom_pad) | w_full | key("vl-bot"));
    return col_(std::move(kids)) | w_full;
}

/// `scroll_window(height, onScroll, content)` — a fixed-height vertical scroll
/// container that reports its `scrollTop` (px) as `onScroll`'s value on every
/// scroll. Pair with `virtual_list` inside it. `onScroll` is a Msg; the runtime
/// delivers the offset as the update value (parse with std::atoi).
template <typename Msg>
inline NodeRef scroll_window(int height, Msg onScroll, NodeRef content){
    return box(std::move(content))
        | w_full | h((float)height)
        | detail::raw_css("overflow-y", "auto")
        | detail::raw_css("overflow-x", "hidden")
        // fire onScroll with the element's scrollTop as the value.
        | on_ev("scroll", [onScroll](std::string) { return onScroll; })
        | attr("data-scroll", "top");   // the client reads scrollTop into the value
}

} // namespace waya::ui
