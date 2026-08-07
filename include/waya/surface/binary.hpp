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
///   replace(0)/set_shell(1)/insert(6)/insert_at(8)/paint(9): [html_len][html]
///   set_text(2)/set_inner(3): [str_len][str]   set_prop(4): [prop][value]
///   set_prop = [prop_len][prop][val_len][val]   move(7): [from][to]   remove(5): none
///   insert_at(8): [to varint][html_len][html]

#include "node.hpp"
#include "dom.hpp"
#include "diff.hpp"
#include "wire.hpp"   // render_fragment / render_paint

#include <cstdint>
#include <string>
#include <unordered_set>
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
    // O(1) css dedup (was an O(len) substring scan per op = quadratic). The set
    // owns its keys since the css handed in is a temporary from render_*.
    std::unordered_set<std::string> seen;
    auto add_css = [&](const std::string& c){ if(!c.empty() && seen.insert(c).second) css += c; };
    for (const auto& op : p) {
        ++n;
        // Wire opcodes match wire.hpp: base ops are the Op enum ordinal; a keyed
        // insert with a target index is WIRE_INSERT_AT; OP_PAINT is a full paint.
        bool insert_at = (op.op == Op::insert && op.to >= 0);
        std::uint8_t wire = insert_at ? (std::uint8_t)WIRE_INSERT_AT : (std::uint8_t)op.op;
        ops += char(wire);
        bin::put_path(ops, op.path);
        switch (op.op) {
            case Op::set_text: case Op::set_inner:
                bin::put_str(ops, op.s); break;
            case Op::set_prop:
                bin::put_str(ops, op.prop); bin::put_str(ops, op.s); break;
            case Op::set_shell: {
                // attrs-only on the client -> ship the shallow shell, not the body.
                if (op.node) { auto [html,c] = render_shell(*op.node); add_css(c); bin::put_str(ops, html); }
                else bin::put_str(ops, "");
                break; }
            case Op::replace: {
                if (op.node) { auto [html,c] = render_fragment(*op.node); add_css(c); bin::put_str(ops, html); }
                else bin::put_str(ops, "");
                break; }
            case Op::insert: {
                if (insert_at) bin::put_varint(ops, (std::uint64_t)op.to);
                if (op.node) { auto [html,c] = render_fragment(*op.node); add_css(c); bin::put_str(ops, html); }
                else bin::put_str(ops, "");
                break; }
            case Op::move:
                bin::put_varint(ops, (std::uint64_t)op.from);
                bin::put_varint(ops, (std::uint64_t)op.to);
                break;
            case Op::remove: break;
        }
    }
    std::string frame;
    bin::put_str(frame, css);
    bin::put_varint(frame, n);
    frame += ops;
    return frame;
}

/// Encode a full paint as a binary frame: one `paint` op (OP_PAINT) carrying
/// the root.
inline std::string encode_full(const Node& root) {
    auto [html, css] = render_fragment(root);
    std::string frame;
    bin::put_str(frame, css);
    bin::put_varint(frame, 1);
    frame += char(OP_PAINT);
    bin::put_varint(frame, 0);   // path depth 0 (root)
    bin::put_str(frame, html);
    return frame;
}

} // namespace waya::surface
