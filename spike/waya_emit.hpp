#pragma once
// waya::style::emit — the renderer OWNS style output (maya-style).
//
// maya walks the tree and emits ANSI/SGR. waya walks the tree and emits an
// interned atomic class per node plus ONE generated stylesheet. The DSL author
// never writes or sees CSS; it is purely waya's output encoding.
//
// Two jobs, both proven here:
//   A. Sty  -> CSS declaration block   (the "SGR serialiser")
//   B. a StyleSheet that INTERNS identical Stys to one class name and
//      deduplicates, so `pad(px(16))` used 500 times is ONE rule.

#include "waya_style.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace waya {

// ── A: serialise one Sty into CSS declarations ──────────────────────────────
// This is an implementation detail. Users never call it. Analogous to maya's
// Style::to_sgr().

inline void len_into(std::string& o, Len l) {
    auto num = [&](float v) {
        // compact float formatting (spike-grade)
        if (v == (long long)v) { o += std::to_string((long long)v); return; }
        std::string s = std::to_string(v);
        while (s.size() && s.back() == '0') s.pop_back();
        if (s.size() && s.back() == '.') s.pop_back();
        o += s;
    };
    switch (l.unit) {
        case Unit::px:    num(l.value); o += "px";  break;
        case Unit::rem:   num(l.value); o += "rem"; break;
        case Unit::pct:   num(l.value); o += "%";   break;
        case Unit::fr:    num(l.value); o += "fr";  break;
        case Unit::auto_: o += "auto"; break;
        case Unit::zero:  o += "0";    break;
    }
}

inline void hex_into(std::string& o, uint32_t c) {
    static const char* H = "0123456789abcdef";
    o += '#';
    for (int shift = 20; shift >= 0; shift -= 4) o += H[(c >> shift) & 0xF];
}

// Emit the CSS body (declarations only, no selector) for a Sty.
inline std::string declarations(const Sty& s) {
    std::string o;
    auto kv = [&](std::string_view k, auto emit) {
        o += k; o += ':'; emit(); o += ';';
    };

    switch (s.display) {
        case Disp::Flex:   o += "display:flex;";  break;
        case Disp::Grid:   o += "display:grid;";  break;
        case Disp::Block:  o += "display:block;"; break;
        case Disp::Inline: o += "display:inline;";break;
        case Disp::None:   o += "display:none;";  break;
        case Disp::Default: break;
    }
    if (s.direction == Dir::Row)    o += "flex-direction:row;";
    if (s.direction == Dir::Col)    o += "flex-direction:column;";
    if (s.direction == Dir::RowRev) o += "flex-direction:row-reverse;";
    if (s.direction == Dir::ColRev) o += "flex-direction:column-reverse;";

    if (s.has_fg)  kv("color",           [&]{ hex_into(o, s.fg); });
    if (s.has_bg)  kv("background",       [&]{ hex_into(o, s.bg); });
    if (s.has_pad) kv("padding",          [&]{ len_into(o, s.pad); });
    if (s.has_gap) kv("gap",              [&]{ len_into(o, s.gap); });
    if (s.has_w)   kv("width",            [&]{ len_into(o, s.w); });
    if (s.has_h)   kv("height",           [&]{ len_into(o, s.h); });
    if (s.has_radius) kv("border-radius", [&]{ len_into(o, s.radius); });
    if (s.has_size)   kv("font-size",     [&]{ len_into(o, s.size); });
    if (s.has_grow)   kv("flex-grow",     [&]{ o += std::to_string(s.grow); });

    switch (s.justify) {
        case Justify::Start:   o += "justify-content:flex-start;"; break;
        case Justify::Center:  o += "justify-content:center;"; break;
        case Justify::End:     o += "justify-content:flex-end;"; break;
        case Justify::Between:  o += "justify-content:space-between;"; break;
        case Justify::Around:  o += "justify-content:space-around;"; break;
        case Justify::Evenly:  o += "justify-content:space-evenly;"; break;
        case Justify::None: break;
    }
    switch (s.align) {
        case Align::Start:    o += "align-items:flex-start;"; break;
        case Align::Center:   o += "align-items:center;"; break;
        case Align::End:      o += "align-items:flex-end;"; break;
        case Align::Stretch:  o += "align-items:stretch;"; break;
        case Align::Baseline: o += "align-items:baseline;"; break;
        case Align::None: break;
    }
    switch (s.weight) {
        case Weight::Normal:   o += "font-weight:400;"; break;
        case Weight::Medium:   o += "font-weight:500;"; break;
        case Weight::Semibold: o += "font-weight:600;"; break;
        case Weight::Bold:     o += "font-weight:700;"; break;
        case Weight::None: break;
    }
    if (s.italic)     o += "font-style:italic;";
    if (s.underline)  o += "text-decoration:underline;";
    if (s.has_shadow) o += "box-shadow:0 1px 3px rgba(0,0,0,.2);";
    if (s.has_opacity) {
        o += "opacity:"; o += std::to_string(s.opacity_pct / 100.0).substr(0,4); o += ';';
    }
    return o;
}

// ── B: the interning stylesheet (maya's StylePool analogue) ─────────────────
//
// Every unique Sty reachable from view() maps to ONE class. Identical Stys
// (structurally equal) collapse to the same class — that's the dedup that
// makes this atomic-CSS-grade rather than inline-style-grade.

struct StyleSheet {
    std::vector<Sty>          styles;   // interned, unique
    std::vector<std::string>  names;    // parallel: class name per style

    // Intern a style, returning its stable class name. Structural equality
    // means the SAME visual style anywhere in the tree shares one class.
    std::string_view intern(const Sty& s) {
        for (std::size_t i = 0; i < styles.size(); ++i)
            if (styles[i] == s) return names[i];
        // content-ish hash for a short, stable name
        uint32_t h = 2166136261u;
        std::string decls = declarations(s);
        for (char c : decls) { h ^= (uint8_t)c; h *= 16777619u; }
        std::string name = "wa-";
        static const char* H = "0123456789abcdef";
        for (int shift = 20; shift >= 0; shift -= 4) name += H[(h >> shift) & 0xF];
        styles.push_back(s);
        names.push_back(name);
        return names.back();
    }

    // The one <style> blob for the whole page.
    std::string render() const {
        std::string o;
        for (std::size_t i = 0; i < styles.size(); ++i) {
            std::string d = declarations(styles[i]);
            if (d.empty()) continue;
            o += '.'; o += names[i]; o += '{'; o += d; o += '}';
        }
        return o;
    }
};

} // namespace waya
