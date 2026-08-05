#pragma once
/// \file node.hpp
/// The compile-time UI node and its runtime lowering.
///
/// `ElemNode<Tag, Cfg, Children...>` is the compile-time tree the DSL builds;
/// its structure, tag, and attribute-presence flags live in the type. Values
/// (class strings, style) ride as members so the NTTP config stays tiny and
/// diagnostics stay readable (DESIGN §10.1).

#include "../html/tag.hpp"
#include "../html/traits.hpp"
#include "../style/sty.hpp"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace waya::dsl {

using html::Tag;
using style::Sty;

/// Presence flags for attributes and layout state — the type-state, kept minimal.
/// `is_container` is what gates the container-only style tokens at compile time,
/// exactly as maya's `has_border` gates border colour.
struct ElemCfg {
    bool has_id       = false;
    bool has_cls      = false;
    bool has_href     = false;
    bool has_style    = false;
    bool is_container = false;   ///< set by `| row`/`| col`/`| flex`/`| grid`
    constexpr bool operator==(const ElemCfg&) const = default;
};

/// Runtime attribute values (string_views into static storage; free to copy).
struct Attrs {
    std::string_view id{};
    std::string_view cls{};
    std::string_view href{};
};

// ── Leaf: text ──────────────────────────────────────────────────────────────

/// A dynamic text leaf (escaped at render). Belongs to phrasing/flow content.
struct TextNode {
    static constexpr html::Cat Categories =
        html::Cat::Phrasing | html::Cat::Flow | html::Cat::Palpable;
    std::string value;
};

// ── Element node ────────────────────────────────────────────────────────────

template <Tag T, ElemCfg Cfg, typename... Children>
struct ElemNode {
    static constexpr html::Cat Categories = html::Traits<T>::categories;
    static constexpr Tag       NodeTag    = T;   ///< opt into named diagnostics
    static constexpr Tag       tag        = T;
    static constexpr ElemCfg   cfg        = Cfg;

    Attrs                  attrs{};
    Sty                    style{};
    std::tuple<Children...> children;
};

} // namespace waya::dsl
