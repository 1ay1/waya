#pragma once
/// \file ui/scene.hpp
/// A vector-drawing vocabulary — the missing "canvas" of the surface language.
///
/// waya's promise is that you never write HTML/CSS/SVG by hand. Everywhere
/// EXCEPT drawing that held: a chart, a sprite, a game board, an animated
/// backdrop meant reaching for `markup("<svg>…")` and concatenating element
/// strings with manual `&lt;` escaping — the one place the framework's own
/// examples still wrote raw markup, error-prone and unsafe.
///
/// `scene(...)` closes that. Shapes are plain VALUES, painted with a fluent
/// chain and composed like any node:
///
///   scene(400, 200,
///       vrect(0, 0, 400, 200).fill(0x0b1020),
///       vline(0, 100, 400, 100).stroke(0x22d3ee, 2),
///       vcircle(200, 100, 40).fill(rgba(0x6366f1, .8f)),
///       vtext(200, 105, "hi").fill(0xffffff).anchor_mid())
///
/// The scene renders to ONE <svg> through the DOM backend — text is escaped,
/// numbers are formatted once, and the whole thing diffs like any subtree (a
/// changed circle emits a set_inner on the scene, not a full-page repaint).
/// It's the same "describe a surface, we render it" model, extended to pixels.
///
/// Paint is a FLUENT method chain (`.fill(…).stroke(…)`), not `| mod` — so the
/// natural words fill/stroke/opacity/bold/mono never collide with the box-style
/// mods of the same name in `waya::surface`. The scene node itself still takes
/// the normal box mods: `scene(…) | w_full | h(240) | fg(0x22d3ee)` (the SVG
/// inherits `currentColor`, so `fg()` recolours every `currentColor` shape).
///
/// WHY SVG, NOT <canvas>: SVG is declarative (a function of your data, like the
/// rest of waya) and lands in the existing innerHTML/diff pipeline with zero
/// client JS. For 60fps-per-pixel work a <canvas> driver would be a different
/// backend; for charts, sprites, diagrams, boards, and ambient art — the 95%
/// case — a diffed SVG scene is exactly right.

#include "../surface/node.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

namespace scene_detail {
inline std::string num(float f){
    // compact, locale-independent (avoid std::to_string's trailing zeros)
    if (f == (long long)f) return std::to_string((long long)f);
    char b[32]; std::snprintf(b, sizeof b, "%.3f", f);
    std::string s(b); while(s.size()>1 && s.back()=='0') s.pop_back();
    if(!s.empty() && s.back()=='.') s.pop_back(); return s;
}
}

// ── a Shape: one SVG element as a value, painted by a fluent chain ───────────
struct Shape {
    std::string tag;                                            // "rect","circle",…
    std::vector<std::pair<std::string,std::string>> attrs;      // geometry (x,y,r,…)
    std::string body;                                           // vtext content (escaped on emit)
    // paint (SVG presentation attributes; empty = inherit/none)
    std::string fill_ = "currentColor", stroke_;
    float stroke_w = 0, fill_op = 1, stroke_op = 1, op = 1;
    std::string extra;                                          // pre-built extra attrs (linecap, dash…)
    std::vector<Shape> kids;                                    // group children (for vgroup)

