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
#include <vector>

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
    bool has_attrs    = false;   ///< any general attribute/event present
    bool is_container = false;   ///< set by `| row`/`| col`/`| flex`/`| grid`
    constexpr bool operator==(const ElemCfg&) const = default;
};

/// Attribute values carried on a node.
///
/// The typed id/cls/href stay as fast fields (common, and href is type-gated).
/// Everything else — ANY attribute, boolean attributes, data-*, ARIA, and
/// event handlers — lives in the general `extra` channel, exactly like the
/// style `extra` channel. This is the "not limiting" guarantee for attributes:
/// nothing is off-limits, and it's the same clean pipe.
struct Attrs {
    std::string_view id{};
    std::string_view cls{};
    std::string_view href{};

    /// (name, value) — rendered as name="value" (value escaped).
    std::vector<std::pair<std::string, std::string>> extra;
    /// boolean attributes (disabled, checked, required, …) — rendered bare.
    std::vector<std::string> flags;

    [[nodiscard]] bool any() const {
        return !extra.empty() || !flags.empty();
    }
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
