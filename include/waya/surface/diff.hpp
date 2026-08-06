#pragma once
/// \file diff.hpp
/// The surface delta engine. Keeps the previous surface, diffs the next against
/// it, and emits only the changed nodes — the payload that travels to the
/// browser. Uses the subtree hash for O(1) skip of unchanged branches.
///
/// The op set is small and substrate-neutral: the client (DOM or canvas) knows
/// how to apply each one to its own representation.
///
/// Children reconcile one of two ways:
///   • positional (default): lockstep, cheap, correct when the list is stable;
///   • keyed: when children carry `key(...)`, the diff matches by key and emits
///     `move` ops for reorders instead of re-rendering every shifted row — so a
///     list that only reorders touches O(moved) nodes, not O(n), and each row's
///     own DOM (and its input focus / scroll) survives the reorder.

#include "node.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace waya::surface {

enum class Op : std::uint8_t { set_text, set_paint, set_path, set_src, replace, remove, insert, move };

struct PatchOp {
    Op          op;
    std::string path;   ///< dotted child index from root, "0.2.1"
    std::string s;      ///< set_text: text; set_src: url; move: dest index (as string)
    NodeRef     node;   ///< replace/insert: the new subtree; set_paint/path: the node
    int         from = -1;  ///< move: current child index of the node being moved
    int         to   = -1;  ///< move: target child index
};
using Patch = std::vector<PatchOp>;

