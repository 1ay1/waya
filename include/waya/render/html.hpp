#pragma once
/// \file html.hpp
/// Walk a compile-time DSL tree and emit HTML + the interned stylesheet.
///
/// This is waya's renderer, the analogue of maya's canvas→ANSI serialiser.
/// It threads a `StyleSheet` through the walk so that every node's `Sty`
/// becomes an interned atomic class — no inline styles, one deduplicated
/// stylesheet for the whole page.

#include "escape.hpp"
#include "../dsl/node.hpp"
#include "../dsl/dynamic.hpp"
#include "../style/css.hpp"

#include <string>
#include <tuple>

namespace waya::render {

/// The result of rendering a tree: HTML body plus the CSS it needs.
struct Rendered {
    std::string html;
    std::string css;
};

namespace detail {

using dsl::ElemNode;
using dsl::TextNode;

inline void walk(std::string& out, style::StyleSheet& sheet, const TextNode& t) {
    escape_text(out, t.value);
}

inline void walk(std::string& out, style::StyleSheet&, const dsl::RawHtml& r) {
    out += r.html;   // trusted, unescaped by contract
}

template <html::Cat C>
void walk(std::string& out, style::StyleSheet& sheet, const dsl::Frag<C>& f) {
    for (const auto& part : f.parts) part(out, sheet);
}

template <html::Tag T, dsl::ElemCfg Cfg, typename... Cs>
void walk(std::string& out, style::StyleSheet& sheet, const ElemNode<T, Cfg, Cs...>& e) {
    constexpr std::string_view name = html::Traits<T>::name;
    out += '<'; out += name;

    if constexpr (Cfg.has_id)   { out += " id=\"";   escape_attr(out, e.attrs.id);   out += '"'; }

    // class attribute = user class (if any) + the interned style class (if any)
    std::string_view style_cls = sheet.intern(e.style);
    const bool has_user_cls = Cfg.has_cls;
    if (has_user_cls || !style_cls.empty()) {
        out += " class=\"";
        if constexpr (Cfg.has_cls) escape_attr(out, e.attrs.cls);
        if (has_user_cls && !style_cls.empty()) out += ' ';
        out += style_cls;
        out += '"';
    }

    if constexpr (Cfg.has_href) { out += " href=\""; escape_attr(out, e.attrs.href); out += '"'; }

    out += '>';
    if constexpr (!html::Traits<T>::is_void) {
        std::apply([&](const auto&... cs) { (walk(out, sheet, cs), ...); }, e.children);
        out += "</"; out += name; out += '>';
    }
}

} // namespace detail
} // namespace waya::render

// Satisfy the forward declaration in dsl/dynamic.hpp: type-erased fragments
// render their captured nodes by calling back into the renderer's walk.
namespace waya::dsl::detail {
template <typename N>
void render_child(std::string& out, style::StyleSheet& sheet, const N& n) {
    waya::render::detail::walk(out, sheet, n);
}
} // namespace waya::dsl::detail

namespace waya::render {

template <typename Node>
[[nodiscard]] Rendered render(const Node& node) {
    style::StyleSheet sheet;
    std::string html;
    detail::walk(html, sheet, node);
    return { std::move(html), sheet.render() };
}

/// Render a full document: `<!DOCTYPE html>` + the tree, with the generated
/// stylesheet injected into <head> as a single <style>. The author's tree is
/// expected to be rooted at <html>.
template <typename Node>
[[nodiscard]] std::string render_document(const Node& node) {
    style::StyleSheet sheet;
    std::string body;
    detail::walk(body, sheet, node);

    std::string css = sheet.render();
    std::string doc = "<!DOCTYPE html>";
    if (css.empty()) { doc += body; return doc; }

    // Inject the stylesheet right after <head> if present, else prepend a
    // <style> before the body. Phase 1: simple, correct-for-the-common-case.
    constexpr std::string_view head_open = "<head>";
    if (auto pos = body.find(head_open); pos != std::string::npos) {
        pos += head_open.size();
        body.insert(pos, "<style>" + css + "</style>");
    } else {
        doc += "<style>" + css + "</style>";
    }
    doc += body;
    return doc;
}

} // namespace waya::render
