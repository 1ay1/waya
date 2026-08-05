#pragma once
/// \file node.hpp
/// The Surface — waya's substrate-free rendering model.
///
/// You describe WHAT to render with four primitives; waya owns HOW (a DOM
/// backend, a canvas backend, later others). Nothing here mentions HTML, CSS,
/// or the canvas API. See SURFACE.md.
///
///   col({
///       text("Dashboard") | fg(0x3b82f6) | size(28) | bold,
///       box({ text("Requests"), text("42") }) | pad(12) | bg(0x1e293b) | round_(12),
///       path(cpu_history) | fg(0x22d3ee),          // a chart — one primitive
///       text("+") | pad(8) | bg(0x334155) | tap(Inc),
///   }) | gap(16) | pad(24)
///
/// The vocabulary is deliberately tiny (like maya's handful of style tags).
/// `path` is the "do anything" escape: any 2-D shape is points + stroke/fill,
/// so a chart, an icon, or a custom widget is a single node.

#include "../core/hash.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace waya::surface {

/// Visual attributes — semantic, not CSS. Each backend maps them its own way.
struct Paint {
    std::uint32_t fg     = 0xffffff;  ///< text / stroke colour (RGB)
    std::uint32_t bg     = 0;         ///< fill colour
    bool          has_bg = false;
    float         size   = 16;        ///< text size / stroke width
    bool          bold   = false;
    float         radius = 0;         ///< corner rounding
    float         pad    = 0;         ///< inner spacing
    float         gap    = 0;         ///< spacing between children
    float         grow   = 0;         ///< flex grow weight (0 = natural size)
    bool operator==(const Paint&) const = default;
};

/// How a box arranges its children. Not "flexbox" — just direction.
enum class Flow : std::uint8_t { none, row, col, stack };

/// The four primitives. Everything composes from these.
enum class Kind : std::uint8_t { box, text, image, path };

struct Node;
using NodeRef = std::shared_ptr<Node>;

/// A point for `path`.
struct Pt { float x, y; bool operator==(const Pt&) const = default; };

struct Node {
    Kind  kind = Kind::box;
    Paint paint{};
    Flow  flow = Flow::none;

    std::string     text;    ///< Kind::text
    std::string     src;     ///< Kind::image
    std::vector<Pt> points;  ///< Kind::path
    bool            closed = false;

    std::string          key;      ///< optional stable identity (for keyed lists)
    int                  on_tap = -1;  ///< tap → this message index (-1 = none)
    std::vector<NodeRef> kids;

    /// Content hash of this whole subtree (the maya fast-diff trick). Filled by
    /// finalize(); the diff compares hashes to skip unchanged subtrees in O(1).
    std::uint64_t hash = 0;
};

// ── Content hashing (bottom-up, mirrors the DOM VNode) ──────────────────────

template <typename I>
    requires std::is_integral_v<I> || std::is_enum_v<I>
inline std::uint64_t mix(std::uint64_t h, I v) {
    auto u = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i) { h ^= (u >> (i*8)) & 0xFF; h *= 1099511628211ull; }
    return h;
}
inline std::uint64_t mix(std::uint64_t h, std::string_view s) {
    for (char c : s) { h ^= (std::uint8_t)c; h *= 1099511628211ull; }
    return h;
}
inline std::uint64_t mix(std::uint64_t h, float f) {
    std::uint32_t b; std::memcpy(&b, &f, 4); return mix(h, (std::uint64_t)b);
}

/// Compute the subtree hash of a node whose children are already hashed.
inline void finalize(Node& n) {
    std::uint64_t h = 1469598103934665603ull;
    h = mix(h, (std::uint64_t)n.kind);
    h = mix(h, (std::uint64_t)n.flow);
    h = mix(h, n.paint.fg); h = mix(h, n.paint.bg); h = mix(h, (std::uint64_t)n.paint.has_bg);
    h = mix(h, n.paint.size); h = mix(h, (std::uint64_t)n.paint.bold);
    h = mix(h, n.paint.radius); h = mix(h, n.paint.pad); h = mix(h, n.paint.gap); h = mix(h, n.paint.grow);
    h = mix(h, n.text); h = mix(h, n.src);
    for (auto& p : n.points) { h = mix(h, p.x); h = mix(h, p.y); }
    h = mix(h, (std::uint64_t)n.closed);
    h = mix(h, n.key);
    h = mix(h, (std::uint64_t)(std::int64_t)n.on_tap);
    for (auto& k : n.kids) h = mix(h, k->hash);
    n.hash = h;
}

