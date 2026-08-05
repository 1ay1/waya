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

#include <cstdint>
#include <memory>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

namespace waya::vdom {

/// One node of the previous/next render. Either an element or a text leaf.
///
/// FAST PATH — the maya move: every node carries a 64-bit content `hash`
/// summarising its ENTIRE subtree (tag + attrs + text + all descendants),
/// computed bottom-up once during the walk. Comparing two nodes for "did this
/// whole subtree change?" is then a single `uint64_t` compare — O(1), no string
/// touches, no descent — exactly like maya packing a cell into one word and
/// comparing cells with one integer op. The diff uses it to skip unchanged
/// subtrees wholesale.
struct VNode {
    std::uint64_t hash = 0;   ///< content hash of this whole subtree (fast compare)
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

    /// Cheap subtree-equality: compare the packed hash first (the common case is
    /// "unchanged", answered in one compare). Full compare only on hash match to
    /// guard the astronomically-rare collision.
    [[nodiscard]] bool same(const VNode& o) const {
        return hash == o.hash;
    }

    bool operator==(const VNode& o) const {
        if (hash != o.hash) return false;
        return is_text == o.is_text && tag == o.tag && text == o.text
            && key == o.key && attrs == o.attrs && kids == o.kids;
    }
};

// ── Content-hash mixing (FNV-1a over the node's own fields + child hashes) ───

constexpr std::uint64_t hash_mix(std::uint64_t h, std::string_view s) {
    for (char c : s) { h ^= static_cast<std::uint8_t>(c); h *= 1099511628211ull; }
    return h;
}
constexpr std::uint64_t hash_mix(std::uint64_t h, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) { h ^= (v >> (i*8)) & 0xFF; h *= 1099511628211ull; }
    return h;
}

/// Compute a node's packed hash from its own fields + already-hashed children.
/// Call this once, bottom-up, as the walk finishes each node.
inline void finalize_hash(VNode& n) {
    std::uint64_t h = 1469598103934665603ull;
    h = hash_mix(h, n.is_text ? std::uint64_t{1} : std::uint64_t{0});
    if (n.is_text) {
        h = hash_mix(h, n.text);
    } else {
        h = hash_mix(h, n.tag);
        for (const auto& [k, v] : n.attrs) { h = hash_mix(h, k); h = hash_mix(h, v); }
        for (const auto& c : n.kids) h = hash_mix(h, c.hash);   // child hashes only
    }
    if (!n.key.empty()) h = hash_mix(h, n.key);
    n.hash = h;
}

} // namespace waya::vdom
