#pragma once
/// \file diff.hpp
/// Tree diff — the maya cell-diff, transposed onto the DOM.
///
/// `diff(prev, next)` walks both trees in lockstep and emits the minimal set of
/// DOM operations to turn `prev` into `next`, each addressed by a node PATH
/// (child indices from the root, e.g. "0.3.1"). This is the web analogue of
/// maya emitting only changed cells: only changed nodes/attrs/text go on the
/// wire. A one-character text change is one `set_text` op of a few dozen bytes,
/// not a re-render.
///
/// Op set (deliberately small, like maya's cell writes):
///   set_text    path, text            — a text leaf changed
///   set_attr    path, name, value     — an attribute was added/changed
///   remove_attr path, name            — an attribute was removed
///   replace     path, html            — subtree shape changed; swap it wholesale
///   remove      path                  — a trailing child went away
///   insert      path, html            — a new trailing child appeared
///
/// `replace` is the safety valve (maya's full-row redraw): when two nodes differ
/// in tag or a child list changes shape in a way finer ops can't express, we
/// replace the subtree. Correctness first; the finer ops handle the common case
/// (text/attr churn) that dominates real UIs.

#include "vdom.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace waya::vdom {

enum class Op { set_text, set_attr, remove_attr, replace, remove, insert };

struct PatchOp {
    Op          op;
    std::string path;   ///< "0.3.1" — child indices from root
    std::string a;      ///< set_text: text; set_attr: name; remove_attr: name
    std::string b;      ///< set_attr: value
    VNode       node;   ///< replace/insert: the new subtree (HTML only at wire)
};

using Patch = std::vector<PatchOp>;

// Rendering a VNode subtree back to HTML — used by `replace`/`insert` ops.
inline void vnode_to_html(std::string& out, const VNode& n) {
    if (n.is_text) {
        // A raw/fragment leaf carries pre-rendered HTML; emit it verbatim.
        if (n.tag == "\x01raw") { out += n.text; return; }
        for (char c : n.text) switch (c) {
            case '&': out += "&amp;"; break; case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break; default: out += c;
        }
        return;
    }
    out += '<'; out += n.tag;
    for (const auto& [k, v] : n.attrs) {
        out += ' '; out += k; out += "=\"";
        for (char c : v) switch (c) {
            case '&': out += "&amp;"; break; case '"': out += "&quot;"; break;
            case '<': out += "&lt;"; break; default: out += c;
        }
        out += '"';
    }
    out += '>';
    // void elements have no closing tag
    static constexpr std::string_view voids[] =
        {"br","img","hr","input","meta","link","col","source","wbr"};
    bool is_void = false;
    for (auto v : voids) if (n.tag == v) { is_void = true; break; }
    if (!is_void) {
        for (const auto& k : n.kids) vnode_to_html(out, k);
        out += "</"; out += n.tag; out += '>';
    }
}

inline std::string vnode_to_html(const VNode& n) {
    std::string s; vnode_to_html(s, n); return s;
}

namespace detail {

inline std::string child_path(const std::string& parent, std::size_t i) {
    return parent.empty() ? std::to_string(i) : parent + "." + std::to_string(i);
}

/// Diff attribute lists (both sorted by name): emit set/remove for the delta.
inline void diff_attrs(const VNode& a, const VNode& b, const std::string& path, Patch& out) {
    std::size_t i = 0, j = 0;
    while (i < a.attrs.size() || j < b.attrs.size()) {
        if (j >= b.attrs.size()) {                       // trailing removals
            out.push_back({Op::remove_attr, path, a.attrs[i].first, {}}); ++i;
        } else if (i >= a.attrs.size()) {                // trailing additions
            out.push_back({Op::set_attr, path, b.attrs[j].first, b.attrs[j].second}); ++j;
        } else if (a.attrs[i].first == b.attrs[j].first) {
            if (a.attrs[i].second != b.attrs[j].second)  // value changed
                out.push_back({Op::set_attr, path, b.attrs[j].first, b.attrs[j].second});
            ++i; ++j;
        } else if (a.attrs[i].first < b.attrs[j].first) {
            out.push_back({Op::remove_attr, path, a.attrs[i].first, {}}); ++i;
        } else {
            out.push_back({Op::set_attr, path, b.attrs[j].first, b.attrs[j].second}); ++j;
        }
    }
}

inline void diff_node(const VNode& a, const VNode& b, const std::string& path, Patch& out) {
    // FAST PATH (maya's packed-cell compare): if the subtree hashes match, the
    // whole subtree is unchanged — one uint64_t compare, no descent, no string
    // touches. This is what makes diffing a 1000-row table where one cell
    // changed cost ~one path down, not a full tree walk.
    if (a.hash == b.hash) return;

    // Different KIND or tag → the shape changed here: replace the subtree.
    if (a.is_text != b.is_text || (!a.is_text && a.tag != b.tag)) {
        out.push_back({Op::replace, path, {}, {}, b});
        return;
    }
    if (b.is_text) {                                     // both text
        if (a.text != b.text) out.push_back({Op::set_text, path, b.text, {}, {}});
        return;
    }
    // Both elements, same tag: diff attributes, then children.
    diff_attrs(a, b, path, out);

    const std::size_t na = a.kids.size(), nb = b.kids.size();
    const std::size_t common = na < nb ? na : nb;
    for (std::size_t i = 0; i < common; ++i)
        diff_node(a.kids[i], b.kids[i], child_path(path, i), out);
    // Trailing children removed (remove from the end so indices stay valid).
    for (std::size_t i = nb; i < na; ++i)
        out.push_back({Op::remove, child_path(path, na - 1 - (i - nb)), {}, {}, {}});
    // Trailing children added.
    for (std::size_t i = na; i < nb; ++i)
        out.push_back({Op::insert, path, {}, {}, b.kids[i]});
}

} // namespace detail

