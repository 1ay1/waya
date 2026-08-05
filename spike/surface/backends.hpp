#pragma once
// waya::surface backends — the proof that the rendering substrate is invisible.
//
// A Backend turns the SAME Surface node tree into output. Two are implemented:
//
//   DomBackend    → HTML + CSS (a <div> here, styled text there). What a browser
//                   likes for forms, flowing text, accessibility.
//   CanvasBackend → a list of canvas draw-ops (fillRect, fillText, moveTo…).
//                   What a browser likes for a 10k-point chart or custom pixels.
//
// The user's `view()` produces a Surface and NEVER knows which backend renders
// it. That is the whole point: you describe WHAT to render; waya owns HOW.
// HTML, CSS, and canvas are implementation details waya picks — swap the
// backend and the app is unchanged.
//
// (Real waya would DIFF the surface and stream only the deltas — see the test.
//  These backends render a full frame; the diff lives in the framework proper.)

#include "surface.hpp"

#include <string>

namespace waya::surface {

namespace detail {
inline void hex(std::string& o, uint32_t argb) {
    static const char* H = "0123456789abcdef";
    o += '#';
    for (int sh = 20; sh >= 0; sh -= 4) o += H[(argb >> sh) & 0xF];  // rgb (drop alpha)
}
inline std::string num(float f) {
    long long i = (long long)f;
    if ((float)i == f) return std::to_string(i);
    std::string s = std::to_string(f);
    while (s.size() && s.back()=='0') s.pop_back();
    if (s.size() && s.back()=='.') s.pop_back();
    return s;
}
} // namespace detail

// ── Backend A: DOM / HTML + CSS ─────────────────────────────────────────────
struct DomBackend {
    static std::string render(const Node& n) { std::string o; emit(o, n); return o; }

    static void emit(std::string& o, const Node& n) {
        switch (n.kind) {
            case Kind::text: {
                o += "<span style=\""; text_style(o, n); o += "\"";
                tap(o, n); o += ">"; esc(o, n.text); o += "</span>";
                return;
            }
            case Kind::image:
                o += "<img src=\""; esc(o, n.src); o += "\""; tap(o, n); o += ">";
                return;
            case Kind::path: {
                // A vector shape → inline SVG (still no HTML the user wrote).
                o += "<svg><polyline points=\"";
                for (auto& [x,y] : n.points) { o += detail::num(x); o += ','; o += detail::num(y); o += ' '; }
                o += "\" fill=\""; if (n.closed) detail::hex(o, n.paint.bg); else o += "none";
                o += "\" stroke=\""; detail::hex(o, n.paint.fg); o += "\"/></svg>";
                return;
            }
            case Kind::box: {
                o += "<div style=\""; box_style(o, n); o += "\""; tap(o, n); o += ">";
                for (auto& k : n.kids) emit(o, *k);
                o += "</div>";
                return;
            }
        }
    }

    static void box_style(std::string& o, const Node& n) {
        switch (n.flow) {
            case Flow::row:   o += "display:flex;flex-direction:row;"; break;
            case Flow::col:   o += "display:flex;flex-direction:column;"; break;
            case Flow::stack: o += "display:grid;"; break;   // overlay
            case Flow::none:  break;
        }
        if (n.paint.gap)    { o += "gap:"; o += detail::num(n.paint.gap); o += "px;"; }
        if (n.paint.pad)    { o += "padding:"; o += detail::num(n.paint.pad); o += "px;"; }
        if (n.paint.radius) { o += "border-radius:"; o += detail::num(n.paint.radius); o += "px;"; }
        if (n.paint.bg & 0xff000000) { o += "background:"; detail::hex(o, n.paint.bg); o += ';'; }
    }
    static void text_style(std::string& o, const Node& n) {
        o += "color:"; detail::hex(o, n.paint.fg); o += ';';
        o += "font-size:"; o += detail::num(n.paint.size); o += "px;";
        if (n.paint.bold) o += "font-weight:700;";
    }
    static void tap(std::string& o, const Node& n) {
        if (n.on_tap >= 0) { o += " data-tap=\""; o += std::to_string(n.on_tap); o += "\""; }
    }
    static void esc(std::string& o, std::string_view s) {
        for (char c : s) switch (c) { case '&':o+="&amp;";break; case '<':o+="&lt;";break;
            case '>':o+="&gt;";break; default:o+=c; }
    }
};

// ── Backend B: Canvas draw-ops ──────────────────────────────────────────────
// Emits a flat list of drawing commands with absolute coordinates — exactly
// what a <canvas> 2D context replays. A trivial layout pass assigns positions
// (the point is to prove the SAME surface renders here too, not to be a full
// layout engine).
struct CanvasBackend {
    struct Op { std::string json; };
    std::string ops;   // JSON array of draw commands