// ── Builders — the user-facing API ──────────────────────────────────────────

inline NodeRef box(std::vector<NodeRef> kids = {}) {
    auto n = std::make_shared<Node>(); n->kind = Kind::box; n->kids = std::move(kids);
    finalize(*n); return n;
}
inline NodeRef text(std::string s) {
    auto n = std::make_shared<Node>(); n->kind = Kind::text; n->text = std::move(s);
    finalize(*n); return n;
}
inline NodeRef text(long long v) { return text(std::to_string(v)); }
inline NodeRef image(std::string src) {
    auto n = std::make_shared<Node>(); n->kind = Kind::image; n->src = std::move(src);
    finalize(*n); return n;
}
inline NodeRef path(std::vector<Pt> pts, bool closed = false) {
    auto n = std::make_shared<Node>(); n->kind = Kind::path; n->points = std::move(pts);
    n->closed = closed; finalize(*n); return n;
}

inline NodeRef row(std::vector<NodeRef> kids)   { auto n = box(std::move(kids)); n->flow = Flow::row;   finalize(*n); return n; }
inline NodeRef col(std::vector<NodeRef> kids)   { auto n = box(std::move(kids)); n->flow = Flow::col;   finalize(*n); return n; }
inline NodeRef stack(std::vector<NodeRef> kids) { auto n = box(std::move(kids)); n->flow = Flow::stack; finalize(*n); return n; }

// ── Chaining attributes via `|` — reads like maya ───────────────────────────
// Each returns the (mutated, re-hashed) node so they compose left-to-right.

struct Attr { void (*apply)(Node&, std::uint32_t, float); std::uint32_t u; float f; };

inline NodeRef operator|(NodeRef n, Attr a) { a.apply(*n, a.u, a.f); finalize(*n); return n; }

inline Attr fg(std::uint32_t c)   { return {[](Node& n,std::uint32_t u,float){ n.paint.fg=u; }, c, 0}; }
inline Attr bg(std::uint32_t c)   { return {[](Node& n,std::uint32_t u,float){ n.paint.bg=u; n.paint.has_bg=true; }, c, 0}; }
inline Attr size(float s)         { return {[](Node& n,std::uint32_t,float f){ n.paint.size=f; }, 0, s}; }
inline Attr round_(float r)       { return {[](Node& n,std::uint32_t,float f){ n.paint.radius=f; }, 0, r}; }
inline Attr pad(float p)          { return {[](Node& n,std::uint32_t,float f){ n.paint.pad=f; }, 0, p}; }
inline Attr gap(float g)          { return {[](Node& n,std::uint32_t,float f){ n.paint.gap=f; }, 0, g}; }
inline Attr grow(float g = 1)     { return {[](Node& n,std::uint32_t,float f){ n.paint.grow=f; }, 0, g}; }
inline Attr tap(int msg)          { return {[](Node& n,std::uint32_t u,float){ n.on_tap=(int)u; }, (std::uint32_t)msg, 0}; }

// `key("id")` sets a stable identity for keyed diffing — its own overload since
// it carries a string, not a number.
struct KeyTag { std::string k; };
inline KeyTag key(std::string k) { return {std::move(k)}; }
inline NodeRef operator|(NodeRef n, KeyTag t) { n->key = std::move(t.k); finalize(*n); return n; }

// `bold` is a bare tag (no argument).
struct BoldTag {};
inline constexpr BoldTag bold{};
inline NodeRef operator|(NodeRef n, BoldTag) { n->paint.bold = true; finalize(*n); return n; }

} // namespace waya::surface
