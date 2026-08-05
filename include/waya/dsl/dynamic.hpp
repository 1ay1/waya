#pragma once
/// \file dynamic.hpp
/// Runtime-data combinators: `each`, `when`, `cond`, `dyn`, `frag`.
///
/// waya's DSL is compile-time by default, but real apps render runtime data of
/// a runtime-varying shape (a list whose length is unknown at compile time, a
/// branch chosen at runtime). A `std::tuple<Children...>` cannot express that.
///
/// The bridge is `Frag<Cats>`: a type-erased list of self-rendering nodes that
/// carries its **content category as a compile-time template parameter**, so
/// the HTML content model still works through the erasure —
/// `div_(each(rows, …tr…))` is still a compile error because the fragment is
/// typed `Frag<Cat::TableRow>`.
///
/// Powerful (any range, any node, arbitrarily nested) but clean (`each`/`when`
/// are one call each and read like the static DSL).

#include "node.hpp"
#include "../html/category.hpp"
#include "../render/escape.hpp"
#include "../style/css.hpp"

#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace waya::dsl {

/// A type-erased, self-rendering fragment. Each part renders one node into the
/// output string, interning its styles into the shared page StyleSheet. `Cats`
/// is the content category of the produced content — the content model reads it.
///
/// FAST PATH: a fragment carries a SECOND erased hook per part that splices a
/// real child VNode into a parent (with its own subtree hash), so `each` over
/// 1000 rows produces 1000 diffable VNodes — change one cell and the diff skips
/// the other 999 via their hashes. Without this, a list would be one opaque
/// blob that re-diffs and re-sends wholesale (the un-maya way).
template <html::Cat Cats>
struct Frag {
    static constexpr html::Cat Categories = Cats;
    static constexpr bool WayaFragment = true;   ///< enables fragment diagnostics
    using Part  = std::function<void(std::string&, style::StyleSheet&)>;
    using VPart = std::function<void(void* parent_vnode, style::StyleSheet&)>;
    std::vector<Part>  parts;    ///< render to HTML
    std::vector<VPart> vparts;   ///< splice a child VNode into parent
};

/// The content category a node type belongs to. Every waya node type exposes
/// `Categories`; this reads it so combinators can propagate it onto their Frag.
template <typename N>
inline constexpr html::Cat node_categories_v = N::Categories;

// The renderer's node-walk, forward-declared so combinators can capture it.
// Defined in render/html.hpp; both are included together via waya.hpp.
namespace detail {
template <typename N>
void render_child(std::string& out, style::StyleSheet& sheet, const N& n);

// Splice a real child VNode (with its subtree hash) into `parent_vnode`, which
// is a `waya::vdom::VNode*` erased to void*. Defined in render/vwalk.hpp.
template <typename N>
void vbuild_child(void* parent_vnode, style::StyleSheet& sheet, const N& n);
} // namespace detail

// ── frag(): build a Frag from an explicit list of homogeneous nodes ─────────
// Rarely needed directly, but it's the primitive the others build on.

template <typename N>
auto frag(std::vector<N> nodes) {
    Frag<node_categories_v<N>> f;
    f.parts.reserve(nodes.size());
    f.vparts.reserve(nodes.size());
    for (auto& n : nodes) {
        f.parts.emplace_back([n](std::string& o, style::StyleSheet& s) {
            detail::render_child(o, s, n);
        });
        f.vparts.emplace_back([n](void* p, style::StyleSheet& s) {
            detail::vbuild_child(p, s, n);
        });
    }
    return f;
}

// ── each(): map a range to a fragment ───────────────────────────────────────
//
//   tbody_(
//     each(model.rows, [](const Row& r) {
//       return tr_(td_(text(r.name)), td_(text(r.status)));
//     })
//   )
//
// The callback's return type fixes the fragment's content category, so the
// nesting `tbody_(each(... -> tr ...))` is validated exactly like `tbody_(tr_)`.