    // ── fluent paint API — reads left-to-right, no name collisions ──────────
    Shape& fill(std::uint32_t hex){ fill_ = rgb(hex).css(); return *this; }
    Shape& fill(Color c){ fill_ = c.css(); return *this; }
    /// outline-only (paired with .stroke()).
    Shape& no_fill(){ fill_ = "none"; return *this; }
    Shape& stroke(std::uint32_t hex, float w=1){ stroke_ = rgb(hex).css(); stroke_w = w; return *this; }
    Shape& stroke(Color c, float w=1){ stroke_ = c.css(); stroke_w = w; return *this; }
    Shape& opacity(float a){ op = a; return *this; }
    Shape& fill_opacity(float a){ fill_op = a; return *this; }
    Shape& stroke_opacity(float a){ stroke_op = a; return *this; }
    /// round line caps/joins — for polylines and open paths.
    Shape& round_cap(){ extra += " stroke-linecap=\"round\" stroke-linejoin=\"round\""; return *this; }
    Shape& dashed(float on=4, float off=4){ extra += " stroke-dasharray=\"" + scene_detail::num(on) + " " + scene_detail::num(off) + "\""; return *this; }
    /// text placement + face (vtext only).
    Shape& anchor_mid(){ extra += " text-anchor=\"middle\""; return *this; }
    Shape& anchor_end(){ extra += " text-anchor=\"end\""; return *this; }
    Shape& font_px(float px){ extra += " font-size=\"" + scene_detail::num(px) + "\""; return *this; }
    Shape& bold(){ extra += " font-weight=\"700\""; return *this; }
    Shape& mono(){ extra += " font-family=\"ui-monospace,Menlo,Consolas,monospace\""; return *this; }
    /// a raw SVG transform for the rare rotate/skew case.
    Shape& transform(std::string spec){ extra += " transform=\"" + spec + "\""; return *this; }
};

namespace scene_detail {
inline void esc(std::string& o, std::string_view s){
    for(char c : s) switch(c){ case '<':o+="&lt;";break; case '>':o+="&gt;";break;
        case '&':o+="&amp;";break; case '"':o+="&quot;";break; default:o+=c; }
}
inline void attr(std::string& o, const char* k, const std::string& v){
    o+=' '; o+=k; o+="=\""; esc(o,v); o+='"';
}
/// Emit one shape (and its children) as an SVG element string.
inline void emit(std::string& o, const Shape& s){
    o+='<'; o+=s.tag;
    for(auto& [k,v] : s.attrs) attr(o, k.c_str(), v);
    if(!s.fill_.empty())   attr(o, "fill", s.fill_);
    if(!s.stroke_.empty()) attr(o, "stroke", s.stroke_);
    if(s.stroke_w>0)      attr(o, "stroke-width", num(s.stroke_w));
    if(s.fill_op<1)       attr(o, "fill-opacity", num(s.fill_op));
    if(s.stroke_op<1)     attr(o, "stroke-opacity", num(s.stroke_op));
    if(s.op<1)            attr(o, "opacity", num(s.op));
    o += s.extra;
    if(s.tag=="g" || !s.body.empty()){
        o+='>';
        if(!s.body.empty()) esc(o, s.body);
        for(auto& k : s.kids) emit(o, k);
        o+="</"; o+=s.tag; o+='>';
    } else {
        o+="/>";
    }
}
inline Shape mk(std::string tag){ Shape s; s.tag = std::move(tag); return s; }
inline void set(Shape& s, const char* k, float v){ s.attrs.emplace_back(k, num(v)); }
} // namespace scene_detail

