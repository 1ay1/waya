#pragma once
/// \file content_model.hpp
/// The single concept and the diagnostics that make invalid HTML fail to
/// compile. This is waya's headline guarantee, distilled to one predicate.

#include "category.hpp"
#include "tag.hpp"
#include "traits.hpp"
#include "../core/diagnostic.hpp"

namespace waya::html {

/// Every node type exposes `Categories` — the content categories it belongs to.
/// Elements read it from their Traits; text and fragments define their own.
template <typename N>
inline constexpr Cat categories_of = N::Categories;

/// THE gate. A child is permitted inside `Parent` iff the parent's permitted
/// categories intersect the child's own categories.
template <Tag Parent, typename Child>
concept PermittedChild = any(Traits<Parent>::permits & categories_of<Child>);

// ── Diagnostics (authored by waya, one readable sentence — see DESIGN §10.1) ─

namespace detail {

/// "waya: <div> is not permitted inside <p>. The HTML5 content model for <p>
///  permits phrasing content (...). See https://html.spec.whatwg.org/#the-p-element"
template <Tag Parent, Tag Child>
consteval auto nesting_msg() {
    diag::Msg<512> m;
    m += "waya: <";  m += Traits<Child>::name;
    m += "> is not permitted inside <"; m += Traits<Parent>::name;
    m += ">. The HTML5 content model for <"; m += Traits<Parent>::name;
    m += "> permits "; m += describe(Traits<Parent>::permits);
    m += ". See https://html.spec.whatwg.org/#the-"; m += Traits<Parent>::name;
    m += "-element";
    return m;
}

template <Tag Parent>
consteval auto text_msg() {
    diag::Msg<384> m;
    m += "waya: text is not permitted inside <"; m += Traits<Parent>::name;
    m += ">. It permits "; m += describe(Traits<Parent>::permits); m += ".";
    return m;
}

/// A fragment (from each/when/dyn) carries a content category, not a tag. This
/// message names what the fragment produces so the error is actionable.
template <Tag Parent, Cat ChildCats>
consteval auto frag_msg() {
    diag::Msg<448> m;
    m += "waya: this fragment produces "; m += describe(ChildCats);
    m += ", which is not permitted inside <"; m += Traits<Parent>::name;
    m += ">. <"; m += Traits<Parent>::name;
    m += "> permits "; m += describe(Traits<Parent>::permits); m += ".";
    return m;
}

template <Tag T>
consteval auto void_msg() {
    diag::Msg<256> m;
    m += "waya: <"; m += Traits<T>::name;
    m += "> is a void element and cannot have children.";
    return m;
}

} // namespace detail

// ── The checker ─────────────────────────────────────────────────────────────
//
// Two tricks (from the Phase 0 spike) keep GCC's output to ONE sentence:
//   1. assert on a plain `bool`, never on the concept (else GCC appends the
//      concept-satisfaction derivation);
//   2. trigger via TYPE instantiation, not a constexpr call (else GCC adds an
//      "in constexpr expansion of …" frame for every node in the tree).
// A child type exposing `Tag ChildTag` is reported by name; anything else
// (e.g. text) is reported as text.

template <typename C> struct TagOf { static constexpr bool has = false; };

/// A node type opts into named diagnostics by defining `static constexpr Tag NodeTag`.
template <typename C>
    requires requires { C::NodeTag; }
struct TagOf<C> {
    static constexpr bool has = true;
    static constexpr Tag  value = C::NodeTag;
};

/// Distinguishes fragments (each/when/dyn — they carry `Categories` but no
/// `NodeTag`) from plain text, so their diagnostic can name the category they
/// produce.
template <typename C> struct IsFrag { static constexpr bool value = false; };
template <typename C>
    requires requires { C::WayaFragment; }
struct IsFrag<C> { static constexpr bool value = true; };

template <Tag Parent, typename Child,
          bool Ok = PermittedChild<Parent, Child>,
          bool IsElem = TagOf<Child>::has,
          bool IsFragment = IsFrag<Child>::value>
struct CheckChild {
    static_assert(Ok, detail::nesting_msg<Parent, TagOf<Child>::value>());
    static constexpr bool value = true;
};

// A fragment child that is not permitted: name the category it produces.
template <Tag Parent, typename Child>
struct CheckChild<Parent, Child, false, false, true> {
    static_assert(sizeof(Child) == 0, detail::frag_msg<Parent, categories_of<Child>>());
    static constexpr bool value = true;
};

// Plain text (or other tagless, non-fragment node) that is not permitted.
template <Tag Parent, typename Child>
struct CheckChild<Parent, Child, false, false, false> {
    static_assert(sizeof(Child) == 0, detail::text_msg<Parent>());
    static constexpr bool value = true;
};

template <Tag Parent, typename Child, bool IsElem, bool IsFragment>
struct CheckChild<Parent, Child, true, IsElem, IsFragment> {
    static constexpr bool value = true;
};

/// Fires one static_assert per offending child; a no-op when all are valid.
template <Tag Parent, typename... Children>
inline constexpr bool check_children = (CheckChild<Parent, Children>::value && ...);

} // namespace waya::html
