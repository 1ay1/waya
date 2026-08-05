#pragma once
/// \file vwalk.hpp
/// Walk a DSL tree into a `VNode` (the diffable snapshot) — the parallel of the
/// HTML renderer's walk, producing the virtual-DOM the diff engine compares.
///
/// The class attribute merges the user class and the interned style class, so a
/// style change shows up as an attribute delta and diffs like anything else.
/// Attributes are sorted by name so two VNodes compare deterministically.

#include "vdom.hpp"
#include "escape.hpp"
#include "../dsl/node.hpp"
#include "../dsl/dynamic.hpp"
#include "../style/css.hpp"

#include <algorithm>
#include <string>
#include <tuple>

namespace waya::render {

namespace detail {

using dsl::ElemNode;
using dsl::TextNode;

inline void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const TextNode& t);
inline void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const dsl::RawHtml& r);
template <html::Cat C>
void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const dsl::Frag<C>& f);
template <html::Tag T, dsl::ElemCfg Cfg, typename... Cs>
void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const ElemNode<T, Cfg, Cs...>& e);

inline void vbuild(vdom::VNode& parent, style::StyleSheet&, const TextNode& t) {
    parent.kids.push_back(vdom::VNode::textnode(t.value));
}

inline void vbuild(vdom::VNode& parent, style::StyleSheet&, const dsl::RawHtml& r) {
    // Raw HTML isn't diffable structurally; treat it as an opaque text-ish leaf.
    auto n = vdom::VNode::textnode(r.html);
    n.tag = "\x01raw";   // marker so vnode_to_html emits it unescaped (handled below)
    parent.kids.push_back(std::move(n));
}

template <html::Cat C>
void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const dsl::Frag<C>& f) {
    // A fragment splices its produced nodes directly into the parent — but the
    // type-erased parts render to HTML, so we can't introspect them into VNodes
    // without a second erased hook. For Phase-3 correctness we wrap the whole
    // fragment as one opaque node keyed by its rendered HTML; the diff will
    // replace it if it changes. (A finer per-item diff for `each` is a later
    // optimisation.)
    std::string html;
    for (const auto& part : f.parts) part(html, sheet);
    auto n = vdom::VNode::textnode(html);
    n.tag = "\x01raw";
    parent.kids.push_back(std::move(n));
}

template <html::Tag T, dsl::ElemCfg Cfg, typename... Cs>
void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const ElemNode<T, Cfg, Cs...>& e) {
    vdom::VNode v = vdom::VNode::element(std::string(html::Traits<T>::name));

    if constexpr (Cfg.has_id)   v.attrs.emplace_back("id", std::string(e.attrs.id));

    std::string_view style_cls = sheet.intern(e.style);
    if (Cfg.has_cls || !style_cls.empty()) {
        std::string cls;
        if constexpr (Cfg.has_cls) cls = std::string(e.attrs.cls);
        if (!style_cls.empty()) { if (!cls.empty()) cls += ' '; cls += style_cls; }
        v.attrs.emplace_back("class", std::move(cls));
    }
    if constexpr (Cfg.has_href) v.attrs.emplace_back("href", std::string(e.attrs.href));
    if constexpr (Cfg.has_attrs) {
        for (const auto& [k, val] : e.attrs.extra) v.attrs.emplace_back(k, val);
        for (const auto& f : e.attrs.flags)        v.attrs.emplace_back(f, "");
    }
    // sort attrs by name so VNode compares/diffs deterministically
    std::sort(v.attrs.begin(), v.attrs.end(),
              [](const auto& x, const auto& y){ return x.first < y.first; });

    if constexpr (!html::Traits<T>::is_void)
        std::apply([&](const auto&... cs){ (vbuild(v, sheet, cs), ...); }, e.children);

    parent.kids.push_back(std::move(v));
}

} // namespace detail

/// Build the VNode snapshot of a DSL tree (plus the stylesheet it needs).
template <typename Node>
[[nodiscard]] vdom::VNode to_vnode(const Node& node, style::StyleSheet& sheet) {
    vdom::VNode root = vdom::VNode::element("\x01root");
    detail::vbuild(root, sheet, node);
    // unwrap: the DSL root is the single child we just pushed
    return root.kids.empty() ? root : std::move(root.kids[0]);
}

} // namespace waya::render
