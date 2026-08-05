#pragma once
/// \file dom.hpp
/// The DOM rendering backend. Turns a surface into HTML with interned CSS
/// classes — one of waya's backends; your `view()` never knows it exists.
/// Emits the full style vocabulary (box model, flex, position, effects, states,
/// and the universal `css()` channel), so anything the surface can express
/// renders here.

#include "node.hpp"
#include "../core/hash.hpp"

#include <string>
#include <vector>

namespace waya::surface {

class DomBackend {
public:
    struct Output { std::string html; std::string css; };

    Output render(const Node& root) {
        rules_.clear(); names_.clear();
        std::string html; emit(html, root);
        std::string css;
        for (std::size_t i = 0; i < rules_.size(); ++i) css += rules_[i];
        return { std::move(html), std::move(css) };
    }

private:
    std::vector<std::string> rules_, names_;

    static void hex(std::string& o, std::uint32_t c){ static const char* H="0123456789abcdef";
        o+='#'; for(int s=20;s>=0;s-=4) o+=H[(c>>s)&0xF]; }
    static std::string n(float f){ long long i=(long long)f; if((float)i==f) return std::to_string(i);
        std::string s=std::to_string(f); while(s.size()&&s.back()=='0')s.pop_back(); if(s.size()&&s.back()=='.')s.pop_back(); return s; }
    static void esc(std::string& o, std::string_view s){ for(char c:s) switch(c){
        case '&':o+="&amp;";break; case '<':o+="&lt;";break; case '>':o+="&gt;";break; default:o+=c; } }

    static void len(std::string& o, const Len& l){
        switch(l.unit){ case Unit::px:o+=n(l.value);o+="px";break; case Unit::pct:o+=n(l.value);o+="%";break;
            case Unit::rem:o+=n(l.value);o+="rem";break; case Unit::em:o+=n(l.value);o+="em";break;
            case Unit::vw:o+=n(l.value);o+="vw";break; case Unit::vh:o+=n(l.value);o+="vh";break;
            case Unit::fr:o+=n(l.value);o+="fr";break; case Unit::fill:o+="100%";break; case Unit::hug:o+="auto";break; } }

    static const char* just(Justify j){ switch(j){ case Justify::start:return "flex-start"; case Justify::center:return "center";
        case Justify::end:return "flex-end"; case Justify::between:return "space-between"; case Justify::around:return "space-around";
        case Justify::evenly:return "space-evenly"; default:return ""; } }
    static const char* ali(Align a){ switch(a){ case Align::start:return "flex-start"; case Align::center:return "center";
        case Align::end:return "flex-end"; case Align::stretch:return "stretch"; case Align::baseline:return "baseline"; default:return ""; } }
    static const char* wt(Weight w){ switch(w){ case Weight::thin:return "100"; case Weight::light:return "300";
        case Weight::normal:return "400"; case Weight::medium:return "500"; case Weight::semibold:return "600";
        case Weight::bold:return "700"; case Weight::black:return "900"; default:return ""; } }

