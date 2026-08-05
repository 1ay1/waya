#pragma once
/// \file memo.hpp
/// `memo(id, fn)` — subtree memoisation keyed by a `CacheId` (maya's component
/// cache). If `id` matches last frame's, the cached VNode is REUSED and `fn` is
/// not even called. This removes the O(page-size) rebuild: an unchanged 1000-row
/// list re-runs zero row callbacks.
///
///   each_keyed(rows,
///     [](const Row& r){ return cache_id("row", r.id, r.ms, r.up); },  // key
///     [](const Row& r){ return tr_(td_(text(r.name)), …); });          // view
///
/// The cache lives in the live session (survives between frames). For plain SSR
/// (no session) memo just calls `fn` \u2014 nothing to cache against.

#include "cache_id.hpp"
#include "vwalk.hpp"
#include "../dsl/dynamic.hpp"
#include "vdom.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace waya::dsl {

/// A memoised node: carries its CacheId and a builder. During vwalk it consults
/// the cache; during HTML render it just builds (SSR has no cache).
template <typename Fn>
struct Memo {
    // Category is that of the node the builder returns.
    using Node = std::decay_t<decltype(std::declval<Fn>()())>;
    static constexpr html::Cat Categories = Node::Categories;
    static constexpr bool WayaMemo = true;

    waya::CacheId id;
    Fn            build;
};

/// `memo(id, fn)` — memoise the subtree `fn()` under `id`.
template <typename Fn>
Memo<Fn> memo(waya::CacheId id, Fn fn) { return {id, std::move(fn)}; }

/// `each_keyed(range, key_fn, view_fn)` — a list whose rows are memoised by key.
/// Unchanged rows are reused wholesale; only changed/new rows call `view_fn`.
///
/// The builder is DEFERRED: we store (key, λ: build this row). On a cache hit the
/// λ is never called — that's the whole point. For the HTML (SSR) path there is
/// no cache, so it builds every row.
template <typename Range, typename KeyFn, typename ViewFn>
auto each_keyed(const Range& range, KeyFn key_fn, ViewFn view_fn) {
    using Node = std::decay_t<decltype(view_fn(*std::begin(range)))>;
    Frag<node_categories_v<Node>> f;
    // Store (key, deferred-builder) per row. Copy the item into the builder so
    // it stays valid; the builder runs only on a cache miss.
    using Item = std::decay_t<decltype(*std::begin(range))>;
    auto items = std::make_shared<std::vector<std::pair<waya::CacheId, Item>>>();
    items->reserve(std::size(range));
    for (const auto& x : range) items->emplace_back(key_fn(x), x);

    f.parts.emplace_back([items, view_fn](std::string& o, style::StyleSheet& s) {
        for (auto& [id, item] : *items) { auto n = view_fn(item); detail::render_child(o, s, n); }
    });
    f.vparts.emplace_back([items, view_fn](void* p, style::StyleSheet& s) {
        for (auto& [id, item] : *items)
            detail::vbuild_memo_child(p, s, id, [&]{ return view_fn(item); });
    });
    return f;
}

namespace detail {
/// Forward-declared bridge (defined in render/vwalk.hpp): vbuild a child through
/// the memo cache. `make` is a NULLARY builder called ONLY on a cache miss.
template <typename Make>
void vbuild_memo_child(void* parent_vnode, style::StyleSheet& sheet,
                       waya::CacheId id, Make make);
} // namespace detail

} // namespace waya::dsl
