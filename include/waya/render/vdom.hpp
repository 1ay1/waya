#pragma once
/// \file vdom.hpp
/// The virtual DOM — waya's analogue of maya's `prev_cells`.
///
/// maya keeps the previous frame's cell grid and diffs the next frame against
/// it, emitting only changed cells. waya keeps the previous render's node tree
/// (`VNode`) and diffs the next render against it, emitting only changed DOM
/// ops. Same doctrine, different substrate: the browser window is the terminal,
/// the DOM tree is the cell grid.
///
/// A `VNode` is a minimal, comparable snapshot of a rendered element: tag,
/// attributes (sorted for stable compare), text, and children. It is produced
/// by walking the DSL tree once (see render/vwalk.hpp) and is the ONLY state
/// the live runtime keeps between frames.

#include <string>
#include <utility>
#include <vector>

namespace waya::vdom {

/// One node of the previous/next render. Either an element or a text leaf.
struct VNode {
    bool is_text = false;

    // element
    std::string tag;                                   ///< "div", "button", …
    std::vector<std::pair<std::string, std::string>> attrs;  ///< sorted by name
    std::vector<VNode> kids;

    // text leaf
    std::string text;

    // Optional stable key (from `key(...)`) for keyed list reconciliation.
    std::string key;

    static VNode element(std::string t) { VNode v; v.tag = std::move(t); return v; }
    static VNode textnode(std::string s) { VNode v; v.is_text = true; v.text = std::move(s); return v; }

    bool operator==(const VNode&) const = default;
};

} // namespace waya::vdom