    /// CSS declarations for a style (no selector). Used for the base rule and,
    /// recursively (minus states), for each state/breakpoint overlay.
    std::string decls(const Style& s, Kind kind, bool is_tap) {
        std::string o;
        // layout
        switch(s.flow){ case Flow::row:o+="display:flex;flex-direction:row;";break;
            case Flow::col:o+="display:flex;flex-direction:column;";break;
            case Flow::stack:o+="display:grid;";break; case Flow::none:break; }
        if(s.justify!=Justify::none){ o+="justify-content:"; o+=just(s.justify); o+=';'; }
        if(s.align!=Align::none){ o+="align-items:"; o+=ali(s.align); o+=';'; }
        if(s.wrap==Wrap::wrap) o+="flex-wrap:wrap;"; else if(s.wrap==Wrap::nowrap) o+="flex-wrap:nowrap;";
        if(s.gap.set()){ o+="gap:"; len(o,s.gap); o+=';'; }
        // `grow` = flex: <g> 1 auto — grow to share free space but keep the
        // element's own content as the basis. Emitting bare `flex-grow` (basis
        // 0) made a grow child in a column eat the whole page height.
        if(s.has_grow){ o+="flex:"; o+=n(s.grow); o+=" 1 auto;"; }
        else if(s.has_shrink){ o+="flex-shrink:"; o+=n(s.shrink); o+=';'; }
        // text
        if(kind==Kind::text){ if(s.has_fg){o+="color:";hex(o,s.fg);o+=';';}
            if(s.font_size.set()){o+="font-size:";len(o,s.font_size);o+=';';}
            if(s.weight!=Weight::none){o+="font-weight:";o+=wt(s.weight);o+=';';}
            if(s.italic)o+="font-style:italic;"; if(s.underline)o+="text-decoration:underline;"; if(s.strike)o+="text-decoration:line-through;";
            if(s.has_lh){o+="line-height:";o+=n(s.line_height);o+=';';} if(s.has_ls){o+="letter-spacing:";o+=n(s.letter_spacing);o+="px;"; } }
        if(s.text_align!=Justify::none){ o+="text-align:"; o+= s.text_align==Justify::center?"center":s.text_align==Justify::end?"right":"left"; o+=';'; }
        // box model
        if(s.has_bg){o+="background:";hex(o,s.bg);o+=';';}
        if(s.pad.set()){o+="padding:";len(o,s.pad);o+=';';}
        if(s.pad_x.set()){o+="padding-left:";len(o,s.pad_x);o+=";padding-right:";len(o,s.pad_x);o+=';';}
        if(s.pad_y.set()){o+="padding-top:";len(o,s.pad_y);o+=";padding-bottom:";len(o,s.pad_y);o+=';';}
        if(s.margin.set()){o+="margin:";len(o,s.margin);o+=';';}
        if(s.w.set()){o+="width:";len(o,s.w);o+=';';} if(s.h.set()){o+="height:";len(o,s.h);o+=';';}
        if(s.min_w.set()){o+="min-width:";len(o,s.min_w);o+=';';} if(s.max_w.set()){o+="max-width:";len(o,s.max_w);o+=';';}
        if(s.min_h.set()){o+="min-height:";len(o,s.min_h);o+=';';} if(s.max_h.set()){o+="max-height:";len(o,s.max_h);o+=';';}
        if(s.radius.set()){o+="border-radius:";len(o,s.radius);o+=';';}
        if(s.has_border){o+="border:";len(o,s.border_w);o+=" solid ";hex(o,s.border_c);o+=';';}
        // position
        switch(s.pos){ case Pos::relative:o+="position:relative;";break; case Pos::absolute:o+="position:absolute;";break;
            case Pos::fixed:o+="position:fixed;";break; case Pos::sticky:o+="position:sticky;";break; case Pos::none:break; }
        if(s.top.set()){o+="top:";len(o,s.top);o+=';';} if(s.left.set()){o+="left:";len(o,s.left);o+=';';}
        if(s.right.set()){o+="right:";len(o,s.right);o+=';';} if(s.bottom.set()){o+="bottom:";len(o,s.bottom);o+=';';}
        if(s.has_z){o+="z-index:";o+=std::to_string(s.z);o+=';';}
        // effects
        if(s.has_shadow){ o+="box-shadow:"; o+= s.shadow_spec.empty()? "0 10px 30px rgba(0,0,0,.35)" : s.shadow_spec; o+=';'; }
        if(s.has_opacity){o+="opacity:";o+=n(s.opacity);o+=';';}
        switch(s.cursor){ case Cursor::pointer:o+="cursor:pointer;";break; case Cursor::text:o+="cursor:text;";break;
            case Cursor::move:o+="cursor:move;";break; case Cursor::not_allowed:o+="cursor:not-allowed;";break; case Cursor::none:break; }
        if(is_tap && s.cursor==Cursor::none) o+="cursor:pointer;";
        if(s.has_transition){o+="transition:";o+=s.transition_spec;o+=';';}
        // the universal channel — anything
        for(auto&[k,v]:s.extra){ o+=k; o+=':'; o+=v; o+=';'; }
        return o;
    }

