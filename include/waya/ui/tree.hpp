#pragma once
/// \file ui/tree.hpp
/// tree_view — a nested, expand/collapse tree, expansion as model state.
///
/// A file explorer, an outline, a nested comment thread: nodes that expand and
/// collapse. The tree DATA is yours (whatever shape); the only UI state is
/// "which nodes are open", and `TreeState` holds exactly that — a set of open
/// ids. `tree_view` walks your data and renders rows with indentation + a
/// disclosure caret wired to a toggle Msg.
///
///   struct FileNode { std::string id, name; std::vector<FileNode> children; };
///   struct Model { FileNode root; TreeState tree; };
///   struct Toggle { std::string id; };
///
///   // update:
///   [&](Toggle t){ m.tree.toggle(t.id); return {m, Cmd::none()}; }
///
///   // view: adapt your node to the tree's expectations, render
///   tree_view(m.root, m.tree,
///       /*id*/       [](const FileNode& n){ return n.id; },
///       /*label*/    [](const FileNode& n){ return row(icon("file"), text(n.name)); },
///       /*children*/ [](const FileNode& n){ return n.children; },
///       /*onToggle*/ [](std::string id){ return Toggle{id}; });
///
/// The caret only shows for nodes that HAVE children; clicking a row toggles it.
/// Rendering is a pure walk of (your data + the open set), so the whole tree is
/// a function of state like everything else.

#include "../surface/node.hpp"
#include "components.hpp"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// The expand/collapse state of a tree: the set of open node ids.
struct TreeState {
    std::set<std::string> open;

    bool operator==(const TreeState&) const = default;

    [[nodiscard]] bool is_open(const std::string& id) const { return open.count(id) > 0; }
    /// Toggle a node open/closed.
    void toggle(const std::string& id){ if (!open.erase(id)) open.insert(id); }
    void expand(const std::string& id){ open.insert(id); }
    void collapse(const std::string& id){ open.erase(id); }
    void collapse_all(){ open.clear(); }
};

namespace tree_detail {
inline NodeRef caret(bool open){
    // ▸ collapsed / ▾ expanded — rotated via a class-free inline glyph so it
    // diffs as text, not a paint.
    return text(open ? "\xe2\x96\xbe" : "\xe2\x96\xb8")
        | fg_muted | detail::raw_css("font-size","11px")
        | detail::raw_css("width","14px") | detail::raw_css("text-align","center");
}
/// A caret for a branch, or a blank spacer for a leaf (so labels line up).
inline NodeRef caret_or_space(bool has_kids, bool open){
    if (has_kids) return caret(open);
    return box() | detail::raw_css("width","14px");
}
}

/// `tree_view(root, state, id, label, children, onToggle)` — render a tree.
///   id(node)       -> a stable string id (for the open-set + keying)
///   label(node)    -> the row's content (icon + text, whatever)
///   children(node) -> that node's child nodes (empty = a leaf)
///   onToggle(id)   -> the Msg fired when a row with children is clicked
/// `indent` is the px per depth level.
template <typename Node_, typename IdFn, typename LabelFn, typename ChildrenFn, typename OnToggle>
inline NodeRef tree_view(const Node_& root, const TreeState& st,
                         IdFn id, LabelFn label, ChildrenFn children, OnToggle onToggle,
                         int indent = 18){
    std::vector<NodeRef> rows;
    // depth-first walk, emitting a row per visible node.
    std::function<void(const Node_&, int)> walk = [&](const Node_& node, int depth){
        std::string nid = id(node);
        auto kids = children(node);
        bool has_kids = !kids.empty();
        bool open = has_kids && st.is_open(nid);

        auto rowc = row(tree_detail::caret_or_space(has_kids, open),
                        label(node))
            | items_center | gap(6)
            | pad_y(4) | detail::raw_css("padding-left", std::to_string(depth * indent) + "px")
            | detail::raw_css("border-radius","6px")
            | (has_kids ? pointer : Mod{})
            | (has_kids ? tap(onToggle(nid)) : Mod{})
            | (has_kids ? role("treeitem") : Mod{})
            | aria("expanded", has_kids ? (open ? "true" : "false") : "")
            | key("tree-" + nid);
        rows.push_back(rowc);
        if (open)
            for (auto& child : kids) walk(child, depth + 1);
    };
    walk(root, 0);
    return col_(std::move(rows)) | w_full | role("tree");
}

} // namespace waya::ui
