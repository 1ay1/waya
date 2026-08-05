#pragma once
/// \file binary.hpp
/// The binary frame protocol — the framework's private "ANSI". Same ONE frame
/// shape as the JSON wire ({css, ops}), but packed: varint lengths, a single
/// op-code byte, paths as varint index sequences (not dotted strings), and
/// UTF-8 payloads with no quoting/escaping. The client decodes it on the same
/// single code path (decode → inject css → apply ops), so the model is
/// unchanged; only the bytes shrink.
///
/// Frame layout (all lengths are LEB128 varints):
///   [css_len][css_utf8]
///   [op_count]
///   repeat op_count times:
///     [opcode:1]
///     [path_depth][idx]*path_depth        # the dotted path, as indices
///     [payload]                           # depends on opcode (see below)
///
/// payload:
///   set_text(0)/set_src(3): [str_len][str]
///   set_paint(1)/set_path(2)/replace(4)/insert(6)/paint(7): [html_len][html]
///   remove(5): (none)

#include "node.hpp"
#include "dom.hpp"
#include "diff.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace waya::surface {

namespace bin {

inline void put_varint(std::string& o, std::uint64_t v) {
    while (v >= 0x80) { o += char((v & 0x7F) | 0x80); v >>= 7; }
    o += char(v);
}
inline void put_str(std::string& o, std::string_view s) {
    put_varint(o, s.size()); o.append(s);
}
/// Split a dotted path "0.3.1" into varint indices; "" → depth 0.
inline void put_path(std::string& o, std::string_view path) {
    // count segments
    std::vector<std::uint64_t> idx;
    std::uint64_t cur = 0; bool any = false;
    for (char c : path) {
        if (c == '.') { idx.push_back(cur); cur = 0; }
        else { cur = cur * 10 + (c - '0'); any = true; }
    }
    if (any) idx.push_back(cur);
    put_varint(o, idx.size());
    for (auto i : idx) put_varint(o, i);
}

} // namespace bin

/// Encode a delta patch as a binary frame. Renders changed subtrees (collecting
/// their css) exactly like the JSON path, then packs everything.
inline std::string encode_delta(const Patch& p) {
    std::string css, ops; std::uint64_t n = 0;
    auto add_css = [&](const std::string& c){ if(!c.empty() && css.find(c)==std::string::npos) css += c; };
    for (const auto& op : p) {
        ++n;
        ops += char((std::uint8_t)op.op);
        bin::put_path(ops, op.path);
        switch (op.op) {
            case Op::set_text: case Op::set_src:
                bin::put_str(ops, op.s); break;
            case Op::set_paint: case Op::set_path: case Op::replace: case Op::insert: {
                if (op.node) { auto [html,c] = render_fragment(*op.node); add_css(c); bin::put_str(ops, html); }
                else bin::put_str(ops, "");
                break; }
            case Op::remove: break;
        }
    }
    std::string frame;
    bin::put_str(frame, css);
    bin::put_varint(frame, n);
    frame += ops;
    return frame;
}

/// Encode a full paint as a binary frame: one `paint` op (7) carrying the root.
inline std::string encode_full(const Node& root) {
    auto [html, css] = render_fragment(root);
    std::string frame;
    bin::put_str(frame, css);
    bin::put_varint(frame, 1);
    frame += char(7);            // OP_PAINT
    bin::put_varint(frame, 0);   // path depth 0 (root)
    bin::put_str(frame, html);
    return frame;
}

} // namespace waya::surface