// ── Shape builders (geometry) ───────────────────────────────────────────────
/// `vrect(x, y, w, h, r=0)` — a rectangle (r = corner radius).
inline Shape vrect(float x, float y, float w, float h, float r=0){
    auto s = scene_detail::mk("rect");
    scene_detail::set(s,"x",x); scene_detail::set(s,"y",y);
    scene_detail::set(s,"width",w); scene_detail::set(s,"height",h);
    if(r>0){ scene_detail::set(s,"rx",r); scene_detail::set(s,"ry",r); }
    return s;
}
/// `vcircle(cx, cy, r)`.
inline Shape vcircle(float cx, float cy, float r){
    auto s = scene_detail::mk("circle");
    scene_detail::set(s,"cx",cx); scene_detail::set(s,"cy",cy); scene_detail::set(s,"r",r);
    return s;
}
/// `vellipse(cx, cy, rx, ry)`.
inline Shape vellipse(float cx, float cy, float rx, float ry){
    auto s = scene_detail::mk("ellipse");
    scene_detail::set(s,"cx",cx); scene_detail::set(s,"cy",cy);
    scene_detail::set(s,"rx",rx); scene_detail::set(s,"ry",ry);
    return s;
}
/// `vline(x1, y1, x2, y2)` — a straight segment (give it a `.stroke(…)`).
inline Shape vline(float x1, float y1, float x2, float y2){
    auto s = scene_detail::mk("line"); s.fill_ = "";   // lines don't fill
    scene_detail::set(s,"x1",x1); scene_detail::set(s,"y1",y1);
    scene_detail::set(s,"x2",x2); scene_detail::set(s,"y2",y2);
    return s;
}
/// `vpolyline(points)` / `vpolygon(points)` — an open / closed multi-segment
/// shape. A polyline strokes; a polygon also fills.
inline Shape vpolyline(const std::vector<Pt>& pts){
    auto s = scene_detail::mk("polyline"); s.fill_ = "none";
    std::string p; for(auto& q : pts){ p += scene_detail::num(q.x); p += ','; p += scene_detail::num(q.y); p += ' '; }
    s.attrs.emplace_back("points", std::move(p));
    return s;
}
inline Shape vpolygon(const std::vector<Pt>& pts){
    auto s = scene_detail::mk("polygon");
    std::string p; for(auto& q : pts){ p += scene_detail::num(q.x); p += ','; p += scene_detail::num(q.y); p += ' '; }
    s.attrs.emplace_back("points", std::move(p));
    return s;
}
/// `vpath(d)` — an arbitrary SVG path-data string, for curves the builders don't
/// cover. Still a Shape (fills/strokes/chains) — just with a hand-written `d`.
inline Shape vpath(std::string d){
    auto s = scene_detail::mk("path"); s.fill_ = "none";
    s.attrs.emplace_back("d", std::move(d));
    return s;
}
/// `vtext(x, y, "…")` — a text label; content is ESCAPED. Size/place with the
/// chain (`.font_px().anchor_mid().bold().mono()`). fills like a shape.
inline Shape vtext(float x, float y, std::string text){
    auto s = scene_detail::mk("text");
    scene_detail::set(s,"x",x); scene_detail::set(s,"y",y);
    s.body = std::move(text);
    return s;
}
/// `vgroup(shapes…)` — group shapes so one `.transform()`/`.opacity()` applies
/// to all of them.
template <typename... S>
inline Shape vgroup(S... shapes){
    auto s = scene_detail::mk("g"); s.fill_ = "";
    (s.kids.push_back(std::move(shapes)), ...);
    return s;
}
inline Shape vgroup(std::vector<Shape> shapes){
    auto s = scene_detail::mk("g"); s.fill_ = ""; s.kids = std::move(shapes); return s;
}

// ── scene(): shapes → one diffable node ─────────────────────────────────────
namespace scene_detail {
inline NodeRef finish(float vbw, float vbh, std::vector<Shape> shapes){
    std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
        + num(vbw) + " " + num(vbh) + "\" width=\"100%\" height=\"100%\""
        " preserveAspectRatio=\"none\" style=\"display:block\">";
    for(auto& s : shapes) emit(svg, s);
    svg += "</svg>";
    // markup carries the SVG as innerHTML — the animation-safe channel (a
    // changed scene emits set_inner, not a stale set_paint), and diffs normally.
    return markup(std::move(svg));
}
}
/// `scene(w, h, shapes…)` — a vector drawing. `w`/`h` define the coordinate
/// space (the viewBox); the node scales to fill whatever box you size it into
/// (`| w(N) | h(N)` or `| w_full | h(240)`). Recolour every `currentColor`
/// shape at once with `| fg(hex)`. Everything inside is a Shape.
template <typename... S>
inline NodeRef scene(float w_, float h_, S... shapes){
    std::vector<Shape> v; (v.push_back(std::move(shapes)), ...);
    return scene_detail::finish(w_, h_, std::move(v)) | w(w_) | h(h_);
}
inline NodeRef scene(float w_, float h_, std::vector<Shape> shapes){
    return scene_detail::finish(w_, h_, std::move(shapes)) | w(w_) | h(h_);
}

} // namespace waya::ui
