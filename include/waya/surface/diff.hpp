#pragma once
/// \file diff.hpp
/// The surface delta engine. Keeps the previous surface, diffs the next against
/// it, and emits only the changed nodes — the payload that travels to the
/// browser. Uses the subtree hash for O(1) skip of unchanged branches.
///
/// The op set is small and substrate-neutral: the client (DOM or canvas) knows
/// how to apply each one to its own representation.

#include "node.hpp"

#include <string>
#include <vector>

namespace waya::surface {

enum class Op : std::uint8_t { set_text, set_paint, set_path, set_src, replace, remove, insert };

struct PatchOp {
    Op          op;
    std::string path;   ///< dotted child index from root, "0.2.1"
    std::string s;      ///< set_text: text; set_src: url
    NodeRef     node;   ///< replace/insert: the new subtree; set_paint/path: the node
};
using Patch = std::vector<PatchOp>;

namespace detail {
inline std::string child(const std::string& p, std::size_t i) {
    return p.empty() ? std::to_string(i) : p + "." + std::to_string(i);
}

inline void diff_node(const Node& a, const Node& b, const std::string& path,
                      const NodeRef& bref, Patch& out) {
    if (a.hash == b.hash) return;                       // fast path: unchanged subtree

    if (a.kind != b.kind) { out.push_back({Op::replace, path, {}, bref}); return; }

    // paint/flow delta (applies to every kind)
    if (a.style != b.style || a.on_tap != b.on_tap)
        out.push_back({Op::set_paint, path, {}, bref});

    switch (a.kind) {
        case Kind::text:
            if (a.text != b.text) out.push_back({Op::set_text, path, b.text, bref});
            return;
        case Kind::image:
            if (a.src != b.src) out.push_back({Op::set_src, path, b.src, bref});
            return;
        case Kind::path:
            if (a.points != b.points || a.closed != b.closed)
                out.push_back({Op::set_path, path, {}, bref});
            return;
        case Kind::box: {
            const std::size_t na = a.kids.size(), nb = b.kids.size();
            const std::size_t common = na < nb ? na : nb;
            for (std::size_t i = 0; i < common; ++i)
                diff_node(*a.kids[i], *b.kids[i], child(path, i), b.kids[i], out);
            for (std::size_t i = nb; i < na; ++i)
                out.push_back({Op::remove, child(path, na - 1 - (i - nb)), {}, {}});
            for (std::size_t i = na; i < nb; ++i)
                out.push_back({Op::insert, path, {}, b.kids[i]});
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
