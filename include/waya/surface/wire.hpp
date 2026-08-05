#pragma once
/// \file wire.hpp
/// Serialise a surface node and a patch to the compact JSON the client applies.
/// This is backend-neutral: the client has both a DOM applier and a canvas
/// renderer that consume the SAME node JSON — so the substrate choice is the
/// client's (or the server's) to make, and the wire format doesn't care.

#include "node.hpp"
#include "diff.hpp"

#include <string>

namespace waya::surface {

namespace detail {
inline void jstr(std::string& o, std::string_view s) {
    o += '"';
    for (char c : s) switch (c) {
        case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
        case '\n': o += "\\n"; break; case '\r': o += "\\r"; break;
        default: if ((unsigned char)c < 0x20) { o += "\\u00"; static const char* H="0123456789abcdef";
                     o += H[(c>>4)&0xF]; o += H[c&0xF]; } else o += c;
    }
    o += '"';
}
inline void jnum(std::string& o, float f) {
    long long i = (long long)f;
    if ((float)i == f) { o += std::to_string(i); return; }
    std::string s = std::to_string(f);
    while (s.size() && s.back()=='0') s.pop_back();
    if (s.size() && s.back()=='.') s.pop_back();
    o += s;
}
} // namespace detail

/// A surface node → JSON. Compact keys: k=kind, t=text, s=src, p=points,
/// paint fields, f=flow, c=children, tap=message.
inline void node_json(std::string& o, const Node& n) {
    using namespace detail;
    o += "{\"k\":"; o += std::to_string((int)n.kind);
    if (n.flow != Flow::none) { o += ",\"f\":"; o += std::to_string((int)n.flow); }
    if (!n.text.empty()) { o += ",\"t\":"; jstr(o, n.text); }
    if (!n.src.empty())  { o += ",\"s\":"; jstr(o, n.src); }
    if (n.on_tap >= 0)   { o += ",\"tap\":"; o += std::to_string(n.on_tap); }
    // paint (only non-defaults, keeps the wire small)
    o += ",\"pt\":{\"fg\":"; o += std::to_string(n.paint.fg);
    if (n.paint.has_bg) { o += ",\"bg\":"; o += std::to_string(n.paint.bg); }
    if (n.paint.size != 16) { o += ",\"sz\":"; jnum(o, n.paint.size); }
    if (n.paint.bold)   o += ",\"b\":1";
    if (n.paint.radius) { o += ",\"r\":"; jnum(o, n.paint.radius); }
    if (n.paint.pad)    { o += ",\"pd\":"; jnum(o, n.paint.pad); }
    if (n.paint.gap)    { o += ",\"gp\":"; jnum(o, n.paint.gap); }
    if (n.paint.grow)   { o += ",\"gr\":"; jnum(o, n.paint.grow); }
    o += "}";
    if (n.kind == Kind::path) {
        o += ",\"p\":[";
        for (std::size_t i = 0; i < n.points.size(); ++i) {
            if (i) o += ',';
            o += '['; jnum(o, n.points[i].x); o += ','; jnum(o, n.points[i].y); o += ']';
        }
        o += "]"; if (n.closed) o += ",\"cl\":1";
    }
    if (!n.kids.empty()) {
        o += ",\"c\":[";
        for (std::size_t i = 0; i < n.kids.size(); ++i) { if (i) o += ','; node_json(o, *n.kids[i]); }
        o += "]";
    }
    o += "}";
}

inline std::string node_json(const Node& n) { std::string o; node_json(o, n); return o; }

/// A patch → JSON array of [op, path, payload]. payload depends on op:
///   set_text  → the string
///   set_paint/set_path/replace/insert → the node JSON
///   set_src   → the url
///   remove    → (nothing)
inline std::string patch_json(const Patch& p) {
    std::string o = "[";
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (i) o += ',';
        const auto& op = p[i];
        o += '['; o += std::to_string((int)op.op); o += ','; detail::jstr(o, op.path);
        switch (op.op) {
            case Op::set_text: o += ','; detail::jstr(o, op.s); break;
            case Op::set_src:  o += ','; detail::jstr(o, op.s); break;
            case Op::set_paint:
            case Op::set_path:
            case Op::replace:
            case Op::insert:   o += ','; if (op.node) node_json(o, *op.node); else o += "null"; break;
            case Op::remove:   break;
        }
        o += ']';
    }
    o += ']';
    return o;
}

} // namespace waya::surface
