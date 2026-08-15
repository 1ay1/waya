#pragma once
/// \file ui/split_pane.hpp
/// split_pane — two panes with a draggable divider, ratio as model state.
///
/// `split(a, b, ratio)` (layout.hpp) gives a FIXED split. A resizable split pane
/// \u2014 an editor + preview, a sidebar + content the user can drag \u2014 needs the
/// divider to report its new position so the ratio becomes model state. That's
/// what this adds: a grip you drag, delivering the new ratio (0..1) as a Msg.
///
///   struct Model { float ratio = 0.5f; };
///   struct Resized { std::string value; };   // value = the new ratio, e.g. "0.62"
///
///   // update:
///   [&](Resized r){ m.ratio = std::atof(r.value.c_str()); return {m, Cmd::none()}; }
///
///   // view:
///   split_pane(editor_pane, preview_pane, m.ratio, Resized{})
///
/// Dragging the divider fires `onResize` with the new fraction as its value
/// (parse with std::atof). The client computes the fraction from the pointer
/// position within the container, so no per-frame round-trip \u2014 it commits on
/// pointer-up (and reports live during the drag, rAF-throttled). Works for a
/// horizontal (side-by-side) or vertical (stacked) split.

#include "../surface/node.hpp"

#include <string>

namespace waya::ui {

using namespace waya::surface;

namespace split_detail {
inline std::string ratio_str(float r){
    if (r < 0.05f) r = 0.05f; if (r > 0.95f) r = 0.95f;   // keep both panes visible
    char b[16]; std::snprintf(b, sizeof b, "%.4f", r); return b;
}
/// The first pane's flex-basis as a clamped percentage string ("62%").
inline std::string pct(float r){
    if (r < 0.05f) r = 0.05f; if (r > 0.95f) r = 0.95f;
    char b[16]; std::snprintf(b, sizeof b, "%.2f%%", r * 100.f); return b;
}
}

/// `split_pane(a, b, ratio, onResize, vertical=false)` — two panes with a
/// draggable divider between them. `ratio` (0..1) is the first pane's fraction;
/// dragging fires `onResize` whose value is the new ratio. `vertical=true`
/// stacks them (a horizontal divider you drag up/down).
template <typename Msg>
inline NodeRef split_pane(NodeRef a, NodeRef b, float ratio, Msg onResize, bool vertical = false){
    std::string r = split_detail::ratio_str(ratio);

    // the divider: a thin grip the client turns into a drag handle.
    auto divider = box()
        | (vertical ? (w_full | h(6.f)) : (h_full | w(6.f)))
        | detail::raw_css("cursor", vertical ? "row-resize" : "col-resize")
        | detail::raw_css("background","var(--wa-line, rgba(255,255,255,.10))")
        | no_shrink
        | detail::raw_css("touch-action","none")
        | attr("data-wa-split", vertical ? "v" : "h")
        | on_ev("splitmove", [onResize](std::string){ return onResize; })   // client sets the value
        | role("separator")
        | aria(vertical ? "orientation" : "orientation", vertical ? "horizontal" : "vertical");

    auto pane_a = box(std::move(a))
        | (vertical ? w_full : h_full)
        | detail::raw_css("flex", "0 0 " + split_detail::pct(ratio))
        | detail::raw_css("overflow","auto") | min_w(0) | min_h(0);
    auto pane_b = box(std::move(b))
        | grows | detail::raw_css("overflow","auto") | min_w(0) | min_h(0);

    auto container = box(pane_a, divider, pane_b);
    container->style.flow = vertical ? Flow::col : Flow::row;
    finalize(*container);
    return container
        | w_full | h_full
        | attr("data-wa-split-box", "1");   // the client reads pointer pos relative to this
}

} // namespace waya::ui
