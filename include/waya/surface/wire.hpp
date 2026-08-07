#pragma once
/// \file wire.hpp
/// The wire format. The server renders changed nodes with the DOM backend and
/// ships HTML fragments + the stylesheet — so the client is dumb (innerHTML /
/// replaceWith) and the FULL style vocabulary works automatically, no style
/// reconstruction in JS. Patches are addressed by node path.

#include "node.hpp"
#include "dom.hpp"
#include "diff.hpp"

#include <string>
#include <unordered_set>

namespace waya::surface {

namespace detail {
inline void jstr(std::string& o, std::string_view s){ o+='"';
    for(char c:s) switch(c){ case '"':o+="\\\"";break; case '\\':o+="\\\\";break; case '\n':o+="\\n";break; case '\r':o+="\\r";break;
        default: if((unsigned char)c<0x20){o+="\\u00";static const char* H="0123456789abcdef";o+=H[(c>>4)&0xF];o+=H[c&0xF];} else o+=c; }
    o+='"'; }
}

/// Render one node subtree to an HTML string (with a fresh backend so it's
/// self-contained; the CSS it needs is sent alongside).
inline std::pair<std::string,std::string> render_fragment(const Node& n){
    DomBackend b; auto out = b.render(n); return { out.html, out.css };
}

/// Render just the node's OWN element (attrs + wiring, empty body) for a
/// set_shell op — the client morphs attributes in place and keeps the existing
/// children/body, so shipping the subtree would be wasted bytes the client
/// discards. ALWAYS shallow now: set_shell's client action is attrs-only for
/// every kind (body channels ride their own ops), so no kind needs the body
/// here.
inline std::pair<std::string,std::string> render_shell(const Node& n){
    DomBackend b; auto out = b.render_shallow(n); return { out.html, out.css };
}

// Wire opcodes shared by the JSON encoder, the binary encoder, and the JS
// client (surface/client.hpp). The base ops are the Op enum ordinals:
//   replace=0 set_shell=1 set_text=2 set_inner=3 set_prop=4 remove=5 insert=6 move=7
// Two extra codes cover forms the base ordinals don't distinguish:
static constexpr int WIRE_INSERT_AT = 8;   // keyed insert carrying a target index
static constexpr int OP_PAINT       = 9;   // full-surface repaint (root html)

/// A patch/frame → JSON. ONE shape for everything the terminal ever receives:
///
///   {"css": "<rules>", "ops": [ [op, path, payload?], ... ]}
///
/// A DELTA is the changed ops. A FULL PAINT is the same shape with a single
/// `paint` op (opcode 7) carrying the whole root HTML — "repaint everything",
/// the maya "all cells changed" case. The terminal has exactly ONE code path:
/// inject css, apply ops. It holds no app state and is repaintable from any
/// full frame at any moment (reconnect, drift) with no negotiation.
inline std::string ops_json(const Patch& p, std::string& css_out){
    std::string ops="[";
    // dedup css by exact chunk in O(1), not an O(len) substring scan per op
    // (that was quadratic in the number of ops × accumulated css length). The
    // set OWNS its keys — the css string handed in is a temporary.
    std::unordered_set<std::string> seen;
    auto add_css=[&](const std::string& c){ if(!c.empty() && seen.insert(c).second) css_out+=c; };
    for(std::size_t i=0;i<p.size();++i){
        if(i) ops+=',';
        const auto& op=p[i];
        // Wire opcodes. The base ops use the Op enum ordinal; two structural
        // forms get their own code so the client can size their payload:
        //   insert with a target index -> insert_at (WIRE_INSERT_AT=8); plain append -> insert.
        //   move -> its own [from,to] shape.
        // A full paint is OP_PAINT (9). Everything else carries a single string.
        bool insert_at = (op.op==Op::insert && op.to>=0);
        int wire = insert_at ? WIRE_INSERT_AT : (int)op.op;
        ops+='['; ops+=std::to_string(wire); ops+=','; detail::jstr(ops,op.path);
        switch(op.op){
            case Op::set_text: case Op::set_inner: ops+=','; detail::jstr(ops, op.s); break;
            case Op::set_prop: ops+=','; detail::jstr(ops, op.prop); ops+=','; detail::jstr(ops, op.s); break;
            case Op::set_shell: {
                if(op.node){ auto [html,c]=render_shell(*op.node); add_css(c); ops+=','; detail::jstr(ops, html); }
                else ops+=",\"\"";
                break; }
            case Op::replace: {
                if(op.node){ auto [html,c]=render_fragment(*op.node); add_css(c); ops+=','; detail::jstr(ops, html); }
                else ops+=",\"\"";
                break; }
            case Op::insert: {
                if(insert_at){ ops+=','; ops+=std::to_string(op.to); }
                if(op.node){ auto [html,c]=render_fragment(*op.node); add_css(c); ops+=','; detail::jstr(ops, html); }
                else ops+=",\"\"";
                break; }
            case Op::move:
                ops+=','; ops+=std::to_string(op.from);
                ops+=','; ops+=std::to_string(op.to); break;
            case Op::remove: break;
        }
        ops+=']';
    }
    ops+=']';
    return ops;
}

/// A delta frame (only what changed).
inline std::string delta_frame(const Patch& p){
    std::string css; std::string ops = ops_json(p, css);
    std::string o="{\"css\":"; detail::jstr(o, css); o+=",\"ops\":"; o+=ops; o+="}";
    return o;
}

/// A full-paint frame: repaint the whole surface. Same shape as a delta — one
/// `paint` op (OP_PAINT) carrying the root HTML. This is what makes the terminal
/// trivially resyncable: hand it a full frame and it's correct, no matter what
/// state it was in.
inline std::string full_frame(const Node& root){
    auto [html, css] = render_fragment(root);
    std::string o="{\"css\":"; detail::jstr(o, css);
    o+=",\"ops\":[["; o+=std::to_string(OP_PAINT); o+=",\"\","; detail::jstr(o, html); o+="]]}";
    return o;
}

// Back-compat name used elsewhere.
inline std::string patch_json(const Patch& p){ return delta_frame(p); }

} // namespace waya::surface
