#pragma once
/// \file css.hpp
/// Serialise `Sty` into CSS, and intern identical styles into atomic classes.
///
/// This is entirely an implementation detail — CSS is waya's private output
/// format, the equivalent of maya's ANSI/SGR serialiser. Users never call this.

#include "sty.hpp"
#include "../core/hash.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace waya::style {

namespace detail {

inline void num(std::string& o, float v) {
    if (v == static_cast<long long>(v)) { o += std::to_string(static_cast<long long>(v)); return; }
    std::string s = std::to_string(v);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    o += s;
}

inline void len(std::string& o, Len l) {
    switch (l.unit) {
        case Unit::zero:  o += "0";  return;
        case Unit::auto_: o += "auto"; return;
        case Unit::px:  num(o, l.value); o += "px";  return;
        case Unit::rem: num(o, l.value); o += "rem"; return;
        case Unit::em:  num(o, l.value); o += "em";  return;
        case Unit::pct: num(o, l.value); o += "%";   return;
        case Unit::vw:  num(o, l.value); o += "vw";  return;
        case Unit::vh:  num(o, l.value); o += "vh";  return;
        case Unit::fr:  num(o, l.value); o += "fr";  return;
        case Unit::ch:  num(o, l.value); o += "ch";  return;
    }
}

inline void hex(std::string& o, std::uint32_t c) {
    static constexpr char H[] = "0123456789abcdef";
    o += '#';
    for (int sh = 20; sh >= 0; sh -= 4) o += H[(c >> sh) & 0xF];
}

} // namespace detail

