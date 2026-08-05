#pragma once
/// \file dom.hpp
/// The DOM rendering backend for the Surface Model.
///
/// Turns a surface node tree into HTML with interned CSS classes. This is ONE
/// of waya's backends; the user's `view()` never knows it exists. Good for
/// forms, flowing text, and accessibility.

#include "node.hpp"
#include "../core/hash.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace waya::surface {

/// Renders a surface to HTML + a deduplicated stylesheet. Paints are interned to
/// atomic classes (same trick as the DOM DSL): repeated styles share one rule.
class DomBackend {
public:
    struct Output { std::string html; std::string css; };

    Output render(const Node& root) {
        styles_.clear(); names_.clear();
        std::string html;
        emit(html, root);
        std::string css;
        for (std::size_t i = 0; i < styles_.size(); ++i) {
            css += '.'; css += names_[i]; css += '{'; css += styles_[i]; css += '}';
        }
        return { std::move(html), std::move(css) };
    }

private:
    std::vector<std::string> styles_, names_;

    static void hex(std::string& o, std::uint32_t c) {
        static const char* H = "0123456789abcdef"; o += '#';
        for (int s = 20; s >= 0; s -= 4) o += H[(c >> s) & 0xF];
    }
    static std::string num(float f) {
        long long i = (long long)f;
        if ((float)i == f) return std::to_string(i);
        std::string s = std::to_string(f);
        while (s.size() && s.back()=='0') s.pop_back();
        if (s.size() && s.back()=='.') s.pop_back();
        return s;
    }
    static void esc(std::string& o, std::string_view s) {
        for (char c : s) switch (c) { case '&':o+="&amp;";break; case '<':o+="&lt;";break;
            case '>':o+="&gt;";break; default:o+=c; }
    }

    /// CSS declarations for a node's paint + flow.
    std::string decls(const Node& n) {
        std::string o;
        switch (n.flow) {
            case Flow::row:   o += "display:flex;flex-direction:row;"; break;
            case Flow::col:   o += "display:flex;flex-direction:column;"; break;
            case Flow::stack: o += "display:grid;"; break;
            case Flow::none:  break;
        }
        if (n.kind == Kind::text) {
            o += "color:"; hex(o, n.paint.fg); o += ';';
            o += "font-size:"; o += num(n.paint.size); o += "px;";
            if (n.paint.bold) o += "font-weight:700;";
        }
        if (n.paint.has_bg) { o += "background:"; hex(o, n.paint.bg); o += ';'; }
        if (n.paint.radius) { o += "border-radius:"; o += num(n.paint.radius); o += "px;"; }
        if (n.paint.pad)    { o += "padding:"; o += num(n.paint.pad); o += "px;"; }
        if (n.paint.gap)    { o += "gap:"; o += num(n.paint.gap); o += "px;"; }
        if (n.paint.grow)   { o += "flex:"; o += num(n.paint.grow); o += " 1 0;"; }
        if (n.on_tap >= 0)  o += "cursor:pointer;";
        return o;
    }

    std::string_view intern(std::string d) {
        if (d.empty()) return {};
        for (std::size_t i = 0; i < styles_.size(); ++i) if (styles_[i] == d) return names_[i];
        std::string name = "ws-"; hex8(name, fnv1a(d));
        styles_.push_back(std::move(d)); names_.push_back(std::move(name));
        return names_.back();
    }

    void open_attrs(std::string& o, const Node& n) {
        auto cls = intern(decls(n));
        if (!cls.empty()) { o += " class=\""; o += cls; o += '"'; }
        if (n.on_tap >= 0) { o += " data-tap=\""; o += std::to_string(n.on_tap); o += '"'; }
    }

    void emit(std::string& o, const Node& n) {
        switch (n.kind) {
            case Kind::text:
                o += "<span"; open_attrs(o, n); o += '>'; esc(o, n.text); o += "</span>";
                return;
            case Kind::image:
                o += "<img src=\""; esc(o, n.src); o += '"'; open_attrs(o, n); o += '>';
                return;
            case Kind::path: {
                o += "<svg"; open_attrs(o, n); o += "><polyline points=\"";
                for (auto& p : n.points) { o += num(p.x); o += ','; o += num(p.y); o += ' '; }
                o += "\" fill=\""; if (n.closed) hex(o, n.paint.bg); else o += "none";
                o += "\" stroke=\""; hex(o, n.paint.fg);
                o += "\" stroke-width=\""; o += num(n.paint.size ? n.paint.size/8 : 2);
                o += "\"/></svg>";
                return;
            }
            case Kind::box:
                o += "<div"; open_attrs(o, n); o += '>';
                for (auto& k : n.kids) emit(o, *k);
                o += "</div>";
                return;
        }
    }
};

} // namespace waya::surface
