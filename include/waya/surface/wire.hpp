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
    auto add_css=[&](const std::string& c){ if(!c.empty() && css_out.find(c)==std::string::npos) css_out+=c; };
    for(std::size_t i=0;i<p.size();++i){
        if(i) ops+=',';
        const auto& op=p[i];
        bool insert_at = (op.op==Op::insert && op.to>=0);
        int wire = insert_at ? 9 : (op.op==Op::move ? 8 : (int)op.op);
        ops+='['; ops+=std::to_string(wire); ops+=','; detail::jstr(ops,op.path);
        switch(op.op){
            case Op::set_text: case Op::set_src: ops+=','; detail::jstr(ops, op.s); break;
            case Op::set_paint: case Op::set_path: case Op::replace: {
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
/// `paint` op (7) carrying the root HTML. This is what makes the terminal
/// trivially resyncable: hand it a full frame and it's correct, no matter what
/// state it was in.
static constexpr int OP_PAINT = 7;
inline std::string full_frame(const Node& root){
    auto [html, css] = render_fragment(root);
    std::string o="{\"css\":"; detail::jstr(o, css);
    o+=",\"ops\":[["; o+=std::to_string(OP_PAINT); o+=",\"\","; detail::jstr(o, html); o+="]]}";
    return o;
}

// Back-compat name used elsewhere.
inline std::string patch_json(const Patch& p){ return delta_frame(p); }

} // namespace waya::surface
