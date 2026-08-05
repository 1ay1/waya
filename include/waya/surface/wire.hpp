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

/// A patch → JSON: [[op, path, html?], …] plus a css blob of any new rules the
/// changed fragments need. Shape: {"css": "...", "ops": [...]}.
inline std::string patch_json(const Patch& p){
    // Render every changed/inserted/replaced subtree once, collect css.
    std::string ops="["; std::string css;
    auto add_css=[&](const std::string& c){ if(!c.empty() && css.find(c)==std::string::npos) css+=c; };
    for(std::size_t i=0;i<p.size();++i){
        if(i) ops+=',';
        const auto& op=p[i];
        ops+='['; ops+=std::to_string((int)op.op); ops+=','; detail::jstr(ops,op.path);
        switch(op.op){
            case Op::set_text: ops+=','; detail::jstr(ops, op.s); break;
            case Op::set_src:  ops+=','; detail::jstr(ops, op.s); break;
            case Op::set_paint: case Op::set_path: case Op::replace: case Op::insert: {
                if(op.node){ auto [html,c]=render_fragment(*op.node); add_css(c); ops+=','; detail::jstr(ops, html); }
                else ops+=",\"\"";
                break; }
            case Op::remove: break;
        }
        ops+=']';
    }
    ops+=']';
    std::string o="{\"css\":"; detail::jstr(o, css); o+=",\"ops\":"; o+=ops; o+="}";
    return o;
}

} // namespace waya::surface
