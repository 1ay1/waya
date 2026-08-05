#pragma once
/// \file vwalk.hpp
/// Walk a DSL tree into a `VNode` (the diffable snapshot) — the parallel of the
/// HTML renderer's walk, producing the virtual-DOM the diff engine compares.
///
/// The class attribute merges the user class and the interned style class, so a
/// style change shows up as an attribute delta and diffs like anything else.
/// Attributes are sorted by name so two VNodes compare deterministically.

#include "vdom.hpp"
#include "cache_id.hpp"
#include "escape.hpp"
#include "../dsl/node.hpp"
#include "../dsl/dynamic.hpp"
#include "../style/css.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <unordered_map>

namespace waya::render {

// ── Subtree cache (maya's component cache; combinators live in memo.hpp) ────

/// A per-session cache: CacheId → the VNode built last time. Two generations so
/// entries not touched this frame are evicted (bounded memory, like maya).
class MemoCache {
public:
    const vdom::VNode* get(CacheId id) {
        if (auto it = cur_.find(id.value); it != cur_.end()) return &it->second;
        if (auto it = prev_.find(id.value); it != prev_.end()) {
            auto [ins, _] = cur_.emplace(id.value, it->second);   // promote
            return &ins->second;
        }
        return nullptr;
    }
    const vdom::VNode& put(CacheId id, vdom::VNode v) {
        auto [it, _] = cur_.insert_or_assign(id.value, std::move(v));
        return it->second;
    }
    void rotate() { prev_.swap(cur_); cur_.clear(); }
    [[nodiscard]] std::size_t size() const { return cur_.size(); }
private:
    std::unordered_map<std::uint64_t, vdom::VNode> cur_, prev_;
};

/// The active cache during a walk (set by the live runtime; null for SSR).
inline thread_local MemoCache* active_memo = nullptr;
/// How many memoised callbacks actually ran this frame (tests / debug overlay).
inline thread_local std::size_t memo_builds = 0;

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
    auto n = vdom::VNode::textnode(t.value);
    vdom::finalize_hash(n);
    parent.kids.push_back(std::move(n));
}

inline void vbuild(vdom::VNode& parent, style::StyleSheet&, const dsl::RawHtml& r) {
    // Raw HTML isn't diffable structurally; treat it as an opaque text-ish leaf.
    auto n = vdom::VNode::textnode(r.html);
    n.tag = "\x01raw";   // marker so vnode_to_html emits it unescaped (handled below)
    vdom::finalize_hash(n);
    parent.kids.push_back(std::move(n));
}

template <html::Cat C>
void vbuild(vdom::VNode& parent, style::StyleSheet& sheet, const dsl::Frag<C>& f) {
    // FAST PATH: splice each produced node as a REAL child VNode (with its own
    // subtree hash), so a list diffs per-item — change one row, skip the rest.
    for (const auto& vp : f.vparts) vp(static_cast<void*>(&parent), sheet);
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

    vdom::finalize_hash(v);   // bottom-up: children are already hashed
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

// Satisfy the forward declaration in dsl/dynamic.hpp: fragments splice their
// produced nodes as real child VNodes (with per-subtree hashes) for fine diffs.
namespace waya::dsl::detail {
template <typename N>
void vbuild_child(void* parent_vnode, style::StyleSheet& sheet, const N& n) {
    auto* parent = static_cast<waya::vdom::VNode*>(parent_vnode);
    waya::render::detail::vbuild(*parent, sheet, n);
}

// Memoised child: consult the active per-session cache. On HIT the cached VNode
// is reused and `make` is never called (the whole point). On MISS `make()`
// builds the DSL node, we vbuild it, and cache the result under `id`.
template <typename Make>
void vbuild_memo_child(void* parent_vnode, style::StyleSheet& sheet,
                       waya::CacheId id, Make make) {
    auto* parent = static_cast<waya::vdom::VNode*>(parent_vnode);
    auto* cache = waya::render::active_memo;
    if (cache && !id.empty()) {
        if (const auto* hit = cache->get(id)) {
            parent->kids.push_back(*hit);   // REUSE — no rebuild, no callback
            return;
        }
    }
    // Miss: build the row, vbuild it into a temp, cache + splice.
    ++waya::render::memo_builds;
    auto node = make();
    waya::vdom::VNode tmp = waya::vdom::VNode::element("\x01memo");
    waya::render::detail::vbuild(tmp, sheet, node);
    if (!tmp.kids.empty()) {
        if (cache && !id.empty()) parent->kids.push_back(cache->put(id, std::move(tmp.kids[0])));
        else                      parent->kids.push_back(std::move(tmp.kids[0]));
    }
}
} // namespace waya::dsl::detail