/// The declaration block (no selector, no braces) for a style. Order is stable
/// so equal styles hash equal.
inline std::string declarations(const Sty& s) {
    std::string o;
    using namespace detail;
    auto prop = [&](std::string_view k, auto emit) { o += k; o += ':'; emit(); o += ';'; };

    switch (s.display) {
        case Display::block:        o += "display:block;"; break;
        case Display::inline_:      o += "display:inline;"; break;
        case Display::inline_block: o += "display:inline-block;"; break;
        case Display::flex:         o += "display:flex;"; break;
        case Display::grid:         o += "display:grid;"; break;
        case Display::none:         o += "display:none;"; break;
        case Display::unset: break;
    }
    switch (s.position) {
        case Position::static_:  o += "position:static;"; break;
        case Position::relative: o += "position:relative;"; break;
        case Position::absolute: o += "position:absolute;"; break;
        case Position::fixed:    o += "position:fixed;"; break;
        case Position::sticky:   o += "position:sticky;"; break;
        case Position::unset: break;
    }
    switch (s.direction) {
        case Dir::row:     o += "flex-direction:row;"; break;
        case Dir::col:     o += "flex-direction:column;"; break;
        case Dir::row_rev: o += "flex-direction:row-reverse;"; break;
        case Dir::col_rev: o += "flex-direction:column-reverse;"; break;
        case Dir::unset: break;
    }
    switch (s.wrap) {
        case Wrap::nowrap:   o += "flex-wrap:nowrap;"; break;
        case Wrap::wrap:     o += "flex-wrap:wrap;"; break;
        case Wrap::wrap_rev: o += "flex-wrap:wrap-reverse;"; break;
        case Wrap::unset: break;
    }

    if (s.has_fg)     prop("color",         [&]{ hex(o, s.fg); });
    if (s.has_bg)     prop("background",     [&]{ hex(o, s.bg); });
    if (s.has_pad)    prop("padding",        [&]{ len(o, s.pad); });
    if (s.has_pad_x) { prop("padding-left",  [&]{ len(o, s.pad_x); });
                       prop("padding-right", [&]{ len(o, s.pad_x); }); }
    if (s.has_pad_y) { prop("padding-top",   [&]{ len(o, s.pad_y); });
                       prop("padding-bottom",[&]{ len(o, s.pad_y); }); }
    if (s.has_margin) prop("margin",         [&]{ len(o, s.margin); });
    if (s.has_w)      prop("width",          [&]{ len(o, s.w); });
    if (s.has_h)      prop("height",         [&]{ len(o, s.h); });
    if (s.has_max_w)  prop("max-width",      [&]{ len(o, s.max_w); });
    if (s.has_min_w)  prop("min-width",      [&]{ len(o, s.min_w); });
    if (s.has_radius) prop("border-radius",  [&]{ len(o, s.radius); });
    if (s.has_border_w) { prop("border-width",[&]{ len(o, s.border_w); }); o += "border-style:solid;"; }
    if (s.has_border_c) prop("border-color", [&]{ hex(o, s.border_c); });
    if (s.has_gap)    prop("gap",            [&]{ len(o, s.gap); });
    if (s.has_grow)   prop("flex-grow",      [&]{ o += std::to_string(s.grow); });
    if (s.has_shrink) prop("flex-shrink",    [&]{ o += std::to_string(s.shrink); });
    if (s.has_size)   prop("font-size",      [&]{ len(o, s.size); });
    if (s.has_lh)     prop("line-height",    [&]{ len(o, s.line_height); });

    switch (s.justify) {
        case Justify::start:   o += "justify-content:flex-start;"; break;
        case Justify::center:  o += "justify-content:center;"; break;
        case Justify::end:     o += "justify-content:flex-end;"; break;
        case Justify::between: o += "justify-content:space-between;"; break;
        case Justify::around:  o += "justify-content:space-around;"; break;
        case Justify::evenly:  o += "justify-content:space-evenly;"; break;
        case Justify::unset: break;
    }
    switch (s.align) {
        case Align::start:    o += "align-items:flex-start;"; break;
        case Align::center:   o += "align-items:center;"; break;
        case Align::end:      o += "align-items:flex-end;"; break;
        case Align::stretch:  o += "align-items:stretch;"; break;
        case Align::baseline: o += "align-items:baseline;"; break;
        case Align::unset: break;
    }
    switch (s.weight) {
        case Weight::w100: o+="font-weight:100;"; break; case Weight::w200: o+="font-weight:200;"; break;
        case Weight::w300: o+="font-weight:300;"; break; case Weight::normal: o+="font-weight:400;"; break;
        case Weight::w500: o+="font-weight:500;"; break; case Weight::w600: o+="font-weight:600;"; break;
        case Weight::bold: o+="font-weight:700;"; break; case Weight::w800: o+="font-weight:800;"; break;
        case Weight::w900: o+="font-weight:900;"; break; case Weight::unset: break;
    }
    if (s.italic)    o += "font-style:italic;";
    if (s.underline) o += "text-decoration:underline;";
    switch (s.text_align) {
        case TextAlign::left:    o += "text-align:left;"; break;
        case TextAlign::center:  o += "text-align:center;"; break;
        case TextAlign::right:   o += "text-align:right;"; break;
        case TextAlign::justify: o += "text-align:justify;"; break;
        case TextAlign::unset: break;
    }
    if (s.has_shadow)  o += "box-shadow:0 1px 3px rgba(0,0,0,.2);";
    if (s.has_opacity) { o += "opacity:"; detail::num(o, s.opacity_pct / 100.0f); o += ';'; }
    switch (s.cursor) {
        case Cursor::pointer:     o += "cursor:pointer;"; break;
        case Cursor::default_:    o += "cursor:default;"; break;
        case Cursor::text:        o += "cursor:text;"; break;
        case Cursor::wait:        o += "cursor:wait;"; break;
        case Cursor::not_allowed: o += "cursor:not-allowed;"; break;
        case Cursor::unset: break;
    }
    return o;
}

/// The interning stylesheet — maya's `StylePool`, one level up. Every unique
/// `Sty` reachable from `view()` maps to ONE class; identical styles collapse.
class StyleSheet {
public:
    /// Returns the class name for a style (interning it if new). Empty styles
    /// return "" — no class is emitted.
    std::string_view intern(const Sty& s) {
        if (s.empty()) return {};
        for (std::size_t i = 0; i < styles_.size(); ++i)
            if (styles_[i] == s) return names_[i];
        std::string decls = declarations(s);
        std::string name = "wa-";
        hex8(name, fnv1a(decls));
        styles_.push_back(s);
        decls_.push_back(std::move(decls));
        names_.push_back(std::move(name));
        return names_.back();
    }

    /// The single <style> body for the whole page. Deduplicated by construction.
    [[nodiscard]] std::string render() const {
        std::string o;
        for (std::size_t i = 0; i < styles_.size(); ++i) {
            if (decls_[i].empty()) continue;
            o += '.'; o += names_[i]; o += '{'; o += decls_[i]; o += '}';
        }
        return o;
    }

    [[nodiscard]] std::size_t rule_count() const { return styles_.size(); }

private:
    std::vector<Sty>         styles_;
    std::vector<std::string> decls_;
    std::vector<std::string> names_;
};

} // namespace waya::style
