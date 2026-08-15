#pragma once
/// \file ui/file_tree.hpp
/// file_tree — a batteries-included file/folder explorer over `tree_view`.
///
/// `tree_view` is the generic recursion engine (any node type, any label). A
/// file explorer is the concrete, everyday specialisation of it: a `FileNode`
/// model (name + children), an icon picked from the file extension, an open
/// folder set, a highlighted selection, and click-to-open / click-to-select
/// affordances. This is what you reach for to show a repo, an asset library, a
/// project sidebar — without hand-wiring the tree each time.
///
///   FileNode root = folder("project", {
///       folder("src", { file("main.cpp"), file("util.hpp") }),
///       file("README.md"), file("logo.png"),
///   });
///   struct Model { FileNode tree; TreeState open; std::string sel; };
///   struct Toggle { std::string id; }; struct Select { std::string path; };
///
///   file_tree(m.tree, m.open, m.sel,
///       [](std::string id){ return Toggle{id}; },     // folder clicked
///       [](std::string path){ return Select{path}; }) // file clicked
///
/// In update: on Toggle flip `m.open.toggle(t.id)`, on Select store `m.sel`.
/// Ids/paths are the slash-joined path from the root, so they're stable and
/// unique. The tree is pure data (== + testable); nothing here mutates it.

#include "tree.hpp"
#include "icons.hpp"

#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── the model ─────────────────────────────────────────────────────────────────
/// A node in a file tree: a `name`, whether it's a directory, and its children.
/// A file has `dir=false` and no children; a folder has `dir=true` and any
/// number of children (possibly zero — an empty folder).
struct FileNode {
    std::string name;
    bool dir = false;
    std::vector<FileNode> children;
    bool operator==(const FileNode&) const = default;
};

/// `file("main.cpp")` — a leaf file node.
inline FileNode file(std::string name){ return { std::move(name), false, {} }; }
/// `folder("src", { file(...), ... })` — a directory node.
inline FileNode folder(std::string name, std::vector<FileNode> children = {}){
    return { std::move(name), true, std::move(children) };
}

namespace file_detail {
/// Lower-cased extension (without the dot), or "" if none.
inline std::string ext_of(const std::string& name){
    auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) return "";
    std::string e;
    for (std::size_t i = dot + 1; i < name.size(); ++i){
        char c = name[i];
        e += (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
    }
    return e;
}

/// Pick a file icon + tint from the extension. Code files get file-code, images
/// get file-image, everything else the plain file. Colours are a gentle,
/// language-ish palette so a directory listing reads at a glance.
struct IconSpec { const char* name; std::uint32_t tint; };
inline IconSpec icon_for(const std::string& fname){
    std::string e = ext_of(fname);
    auto is = [&](std::initializer_list<const char*> xs){
        for (auto x : xs) if (e == x) return true; return false; };
    if (is({"cpp","hpp","h","cc","c","cxx","rs","go","py","js","ts","tsx","jsx",
            "java","rb","php","swift","kt","lua","sh","sql"}))
        return { "file-code", 0x60a5fa };                       // blue
    if (is({"png","jpg","jpeg","gif","svg","webp","ico","bmp","avif"}))
        return { "file-image", 0xa78bfa };                      // violet
    if (is({"md","txt","rst","doc","docx","pdf","rtf"}))
        return { "file", 0x94a3b8 };                            // slate
    if (is({"json","yaml","yml","toml","xml","ini","cfg","lock","env"}))
        return { "file-code", 0xfbbf24 };                       // amber (config)
    if (is({"css","scss","sass","less"}))
        return { "file-code", 0xf472b6 };                       // pink
    if (is({"zip","tar","gz","7z","rar","bz2"}))
        return { "file", 0xf59e0b };                            // orange (archive)
    return { "file", 0x94a3b8 };
}
} // namespace file_detail

// ── the view ──────────────────────────────────────────────────────────────────
/// `file_tree(root, open, selected, onToggle, onSelect, indent)` — a file
/// explorer. Folders expand/collapse (onToggle carries the folder's path);
/// files select (onSelect carries the file's path). The selected row is
/// highlighted. Icons are chosen from each file's extension; open folders show
/// an open-folder glyph. Paths are the slash-joined path from the root.
template <typename OnToggle, typename OnSelect>
inline NodeRef file_tree(const FileNode& root, const TreeState& st, const std::string& selected,
                         OnToggle onToggle, OnSelect onSelect, int indent = 16){
    std::vector<NodeRef> rows;

    std::function<void(const FileNode&, const std::string&, int)> walk =
        [&](const FileNode& node, const std::string& parent, int depth){
            std::string path = parent.empty() ? node.name : parent + "/" + node.name;
            bool has_kids = node.dir;
            bool open = has_kids && st.is_open(path);
            bool sel = (path == selected);

            NodeRef glyph;
            std::uint32_t tint;
            if (node.dir){
                glyph = icon(open ? "folder-open" : "folder", 16);
                tint = 0x60a5fa;                     // folders in blue
            } else {
                auto spec = file_detail::icon_for(node.name);
                glyph = icon(spec.name, 16);
                tint = spec.tint;
            }

            auto rowc = row(
                    tree_detail::caret_or_space(has_kids, open),
                    box(glyph) | fg(tint) | detail::raw_css("line-height","0"),
                    text(node.name) | fg_text | detail::raw_css("font-size","13.5px")
                        | detail::raw_css("white-space","nowrap")
                        | detail::raw_css("overflow","hidden")
                        | detail::raw_css("text-overflow","ellipsis"))
                | items_center | gap(7) | w_full
                | pad_y(4) | pad_x(6)
                | detail::raw_css("padding-left", std::to_string(6 + depth * indent) + "px")
                | round(6) | pointer
                | (node.dir ? tap(onToggle(path)) : tap(onSelect(path)))
                | (node.dir ? role("treeitem") : role("treeitem"))
                | aria("expanded", has_kids ? (open ? "true" : "false") : "")
                | aria("selected", sel ? "true" : "false")
                | key("ft-" + path);

            if (sel) rowc = rowc
                | detail::raw_css("background","var(--wa-primary, rgba(99,102,241,.22))")
                | detail::raw_css("color","#fff");
            else rowc = rowc | hover_bg(0xffffff, 0.05f);

            rows.push_back(std::move(rowc));
            if (open)
                for (auto& child : node.children) walk(child, path, depth + 1);
        };
    walk(root, "", 0);
    return col_(std::move(rows)) | w_full | role("tree") | aria_label("File explorer");
}

// ── convenience: total counts (for a header like "12 files, 3 folders") ───────
/// `file_stats(root)` — recursively count {files, folders} under a node
/// (excluding the root itself). Pure; handy for a footer/header.
struct FileStats { int files = 0; int folders = 0; bool operator==(const FileStats&) const = default; };
inline FileStats file_stats(const FileNode& root){
    FileStats s;
    std::function<void(const FileNode&)> go = [&](const FileNode& n){
        for (auto& c : n.children){
            if (c.dir) ++s.folders; else ++s.files;
            go(c);
        }
    };
    go(root);
    return s;
}

} // namespace waya::ui