namespace detail {
inline std::string child(const std::string& p, std::size_t i) {
    return p.empty() ? std::to_string(i) : p + "." + std::to_string(i);
}

inline void diff_node(const Node& a, const Node& b, const std::string& path,
                      const NodeRef& bref, Patch& out);

/// True when EVERY child on both sides carries a non-empty key AND all keys are
/// UNIQUE within their list. Only then can we reconcile by identity; a mixed or
/// duplicate-keyed list falls back to positional diffing. Uniqueness is
/// essential: the keyed planner matches by `std::find`, which always returns the
/// FIRST occurrence — so a repeated key would silently diff the wrong row and
/// corrupt the client DOM. Falling back is always safe.
inline bool fully_keyed(const std::vector<NodeRef>& a, const std::vector<NodeRef>& b) {
    if (a.empty() && b.empty()) return false;
    std::unordered_set<std::string> seen;
    for (auto& c : a) {
        if (!c || c->key.empty()) return false;
        if (!seen.insert(c->key).second) return false;   // duplicate key in old
    }
    seen.clear();
    for (auto& c : b) {
        if (!c || c->key.empty()) return false;
        if (!seen.insert(c->key).second) return false;   // duplicate key in new
    }
    return true;
}

/// Positional child diff — lockstep by index. The historical default.
inline void diff_children_positional(const Node& a, const Node& b,
                                     const std::string& path, Patch& out) {
    const std::size_t na = a.kids.size(), nb = b.kids.size();
    const std::size_t common = na < nb ? na : nb;
    for (std::size_t i = 0; i < common; ++i)
        diff_node(*a.kids[i], *b.kids[i], child(path, i), b.kids[i], out);
    // Remove surplus tail from the back so earlier indices stay valid as we go.
    for (std::size_t i = nb; i < na; ++i)
        out.push_back({Op::remove, child(path, na - 1 - (i - nb)), {}, {}});
    for (std::size_t i = na; i < nb; ++i)
        out.push_back({Op::insert, path, {}, b.kids[i]});
}

/// Keyed child diff — match by `key`, emit move/insert/remove so a reordered
/// list preserves each row's DOM. The plan is computed against a running model
/// of the child order that mirrors exactly what the client applies, op by op,
/// so `from`/`to` indices are always valid at apply time.
inline void diff_children_keyed(const Node& a, const Node& b,
                                const std::string& path, Patch& out) {
    // Where does each old key currently live, and its node (for in-place diff)?
    std::unordered_map<std::string, std::size_t> old_pos;
    for (std::size_t i = 0; i < a.kids.size(); ++i) old_pos[a.kids[i]->key] = i;
    std::unordered_map<std::string, bool> want;
    for (auto& c : b.kids) want[c->key] = true;

    // `cur` is the client's child order as we apply ops. Start from old order.
    std::vector<std::string> cur;
    cur.reserve(a.kids.size());
    std::unordered_map<std::string, NodeRef> old_node;
    for (auto& c : a.kids) { cur.push_back(c->key); old_node[c->key] = c; }

    // 1) Remove old keys not present in the new list (back-to-front indices are
    //    computed live from `cur`, so each remove keeps the rest valid).
    for (std::size_t i = cur.size(); i-- > 0; ) {
        if (!want.count(cur[i])) {
            out.push_back({Op::remove, child(path, i), {}, {}});
            cur.erase(cur.begin() + i);
        }
    }

    // 2) Walk the target order. For each key: insert if new, else move it into
    //    place from wherever it is now, then diff it in place.
    for (std::size_t target = 0; target < b.kids.size(); ++target) {
        const NodeRef& bc = b.kids[target];
        const std::string& k = bc->key;
        auto it = std::find(cur.begin(), cur.end(), k);
        if (it == cur.end()) {
            // New key: insert its subtree at `target`.
            out.push_back({Op::insert, path, std::to_string(target), bc, -1, (int)target});
            cur.insert(cur.begin() + target, k);
        } else {
            std::size_t from = (std::size_t)(it - cur.begin());
            if (from != target) {
                out.push_back({Op::move, path, {}, {}, (int)from, (int)target});
                cur.erase(cur.begin() + from);
                cur.insert(cur.begin() + target, k);
            }
            // Diff the (now correctly placed) node against its old self.
            diff_node(*old_node[k], *bc, child(path, target), bc, out);
        }
    }
}

inline void diff_node(const Node& a, const Node& b, const std::string& path,
                      const NodeRef& bref, Patch& out) {
    if (a.hash == b.hash) return;                       // fast path: unchanged subtree

    if (a.kind != b.kind) { out.push_back({Op::replace, path, {}, bref}); return; }

    // Identity check: if both nodes carry a key and the keys DIFFER, they are
    // semantically different nodes that happen to share a position (e.g. two
    // route screens). Replace as one op instead of morphing one into the other
    // — cheaper on the wire AND correct (no cross-identity DOM reuse). Matches
    // React/Elm keyed reconciliation.
    if (!a.key.empty() && !b.key.empty() && a.key != b.key) {
        out.push_back({Op::replace, path, {}, bref}); return;
    }

    // paint/flow delta (applies to every kind)
    if (a.style != b.style || a.on_tap != b.on_tap || a.events != b.events
        || a.draggable != b.draggable || a.attrs != b.attrs)
        out.push_back({Op::set_paint, path, {}, bref});

    switch (a.kind) {
        case Kind::text:
            if (a.text != b.text) out.push_back({Op::set_text, path, b.text, bref});
            return;
        case Kind::image:
            if (a.src != b.src) out.push_back({Op::set_src, path, b.src, bref});
            return;
        case Kind::video: case Kind::audio:
            if (a.src != b.src) out.push_back({Op::set_paint, path, {}, bref});
            return;
        case Kind::markup:
            if (a.text != b.text) out.push_back({Op::set_paint, path, {}, bref});
            return;
        case Kind::path:
            if (a.points != b.points || a.closed != b.closed)
                out.push_back({Op::set_path, path, {}, bref});
            return;
        // Form controls: their rendered value/state lives in fields, not kids.
        // Any change re-renders the control node (set_paint → replaceWith on the
        // client). The client is authoritative for a focused field's own value,
        // so this only fires when the SERVER changes it.
        case Kind::input: case Kind::textarea: case Kind::checkbox:
        case Kind::radio: case Kind::button:
            if (a.text != b.text || a.checked != b.checked || a.disabled != b.disabled ||
                a.placeholder != b.placeholder || a.input_type != b.input_type ||
                a.name != b.name || a.on_input != b.on_input || a.on_change != b.on_change)
                out.push_back({Op::set_paint, path, {}, bref});
            return;
        case Kind::select:
            if (a.selected != b.selected || a.options != b.options || a.disabled != b.disabled ||
                a.name != b.name || a.on_change != b.on_change || a.on_input != b.on_input)
                out.push_back({Op::set_paint, path, {}, bref});
            return;
        case Kind::box: case Kind::form: {
            if (fully_keyed(a.kids, b.kids))
                diff_children_keyed(a, b, path, out);
            else
                diff_children_positional(a, b, path, out);
            return;
        }
    }
}
} // namespace detail

/// Produce the minimal delta turning `prev` into `next`.
inline Patch diff(const NodeRef& prev, const NodeRef& next) {
    Patch out; detail::diff_node(*prev, *next, "", next, out); return out;
}

} // namespace waya::surface
