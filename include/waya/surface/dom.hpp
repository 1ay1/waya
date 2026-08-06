#pragma once
/// \file dom.hpp
/// The DOM rendering backend. Turns a surface into HTML with interned CSS
/// classes — one of waya's backends; your `view()` never knows it exists.
/// Emits the full style vocabulary (box model, flex, position, effects, states,
/// and the universal `css()` channel), so anything the surface can express
/// renders here.

#include "node.hpp"
#include "../core/hash.hpp"

#include <algorithm>
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
            // stack = ZStack: a 1-cell grid where every child occupies the SAME
            // cell (overlaid), centred by default. For badges, overlays, hero
            // text on an image. The child-overlap rule is emitted in intern().
            case Flow::stack:o+="display:grid;grid-template:minmax(0,1fr)/minmax(0,1fr);place-items:center;";break;
            case Flow::none:break; }
        // Flex correctness: min-width:0 lets a flex CHILD shrink below its
        // content size. CSS defaults flex items to min-width:auto, which causes
        // overflow and breaks truncation/sidebars/wrapping — the classic flexbox
        // footgun. waya defaults it right so layouts just behave.
        if(s.flow==Flow::row||s.flow==Flow::col) o+="min-width:0;";
        if(s.justify!=Justify::none){ o+="justify-content:"; o+=just(s.justify); o+=';'; }
        // Cross-axis alignment. If the author didn't set one, pick the sensible
        // default per direction: a ROW vertically-centres its children (mixed
        // content — text + a badge + an icon — lines up on its centre, which is
        // what every design system does); a COL keeps the CSS default (stretch)
        // so children fill the width. Authors override with align(…)/center.
        if(s.align!=Align::none){ o+="align-items:"; o+=ali(s.align); o+=';'; }
        else if(s.flow==Flow::row){ o+="align-items:center;"; }
        if(s.wrap==Wrap::wrap) o+="flex-wrap:wrap;"; else if(s.wrap==Wrap::nowrap) o+="flex-wrap:nowrap;";
        if(s.gap.set()){ o+="gap:"; len(o,s.gap); o+=';'; }
        // `grow` = flex: <g> 1 auto — grow to share free space but keep the
        // element's own content as the basis. Emitting bare `flex-grow` (basis
        // 0) made a grow child in a column eat the whole page height.
        if(s.has_grow){ o+="flex:"; o+=n(s.grow); o+=" 1 auto;"; }
        else if(s.has_shrink){ o+="flex-shrink:"; o+=n(s.shrink); o+=';'; }
        // text
        // Colour INHERITS, so a container's fg cascades to its text descendants —
        // emit it for any node (not just text-bearing ones).
        if(s.has_fg){o+="color:";hex(o,s.fg);o+=';';}
        // Typography applies to text AND text-bearing controls.
        if(kind==Kind::text || kind==Kind::input || kind==Kind::textarea ||
           kind==Kind::button || kind==Kind::select){
            if(s.font_size.set()){o+="font-size:";len(o,s.font_size);o+=';';}
            if(s.weight!=Weight::none){o+="font-weight:";o+=wt(s.weight);o+=';';}
            if(s.italic)o+="font-style:italic;";
            // underline + strike both map to text-decoration — combine so you can
            // have both ("underline line-through"), not have the last one win.
            if(s.underline || s.strike){ o+="text-decoration:"; if(s.underline)o+="underline"; if(s.underline&&s.strike)o+=' '; if(s.strike)o+="line-through"; o+=';'; }
            if(s.has_lh){o+="line-height:";o+=n(s.line_height);o+=';';} if(s.has_ls){o+="letter-spacing:";o+=n(s.letter_spacing);o+="px;"; } }
        if(s.text_align!=Justify::none){ o+="text-align:"; o+= s.text_align==Justify::center?"center":s.text_align==Justify::end?"right":"left"; o+=';'; }
        // box model
        // background: the solid bg(), UNLESS the extra channel also sets one (a
        // gradient/mesh) — then we skip this so we don't emit two declarations.
        if(s.has_bg){
            bool extra_bg=false; for(auto&[k,v]:s.extra) if(k=="background"){ extra_bg=true; break; }
            if(!extra_bg){ o+="background:"; hex(o,s.bg); o+=';'; }
        }
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
        // the universal channel — anything. Some properties (transform, filter,
        // backdrop-filter) are ADDITIVE: multiple mods (scale + rotate, blur +
        // brightness) each add a function, so we CONCATENATE same-key values with
        // a space instead of letting the last one win (which would drop scale).
        {
            std::vector<std::pair<std::string,std::string>> merged;
            auto is_additive = [](const std::string& k){
                return k=="transform" || k=="filter" || k=="backdrop-filter" ||
                       k=="-webkit-backdrop-filter" || k=="box-shadow";
            };
            for(auto&[k,v]:s.extra){
                bool done=false;
                if(is_additive(k)){
                    for(auto& m:merged) if(m.first==k){ m.second += (k=="box-shadow"?", ":" ")+v; done=true; break; }
                } else {
                    for(auto& m:merged) if(m.first==k){ m.second=v; done=true; break; }  // last wins
                }
                if(!done) merged.emplace_back(k,v);
            }
            for(auto&[k,v]:merged){ o+=k; o+=':'; o+=v; o+=';'; }
        }
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
        // ZStack: every direct child shares the single grid cell so they overlay.
        if(s.flow==Flow::stack){ rule += '.'; rule += name; rule += ">*{grid-area:1/1}"; }
        std::string media;
        for(auto&[sel,st]:s.states){ std::string body=decls(*st, kind, false); if(body.empty()) continue;
            if(sel.rfind("@media",0)==0){ media += sel; media += "{."; media += name; media += '{'; media += body; media += "}}"; }
            else { rule += '.'; rule += name; rule += sel; rule += '{'; rule += body; rule += '}'; } }
        rule += media;
        rules_.push_back(std::move(rule)); names_.push_back(std::move(name)); idents_.push_back(std::move(ident));
        return names_.back();
    }
    std::vector<std::string> idents_;

    /// Generic wired events + draggable. Each event becomes data-ev-<name>=
    /// "<msg>" or "<msg>|<arg>" (arg narrows, e.g. a key). The client reads these
    /// with one delegated listener per event type.
    void event_attrs(std::string& o, const Node& nd){
        for(auto& e : nd.events){
            o+=" data-ev-"; o+=e.event; o+="=\""; o+=std::to_string(e.msg);
            if(!e.arg.empty()){ o+='|'; esc(o,e.arg); }
            o+='"';
        }
        if(nd.draggable){ o+=" draggable=\"true\"";
            // the drag payload rides in name; emit it here for non-control nodes
            // (control_attrs already emits name for form controls).
            if(!nd.name.empty() && nd.kind!=Kind::input && nd.kind!=Kind::textarea &&
               nd.kind!=Kind::checkbox && nd.kind!=Kind::radio && nd.kind!=Kind::select){
                o+=" name=\""; esc(o,nd.name); o+='"';
            }
        }
        // arbitrary attributes (aria-*, role, title, data-*, controls…)
        for(auto& a : nd.attrs){
            o+=' '; o+=a.first;
            if(!a.second.empty()){ o+="=\""; esc(o,a.second); o+='"'; }
        }
    }

    void open_attrs(std::string& o, const Node& nd){
        auto cls = intern(nd.style, nd.kind, nd.on_tap>=0);
        if(!cls.empty()){ o+=" class=\""; o+=cls; o+='"'; }
        if(nd.on_tap>=0){ o+=" data-tap=\""; o+=std::to_string(nd.on_tap); o+='"'; }
        event_attrs(o,nd);
    }

    /// Shared control attributes: class, input/change wiring, disabled, name.
    void control_attrs(std::string& o, const Node& nd){
        auto cls = intern(nd.style, nd.kind, false);
        if(!cls.empty()){ o+=" class=\""; o+=cls; o+='"'; }
        if(!nd.name.empty()){ o+=" name=\""; esc(o,nd.name); o+='"'; }
        if(nd.on_input>=0){ o+=" data-input=\""; o+=std::to_string(nd.on_input); o+='"'; }
        if(nd.on_change>=0){ o+=" data-change=\""; o+=std::to_string(nd.on_change); o+='"'; }
        if(nd.disabled) o+=" disabled";
        event_attrs(o,nd);
    }

    void emit(std::string& o, const Node& nd){
        switch(nd.kind){
            case Kind::text: {
                // A text is a <span> by default, or the semantic tag if `as(...)`
                // set one (h1…h6, p, label). Layout unchanged; SEO/a11y improved.
                const std::string& tg = nd.tag.empty() ? std::string("span") : nd.tag;
                o+='<'; o+=tg; open_attrs(o,nd); o+='>'; esc(o,nd.text); o+="</"; o+=tg; o+='>'; return;
            }
            case Kind::image: o+="<img src=\""; esc(o,nd.src); o+='"'; open_attrs(o,nd); o+='>'; return;
            case Kind::input: {
                o+="<input type=\""; esc(o,nd.input_type.empty()?"text":nd.input_type); o+='"';
                o+=" value=\""; esc(o,nd.text); o+='"';
                if(!nd.placeholder.empty()){ o+=" placeholder=\""; esc(o,nd.placeholder); o+='"'; }
                control_attrs(o,nd);
                o+=">"; return;
            }
            case Kind::textarea: {
                o+="<textarea";
                if(!nd.placeholder.empty()){ o+=" placeholder=\""; esc(o,nd.placeholder); o+='"'; }
                control_attrs(o,nd);
                o+='>'; esc(o,nd.text); o+="</textarea>"; return;
            }
            case Kind::checkbox: {
                o+="<input type=\"checkbox\"";
                if(nd.checked) o+=" checked";
                control_attrs(o,nd);
                o+=">"; return;
            }
            case Kind::radio: {
                o+="<input type=\"radio\" value=\""; esc(o,nd.text); o+='"';
                if(nd.checked) o+=" checked";
                control_attrs(o,nd);
                o+=">"; return;
            }
            case Kind::select: {
                o+="<select"; control_attrs(o,nd); o+='>';
                for(auto&opt:nd.options){
                    o+="<option value=\""; esc(o,opt.value); o+='"';
                    if(opt.value==nd.selected) o+=" selected";
                    o+='>'; esc(o,opt.label); o+="</option>";
                }
                o+="</select>"; return;
            }
            case Kind::button: {
                o+="<button type=\""; o+=(nd.name=="submit"?"submit":"button"); o+='"';
                auto cls = intern(nd.style, nd.kind, true);
                if(!cls.empty()){ o+=" class=\""; o+=cls; o+='"'; }
                if(nd.on_tap>=0){ o+=" data-tap=\""; o+=std::to_string(nd.on_tap); o+='"'; }
                if(nd.disabled) o+=" disabled";
                event_attrs(o,nd);
                o+='>'; esc(o,nd.text); o+="</button>"; return;
            }
            case Kind::form: o+="<form"; open_attrs(o,nd); o+='>';
                for(auto&k:nd.kids){ emit(o,*k); } o+="</form>"; return;
            case Kind::video: o+="<video src=\""; esc(o,nd.src); o+='"'; open_attrs(o,nd); o+="></video>"; return;
            case Kind::audio: o+="<audio src=\""; esc(o,nd.src); o+='"'; open_attrs(o,nd); o+="></audio>"; return;
            case Kind::markup: o+="<div"; open_attrs(o,nd); o+='>'; o+=nd.text /*raw, trusted*/; o+="</div>"; return;
            case Kind::path: {
                // Compute the points' bounds → a viewBox, so the SVG SCALES to fit
                // its container instead of drawing at raw pixel coordinates that
                // overflow. width/height:100% + preserveAspectRatio=none stretch
                // it to the box waya laid out. (Without a viewBox the polyline
                // spilled outside its card.)
                float minx=1e9f,miny=1e9f,maxx=-1e9f,maxy=-1e9f;
                for(auto&p:nd.points){ minx=std::min(minx,p.x); maxx=std::max(maxx,p.x);
                                       miny=std::min(miny,p.y); maxy=std::max(maxy,p.y); }
                if(nd.points.empty()){ minx=miny=0; maxx=maxy=1; }
                float pw = std::max(1.f, maxx-minx), ph = std::max(1.f, maxy-miny);
                float sw = nd.style.has_stroke_w?nd.style.stroke_w:2;
                // pad the viewBox by half the stroke so the line isn't clipped
                o+="<svg"; open_attrs(o,nd);
                o+=" viewBox=\""; o+=n(minx-sw); o+=' '; o+=n(miny-sw); o+=' ';
                o+=n(pw+2*sw); o+=' '; o+=n(ph+2*sw); o+='"';
                o+=" preserveAspectRatio=\"none\" style=\"width:100%;height:100%;display:block\">";
                o+="<polyline points=\"";
                for(auto&p:nd.points){ o+=n(p.x); o+=','; o+=n(p.y); o+=' '; }
                o+="\" fill=\""; if(nd.closed&&nd.style.has_bg) hex(o,nd.style.bg); else o+="none";
                o+="\" stroke=\""; hex(o, nd.style.has_fg?nd.style.fg:0xffffff);
                o+="\" stroke-width=\""; o+=n(sw);
                o+="\" stroke-linejoin=\"round\" stroke-linecap=\"round\"";
                o+=" vector-effect=\"non-scaling-stroke\" fill-opacity=\".15\"/></svg>"; return; }
            case Kind::box: {
                // A box is a <div> by default, or the semantic element `as(...)`
                // chose (main/nav/header/article/section…) — a real landmark.
                const std::string& tg = nd.tag.empty() ? std::string("div") : nd.tag;
                o+='<'; o+=tg; open_attrs(o,nd); o+='>';
                for(auto&k:nd.kids){ emit(o,*k); } o+="</"; o+=tg; o+='>'; return;
            }
        }
    }
};

} // namespace waya::surface