template <typename Range, typename Fn>
auto each(const Range& range, Fn fn) {
    using Node = std::decay_t<decltype(fn(*std::begin(range)))>;
    Frag<node_categories_v<Node>> f;
    // FAST PATH: materialise all nodes into ONE vector and capture it in ONE
    // part/vpart closure, instead of allocating a std::function per item. For a
    // 1000-row list that's 2 allocations, not 2000 — the difference between
    // "slow" and maya-fast.
    auto nodes = std::make_shared<std::vector<Node>>();
    nodes->reserve(std::size(range));
    for (const auto& item : range) nodes->push_back(fn(item));
    f.parts.emplace_back([nodes](std::string& o, style::StyleSheet& s) {
        for (const auto& n : *nodes) detail::render_child(o, s, n);
    });
    f.vparts.emplace_back([nodes](void* p, style::StyleSheet& s) {
        for (const auto& n : *nodes) detail::vbuild_child(p, s, n);
    });
    return f;
}

/// each with an index: `each(rows, [](const Row& r, std::size_t i){ … })`.
template <typename Range, typename Fn>
auto each_indexed(const Range& range, Fn fn) {
    using Node = std::decay_t<decltype(fn(*std::begin(range), std::size_t{}))>;
    Frag<node_categories_v<Node>> f;
    auto nodes = std::make_shared<std::vector<Node>>();
    nodes->reserve(std::size(range));
    std::size_t i = 0;
    for (const auto& item : range) nodes->push_back(fn(item, i++));
    f.parts.emplace_back([nodes](std::string& o, style::StyleSheet& s) {
        for (const auto& n : *nodes) detail::render_child(o, s, n);
    });
    f.vparts.emplace_back([nodes](void* p, style::StyleSheet& s) {
        for (const auto& n : *nodes) detail::vbuild_child(p, s, n);
    });
    return f;
}

// ── when(): conditional rendering ───────────────────────────────────────────
//
//   when(model.loading, spinner())                 // render or nothing
//   when(model.ok, ok_view(), error_view())        // this or that
//
// Both branches must produce the same content category (so the result has a
// well-defined category for the content model). A single-branch `when` renders
// nothing when false.

template <typename Node>
auto when(bool cond, Node node) {
    Frag<node_categories_v<Node>> f;
    if (cond) {
        f.parts.emplace_back([node](std::string& o, style::StyleSheet& s) {
            detail::render_child(o, s, node);
        });
        f.vparts.emplace_back([node](void* p, style::StyleSheet& s) {
            detail::vbuild_child(p, s, node);
        });
    }
    return f;
}

template <typename NodeT, typename NodeF>
auto when(bool cond, NodeT if_true, NodeF if_false) {
    static_assert(node_categories_v<NodeT> == node_categories_v<NodeF>,
        "waya: both branches of when(cond, a, b) must have the same content "
        "category so the result has a well-defined place in the HTML tree.");
    Frag<node_categories_v<NodeT>> f;
    if (cond) {
        f.parts.emplace_back([n = if_true](std::string& o, style::StyleSheet& s) {
            detail::render_child(o, s, n);
        });
        f.vparts.emplace_back([n = if_true](void* p, style::StyleSheet& s) {
            detail::vbuild_child(p, s, n);
        });
    } else {
        f.parts.emplace_back([n = if_false](std::string& o, style::StyleSheet& s) {
            detail::render_child(o, s, n);
        });
        f.vparts.emplace_back([n = if_false](void* p, style::StyleSheet& s) {
            detail::vbuild_child(p, s, n);
        });
    }
    return f;
}

// ── dyn(): the general runtime escape hatch ─────────────────────────────────
//
//   dyn<Cat::Flow>([&]{ return build_something_complex(); })
//
// Runs an arbitrary callback at render time. The category must be stated (it
// cannot be inferred without calling the callback), which keeps the content
// model intact. This is maya's `dyn()`, typed for the web.

template <html::Cat Cats, typename Fn>
auto dyn(Fn fn) {
    Frag<Cats> f;
    f.parts.emplace_back([fn](std::string& o, style::StyleSheet& s) {
        detail::render_child(o, s, fn());
    });
    f.vparts.emplace_back([fn](void* p, style::StyleSheet& s) {
        detail::vbuild_child(p, s, fn());
    });
    return f;
}

// ── raw(): trusted pre-rendered HTML (escape hatch, greppable) ──────────────
// Belongs to Flow|Phrasing so it slots in almost anywhere; bypasses escaping.

struct RawHtml {
    static constexpr html::Cat Categories =
        html::Cat::Flow | html::Cat::Phrasing;
    static constexpr bool WayaFragment = true;
    std::string html;
};
inline RawHtml raw(std::string trusted) { return {std::move(trusted)}; }

} // namespace waya::dsl
