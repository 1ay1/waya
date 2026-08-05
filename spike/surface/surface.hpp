#pragma once
// waya::surface — the Terminal Thesis spike.
//
// THE IDEA (the user's words): "the browser is just a terminal I render to.
// HTML/CSS/canvas/whatever is invisible plumbing waya picks. Not limiting —
// powerful enough to do anything — but simple as fuck."
//
// So the waya user never writes <div>, flex, ctx.fillRect, or onclick. They
// describe a SURFACE with a tiny, complete vocabulary of primitives. A BACKEND
// renders that surface however it renders it (DOM, canvas, later: PDF, a real
// terminal). Same surface, any backend — that's what makes the substrate
// invisible. maya does this for a cell grid; waya does it one level up.
//
// This header proves the load-bearing claim: ONE `view()` renders through TWO
// different backends UNCHANGED, and the diff produces minimal deltas.
//
// The vocabulary is deliberately tiny (like maya's ~8 style tags). Everything
// composes from these; nothing here mentions HTML.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace waya::surface {

// ── Visual attributes — semantic, not CSS. waya maps them per backend. ──────
struct Paint {
    uint32_t fg   = 0xffffffff;  // text/stroke colour (ARGB, ff = opaque)
    uint32_t bg   = 0x00000000;  // fill colour (00 alpha = transparent)
    float    size = 16;          // text size / line thickness (backend units)
    bool     bold = false;
    float    radius = 0;         // corner rounding (a box "border" is just this)
    float    pad  = 0;           // inner spacing
    float    gap  = 0;           // spacing between stacked children
    bool     operator==(const Paint&) const = default;
};

// ── Layout intent — how children flow. Not "flexbox". Just direction. ───────
enum class Flow : uint8_t { none, row, col, stack /*overlay*/ };

// ── The primitives. This is the WHOLE vocabulary. ───────────────────────────
// Box     — a rectangle that can hold children (the universal container).
// Text     — a run of characters.
// Image    — a bitmap by URL/handle.
// Path     — an arbitrary vector shape (the "do anything" escape: points +
//            stroke/fill). A chart, an icon, a custom widget = one Path.
// Every UI ever is these four composed. Simple; not limiting.
enum class Kind : uint8_t { box, text, image, path };

struct Node;
using NodeRef = std::shared_ptr<Node>;

struct Node {
    Kind  kind = Kind::box;
    Paint paint{};
    Flow  flow = Flow::none;

    std::string text;                 // Kind::text  — the string
    std::string src;                  // Kind::image — url/handle
    std::vector<std::pair<float,float>> points;  // Kind::path — polyline/polygon
    bool  closed = false;             // Kind::path  — fill vs stroke

    std::string key;                  // optional stable identity (like maya)
    std::vector<NodeRef> kids;

    // A message to deliver on click/tap — the ONLY interactivity concept the
    // user names. Not "onclick"; just "this responds with msg N".
    int on_tap = -1;
};

// ── The tiny builder API the USER writes. Reads like maya. No HTML anywhere. ─

inline NodeRef box(std::vector<NodeRef> kids = {}) {
    auto n = std::make_shared<Node>(); n->kind = Kind::box; n->kids = std::move(kids); return n;
}
inline NodeRef text(std::string s) {
    auto n = std::make_shared<Node>(); n->kind = Kind::text; n->text = std::move(s); return n;
}
inline NodeRef image(std::string src) {
    auto n = std::make_shared<Node>(); n->kind = Kind::image; n->src = std::move(src); return n;
}
inline NodeRef path(std::vector<std::pair<float,float>> pts, bool closed = false) {
    auto n = std::make_shared<Node>(); n->kind = Kind::path; n->points = std::move(pts); n->closed = closed; return n;
}

// Layout sugar — row/col/stack are just a box with a flow.
inline NodeRef row(std::vector<NodeRef> kids)  { auto n = box(std::move(kids)); n->flow = Flow::row; return n; }
inline NodeRef col(std::vector<NodeRef> kids)  { auto n = box(std::move(kids)); n->flow = Flow::col; return n; }
inline NodeRef stack(std::vector<NodeRef> kids){ auto n = box(std::move(kids)); n->flow = Flow::stack; return n; }

// Attribute setters return the node (pipe-like chaining, maya style).
inline NodeRef fg(NodeRef n, uint32_t c)   { n->paint.fg = c; return n; }
inline NodeRef bg(NodeRef n, uint32_t c)   { n->paint.bg = c; return n; }
inline NodeRef size(NodeRef n, float s)    { n->paint.size = s; return n; }
inline NodeRef bold(NodeRef n)             { n->paint.bold = true; return n; }
inline NodeRef round_(NodeRef n, float r)  { n->paint.radius = r; return n; }
inline NodeRef pad(NodeRef n, float p)     { n->paint.pad = p; return n; }
inline NodeRef gap(NodeRef n, float g)     { n->paint.gap = g; return n; }
inline NodeRef tap(NodeRef n, int msg)     { n->on_tap = msg; return n; }
inline NodeRef key(NodeRef n, std::string k){ n->key = std::move(k); return n; }

} // namespace waya::surface