    static std::string render(const Node& n, float w = 400, float h = 300) {
        CanvasBackend b; b.ops = "[";
        float y = 0;
        b.layout(n, 0, y, w);
        b.ops += "]";
        return b.ops;
    }

    // A minimal top-down flow layout: stack children vertically/horizontally,
    // emit a draw-op per node at its computed box. Enough to show canvas output.
    float layout(const Node& n, float x, float& y, float w) {
        float startY = y;
        float pad = n.paint.pad;
        float cx = x + pad, cy = y + pad;

        if (n.kind == Kind::box) {
            if (n.paint.bg & 0xff000000)
                rect(x, y, w, node_h(n), n.paint.bg, n.paint.radius);
            if (n.flow == Flow::row) {
                float childW = n.kids.empty() ? w : (w - 2*pad) / n.kids.size();
                float rx = cx;
                for (auto& k : n.kids) { float ky = cy; layout(*k, rx, ky, childW); rx += childW + n.paint.gap; }
                y = startY + node_h(n);
            } else {
                for (auto& k : n.kids) { layout(*k, cx, cy, w - 2*pad); cy += n.paint.gap; }
                y = cy + pad;
            }
        } else if (n.kind == Kind::text) {
            fill_text(n.text, cx, cy + n.paint.size, n.paint.fg, n.paint.size, n.paint.bold);
            y = startY + n.paint.size + 6;
        } else if (n.kind == Kind::image) {
            img(n.src, cx, cy, w - 2*pad, 100);
            y = startY + 100;
        } else if (n.kind == Kind::path) {
            poly(n.points, n.paint.fg, n.closed);
            y = startY + 60;
        }
        return y - startY;
    }

    float node_h(const Node& n) const {
        if (n.kind == Kind::text) return n.paint.size + 6;
        if (n.kind == Kind::image) return 100;
        float h = 2*n.paint.pad;
        if (n.flow == Flow::row) { float m=0; for (auto&k:n.kids) m=std::max(m,node_h(*k)); return h+m; }
        for (auto& k : n.kids) h += node_h(*k) + n.paint.gap;
        return h ? h : 24;
    }

    void comma() { if (ops.size() > 1) ops += ','; }
    void rect(float x,float y,float w,float h,uint32_t c,float r){ comma();
        ops += R"({"op":"rect","x":)"+detail::num(x)+R"(,"y":)"+detail::num(y)+
               R"(,"w":)"+detail::num(w)+R"(,"h":)"+detail::num(h)+
               R"(,"r":)"+detail::num(r)+R"(,"c":")"; detail::hex(ops,c); ops+="\"}"; }
    void fill_text(const std::string& s,float x,float y,uint32_t c,float sz,bool b){ comma();
        ops += R"({"op":"text","x":)"+detail::num(x)+R"(,"y":)"+detail::num(y)+
               R"(,"s":")"+esc(s)+R"(","c":")"; detail::hex(ops,c);
        ops += R"(","sz":)"+detail::num(sz)+R"(,"b":)"+(b?"true":"false")+"}"; }
    void img(const std::string& src,float x,float y,float w,float h){ comma();
        ops += R"({"op":"img","src":")"+esc(src)+R"(","x":)"+detail::num(x)+
               R"(,"y":)"+detail::num(y)+R"(,"w":)"+detail::num(w)+R"(,"h":)"+detail::num(h)+"}"; }
    void poly(const std::vector<std::pair<float,float>>& p,uint32_t c,bool closed){ comma();
        ops += R"({"op":"poly","c":")"; detail::hex(ops,c);
        ops += R"(","closed":)"; ops += closed?"true":"false"; ops += R"(,"pts":[)";
        for (size_t i=0;i<p.size();++i){ if(i)ops+=','; ops+="["+detail::num(p[i].first)+","+detail::num(p[i].second)+"]"; }
        ops += "]}"; }
    static std::string esc(const std::string& s){ std::string o; for(char c:s){ if(c=='"'||c=='\\')o+='\\'; o+=c; } return o; }
};

} // namespace waya::surface