    /// Intern a full style (base + states) → one class name. Emits the base
    /// rule plus `.cls:hover{…}` / `@media…{.cls{…}}` for each state.
    std::string_view intern(const Style& s, Kind kind, bool is_tap) {
        std::string base = decls(s, kind, is_tap);
        // identity includes states so two nodes differing only in :hover differ
        std::string ident = base;
        for(auto&[sel,st]:s.states) ident += sel + decls(*st, kind, false);
        if(ident.empty()) return {};
        for(std::size_t i=0;i<names_.size();++i) if(idents_[i]==ident) return names_[i];
        std::string name="ws-"; hex8(name, fnv1a(ident));
        std::string rule;
        if(!base.empty()){ rule += '.'; rule += name; rule += '{'; rule += base; rule += '}'; }
        std::string media;
        for(auto&[sel,st]:s.states){ std::string body=decls(*st, kind, false); if(body.empty()) continue;
            if(sel.rfind("@media",0)==0){ media += sel; media += "{."; media += name; media += '{'; media += body; media += "}}"; }
            else { rule += '.'; rule += name; rule += sel; rule += '{'; rule += body; rule += '}'; } }
        rule += media;
        rules_.push_back(std::move(rule)); names_.push_back(std::move(name)); idents_.push_back(std::move(ident));
        return names_.back();
    }
    std::vector<std::string> idents_;

    void open_attrs(std::string& o, const Node& nd){
        auto cls = intern(nd.style, nd.kind, nd.on_tap>=0);
        if(!cls.empty()){ o+=" class=\""; o+=cls; o+='"'; }
        if(nd.on_tap>=0){ o+=" data-tap=\""; o+=std::to_string(nd.on_tap); o+='"'; }
    }

    void emit(std::string& o, const Node& nd){
        switch(nd.kind){
            case Kind::text: o+="<span"; open_attrs(o,nd); o+='>'; esc(o,nd.text); o+="</span>"; return;
            case Kind::image: o+="<img src=\""; esc(o,nd.src); o+='"'; open_attrs(o,nd); o+='>'; return;
            case Kind::input: {
                o+="<input type=\""; esc(o,nd.input_type.empty()?"text":nd.input_type); o+='"';
                o+=" value=\""; esc(o,nd.text); o+='"';
                if(!nd.placeholder.empty()){ o+=" placeholder=\""; esc(o,nd.placeholder); o+='"'; }
                if(nd.on_input>=0){ o+=" data-input=\""; o+=std::to_string(nd.on_input); o+='"'; }
                if(nd.on_change>=0){ o+=" data-change=\""; o+=std::to_string(nd.on_change); o+='"'; }
                auto cls = intern(nd.style, nd.kind, false);
                if(!cls.empty()){ o+=" class=\""; o+=cls; o+='"'; }
                o+=">"; return;
            }
            case Kind::path: {
                o+="<svg"; open_attrs(o,nd); o+="><polyline points=\"";
                for(auto&p:nd.points){ o+=n(p.x); o+=','; o+=n(p.y); o+=' '; }
                o+="\" fill=\""; if(nd.closed&&nd.style.has_bg) hex(o,nd.style.bg); else o+="none";
                o+="\" stroke=\""; hex(o, nd.style.has_fg?nd.style.fg:0xffffff);
                o+="\" stroke-width=\""; o+=n(nd.style.has_stroke_w?nd.style.stroke_w:2);
                o+="\" fill-opacity=\".15\"/></svg>"; return; }
            case Kind::box: o+="<div"; open_attrs(o,nd); o+='>';
                for(auto&k:nd.kids) emit(o,*k); o+="</div>"; return;
        }
    }
};

} // namespace waya::surface
