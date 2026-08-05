#pragma once
// waya::surface diff — the delta engine over the primitive tree.
//
// The surface is retained (kept between frames). Each frame, waya diffs the new
// surface against the old and emits only what changed. That minimal delta is
// what travels to the browser — the same principle as maya sending only changed
// cells, applied to the primitive tree. The user never thinks about it; they
// just describe the surface and it stays in sync.

#include "surface.hpp"

#include <string>
#include <vector>

namespace waya::surface {

enum class PatchKind : uint8_t { set_text, set_paint, set_path, replace, remove, insert };

struct PatchOp {
    PatchKind   op;
    std::string path;    // dotted child index, "0.2.1"
    std::string value;   // set_text: the text; set_paint: packed; replace/insert: (n/a in spike)
};
using Patch = std::vector<PatchOp>;

namespace detail {
inline std::string child(const std::string& p, std::size_t i) {
    return p.empty() ? std::to_string(i) : p + "." + std::to_string(i);
}
inline bool same_paint(const Paint& a, const Paint& b) { return a == b; }
inline bool same_points(const std::vector<std::pair<float,float>>& a,
                        const std::vector<std::pair<float,float>>& b) { return a == b; }

inline void diff_node(const Node& a, const Node& b, const std::string& path, Patch& out) {
    // Kind change → replace the subtree wholesale (the safety valve).
    if (a.kind != b.kind) { out.push_back({PatchKind::replace, path, {}}); return; }

    switch (a.kind) {
        case Kind::text:
            if (a.text != b.text) out.push_back({PatchKind::set_text, path, b.text});
            if (!same_paint(a.paint, b.paint)) out.push_back({PatchKind::set_paint, path, {}});
            return;
        case Kind::image:
            if (a.src != b.src) out.push_back({PatchKind::replace, path, b.src});
            return;
        case Kind::path:
            if (!same_points(a.points, b.points) || a.closed != b.closed)
                out.push_back({PatchKind::set_path, path, {}});
            if (!same_paint(a.paint, b.paint)) out.push_back({PatchKind::set_paint, path, {}});
            return;
        case Kind::box: {
            if (a.flow != b.flow || !same_paint(a.paint, b.paint))
                out.push_back({PatchKind::set_paint, path, {}});
            const std::size_t na = a.kids.size(), nb = b.kids.size();
            const std::size_t common = na < nb ? na : nb;
            for (std::size_t i = 0; i < common; ++i)
                diff_node(*a.kids[i], *b.kids[i], child(path, i), out);
            for (std::size_t i = nb; i < na; ++i)
                out.push_back({PatchKind::remove, child(path, na - 1 - (i - nb)), {}});
            for (std::size_t i = na; i < nb; ++i)
                out.push_back({PatchKind::insert, path, {}});
            return;
        }
    }
}
} // namespace detail

inline Patch diff(const Node& prev, const Node& next) {
    Patch out; detail::diff_node(prev, next, "", out); return out;
}

inline std::string to_json(const Patch& p) {
    static const char* names[] = {"set_text","set_paint","set_path","replace","remove","insert"};
    std::string o = "[";
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (i) o += ',';
        o += "[\""; o += names[(int)p[i].op]; o += "\",\""; o += p[i].path; o += "\"";
        if (p[i].op == PatchKind::set_text) { o += ",\""; o += p[i].value; o += "\""; }
        o += "]";
    }
    o += "]";
    return o;
}

} // namespace waya::surface