/// Produce the minimal patch turning `prev` into `next`. Empty when identical.
[[nodiscard]] inline Patch diff(const VNode& prev, const VNode& next) {
    Patch out;
    detail::diff_node(prev, next, "", out);
    return out;
}

// ── Wire format ─────────────────────────────────────────────────────────────
// A patch serialises to a compact JSON array the client applies. Each op is
// [opcode, path, a, b?]. This is the whole payload the browser receives per
// update — the DOM analogue of maya's minimal cell writes.

namespace detail {
inline void json_str(std::string& o, std::string_view s) {
    o += '"';
    for (char c : s) switch (c) {
        case '"':  o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\n': o += "\\n";  break;
        case '\r': o += "\\r";  break;
        case '\t': o += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) { o += "\\u00";
                static const char* H="0123456789abcdef";
                o += H[(c>>4)&0xF]; o += H[c&0xF];
            } else o += c;
    }
    o += '"';
}
} // namespace detail

[[nodiscard]] inline std::string to_json(const Patch& p) {
    std::string o = "[";
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (i) o += ',';
        const auto& op = p[i];
        o += '[';
        o += std::to_string(static_cast<int>(op.op)); o += ',';
        detail::json_str(o, op.path);
        // set_text/remove_attr: `a`; set_attr: name `a` + value `b`;
        // replace/insert: the subtree HTML (rendered here, at the wire only).
        if (op.op == Op::set_text || op.op == Op::remove_attr) {
            o += ','; detail::json_str(o, op.a);
        } else if (op.op == Op::set_attr) {
            o += ','; detail::json_str(o, op.a);
            o += ','; detail::json_str(o, op.b);
        } else if (op.op == Op::replace || op.op == Op::insert) {
            o += ','; detail::json_str(o, vnode_to_html(op.node));
        }
        o += ']';
    }
    o += ']';
    return o;
}

// ── apply — THE canonical patch applier ────────────────────────────────────
//
// ONE implementation the whole framework trusts: the runtime applies its own
// patch to `prev` (keeping server + client in lockstep by construction), and
// the soundness test asserts `apply(diff(a,b), a) == b`. The browser's JS
// applier is a line-for-line mirror of THIS — same op semantics, same paths.
// If this and the client ever disagree, one test catches it; there is a single
// source of truth, not three.

namespace detail {
inline VNode* node_at(VNode& root, std::string_view path) {
    if (path.empty()) return &root;
    VNode* cur = &root;
    std::size_t i = 0;
    while (i <= path.size()) {
        std::size_t dot = path.find('.', i);
        std::size_t end = dot == std::string_view::npos ? path.size() : dot;
        std::size_t idx = 0;
        for (std::size_t k = i; k < end; ++k) idx = idx * 10 + (path[k] - '0');
        if (idx >= cur->kids.size()) return nullptr;
        cur = &cur->kids[idx];
        if (dot == std::string_view::npos) break;
        i = dot + 1;
    }
    return cur;
}
inline VNode* parent_and_index(VNode& root, std::string_view path, std::size_t& idx) {
    auto dot = path.rfind('.');
    std::string_view parent = dot == std::string_view::npos ? std::string_view{} : path.substr(0, dot);
    std::string_view last   = dot == std::string_view::npos ? path : path.substr(dot + 1);
    idx = 0; for (char c : last) idx = idx * 10 + (c - '0');
    return node_at(root, parent);
}
} // namespace detail

/// Apply a patch to a tree in place. The server uses this to keep `prev` exactly
/// what the client's DOM now is — so the next diff is correct by construction.
inline void apply(VNode& root, const Patch& p) {
    for (const auto& op : p) {
        switch (op.op) {
            case Op::set_text:
                if (auto* n = detail::node_at(root, op.path)) { n->text = op.a; finalize_hash(*n); }
                break;
            case Op::set_attr:
                if (auto* n = detail::node_at(root, op.path)) {
                    bool found = false;
                    for (auto& [k, v] : n->attrs) if (k == op.a) { v = op.b; found = true; }
                    if (!found) n->attrs.emplace_back(op.a, op.b);
                    std::sort(n->attrs.begin(), n->attrs.end(),
                              [](auto& x, auto& y){ return x.first < y.first; });
                    finalize_hash(*n);
                }
                break;
            case Op::remove_attr:
                if (auto* n = detail::node_at(root, op.path)) {
                    std::erase_if(n->attrs, [&](auto& kv){ return kv.first == op.a; });
                    finalize_hash(*n);
                }
                break;
            case Op::replace:
                if (auto* n = detail::node_at(root, op.path)) *n = op.node;
                break;
            case Op::remove: {
                std::size_t idx;
                if (auto* par = detail::parent_and_index(root, op.path, idx))
                    if (idx < par->kids.size()) par->kids.erase(par->kids.begin() + idx);
                break;
            }
            case Op::insert:
                if (auto* par = detail::node_at(root, op.path)) par->kids.push_back(op.node);
                break;
        }
    }
    // Re-hash ancestors touched by structural ops so the tree stays consistent
    // for the NEXT diff. (set_text/attr already re-hash their node above; the
    // parent hashes are recomputed lazily by the next full to_vnode, which is
    // what `prev` is compared against.)
}

} // namespace waya::vdom
