#pragma once
/// \file element.hpp
/// Element factories (`div_`, `p_`, `tr_`, …) and the `|` pipe overloads for
/// attributes and style. The factories accept any children and diagnose in the
/// body so invalid HTML produces one readable sentence, not an overload dump.

#include "node.hpp"
#include "../html/content_model.hpp"
#include "../core/str.hpp"
#include "../style/tokens.hpp"

namespace waya::dsl {

// ── Generic element construction ────────────────────────────────────────────

/// Non-void element. Content-model checked in the body (DESIGN §10.1 tricks).
template <Tag T, ElemCfg Cfg = ElemCfg{}, typename... Cs>
constexpr auto elem(Cs... cs) {
    static_assert(!html::Traits<T>::is_void, html::detail::void_msg<T>());
    static_assert(html::check_children<T, Cs...>);
    return ElemNode<T, Cfg, Cs...>{{}, {}, {std::move(cs)...}};
}

/// Void element (<br>, <img>, …) — passing children is a named error.
template <Tag T, ElemCfg Cfg = ElemCfg{}, typename... Cs>
constexpr auto void_elem(Cs... cs) {
    static_assert(html::Traits<T>::is_void, "void_elem used on a non-void element");
    static_assert(sizeof...(Cs) == 0, html::detail::void_msg<T>());
    return ElemNode<T, Cfg>{};
}

// ── Text leaf ───────────────────────────────────────────────────────────────
inline TextNode text(std::string s) { return {std::move(s)}; }
inline TextNode text(std::string_view s) { return {std::string(s)}; }
inline TextNode text(const char* s) { return {std::string(s)}; }

/// Numeric text with an optional suffix, e.g. `text(latency, "ms")`.
template <typename N>
    requires std::is_arithmetic_v<N>
inline TextNode text(N n, std::string_view suffix = {}) {
    return {std::to_string(n) + std::string(suffix)};
}

// ── Named factories ─────────────────────────────────────────────────────────
#define WAYA_TAG(NAME, TAG)                                                  \
    template <typename... Cs> constexpr auto NAME(Cs... cs) {                \
        return elem<Tag::TAG>(std::move(cs)...);                             \
    }
#define WAYA_VOID(NAME, TAG)                                                 \
    template <typename... Cs> constexpr auto NAME(Cs... cs) {                \
        return void_elem<Tag::TAG>(std::move(cs)...);                        \
    }

WAYA_TAG(html_, html)   WAYA_TAG(head_, head)   WAYA_TAG(body_, body)
WAYA_TAG(title_, title) WAYA_TAG(style_, style) WAYA_TAG(script_, script)
WAYA_TAG(header_, header) WAYA_TAG(footer_, footer) WAYA_TAG(main_, main)
WAYA_TAG(nav_, nav) WAYA_TAG(section_, section) WAYA_TAG(article_, article)
WAYA_TAG(aside_, aside)
WAYA_TAG(h1_, h1) WAYA_TAG(h2_, h2) WAYA_TAG(h3_, h3)
WAYA_TAG(h4_, h4) WAYA_TAG(h5_, h5) WAYA_TAG(h6_, h6)
WAYA_TAG(div_, div) WAYA_TAG(p_, p) WAYA_TAG(pre_, pre)
WAYA_TAG(blockquote_, blockquote) WAYA_TAG(figure_, figure) WAYA_TAG(figcaption_, figcaption)
WAYA_TAG(ul_, ul) WAYA_TAG(ol_, ol) WAYA_TAG(li_, li)
WAYA_TAG(dl_, dl) WAYA_TAG(dt_, dt) WAYA_TAG(dd_, dd)
WAYA_TAG(a_, a) WAYA_TAG(span_, span) WAYA_TAG(em_, em) WAYA_TAG(strong_, strong)
WAYA_TAG(small_, small) WAYA_TAG(code_, code) WAYA_TAG(i_, i) WAYA_TAG(b_, b)
WAYA_TAG(u_, u) WAYA_TAG(mark_, mark) WAYA_TAG(sub_, sub) WAYA_TAG(sup_, sup)
WAYA_TAG(time_, time) WAYA_TAG(abbr_, abbr) WAYA_TAG(kbd_, kbd)
WAYA_TAG(picture_, picture) WAYA_TAG(video_, video) WAYA_TAG(audio_, audio)
WAYA_TAG(canvas_, canvas)
WAYA_TAG(table_, table) WAYA_TAG(caption_, caption)
WAYA_TAG(thead_, thead) WAYA_TAG(tbody_, tbody) WAYA_TAG(tfoot_, tfoot)
WAYA_TAG(tr_, tr) WAYA_TAG(td_, td) WAYA_TAG(th_, th)
WAYA_TAG(form_, form) WAYA_TAG(label_, label) WAYA_TAG(button_, button)
WAYA_TAG(select_, select) WAYA_TAG(option_, option) WAYA_TAG(optgroup_, optgroup)
WAYA_TAG(textarea_, textarea) WAYA_TAG(fieldset_, fieldset) WAYA_TAG(legend_, legend)
WAYA_TAG(output_, output) WAYA_TAG(progress_, progress) WAYA_TAG(meter_, meter)
WAYA_TAG(details_, details) WAYA_TAG(summary_, summary) WAYA_TAG(dialog_, dialog)

WAYA_VOID(br_, br) WAYA_VOID(wbr_, wbr) WAYA_VOID(hr_, hr)
WAYA_VOID(img_, img) WAYA_VOID(input_, input) WAYA_VOID(meta_, meta)
WAYA_VOID(link_, link) WAYA_VOID(col_element, col) WAYA_VOID(source_, source)

#undef WAYA_TAG
#undef WAYA_VOID

// ── Attribute pipes ─────────────────────────────────────────────────────────

template <Str S> struct ClsTag {};
template <Str S> struct IdTag {};
template <Str S> struct HrefTag {};
template <Str S> inline constexpr ClsTag<S>  cls{};
template <Str S> inline constexpr IdTag<S>   id_{};
template <Str S> inline constexpr HrefTag<S> href{};

namespace detail {
consteval ElemCfg set_id(ElemCfg c)    { c.has_id = true;    return c; }
consteval ElemCfg set_cls(ElemCfg c)   { c.has_cls = true;   return c; }
consteval ElemCfg set_href(ElemCfg c)  { c.has_href = true;  return c; }
consteval ElemCfg set_style(ElemCfg c) { c.has_style = true; return c; }
} // namespace detail

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, ClsTag<S>) {
    n.attrs.cls = S.view();
    return ElemNode<T, detail::set_cls(Cfg), Cs...>{n.attrs, n.style, std::move(n.children)};
}

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, IdTag<S>) {
    n.attrs.id = S.view();
    return ElemNode<T, detail::set_id(Cfg), Cs...>{n.attrs, n.style, std::move(n.children)};
}

// `href` is type-gated to elements that accept it (maya's border-colour rule).
template <Tag T> concept AcceptsHref = (T == Tag::a || T == Tag::link || T == Tag::base);
namespace detail {
template <Tag T> struct HrefGate {
    static_assert(AcceptsHref<T>, []{
        diag::Msg<256> m; m += "waya: the `href` attribute is not valid on <";
        m += html::Traits<T>::name; m += ">."; return m; }());
    static constexpr bool value = true;
};
template <Tag T> requires AcceptsHref<T> struct HrefGate<T> { static constexpr bool value = true; };
} // namespace detail

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, HrefTag<S>) {
    static_assert(detail::HrefGate<T>::value);
    n.attrs.href = S.view();
    return ElemNode<T, detail::set_href(Cfg), Cs...>{n.attrs, n.style, std::move(n.children)};
}

// ── Style pipes ──────────────────────────────────────────────────────────────
//
// Any StyleToken merges into the node's style. Two type-state facts live in
// ElemCfg (a template parameter, so they are visible to static_assert):
//   • container-making tokens (`row`/`col`/`flex`/`grid`) set is_container;
//   • container-only tokens (`gap`/`justify`/`items`/`wrap`) require it, else
//     a compile error — maya's border-colour rule, transposed onto the box model.

namespace detail {
template <bool Ok>
struct ContainerGate {
    static_assert(Ok, style::detail::container_msg("this property"));
    static constexpr bool value = true;
};
template <> struct ContainerGate<true> { static constexpr bool value = true; };

consteval ElemCfg set_container(ElemCfg c) { c.is_container = true; return c; }
} // namespace detail

template <Tag T, ElemCfg Cfg, typename... Cs, style::StyleToken Tok>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, Tok tok) {
    if constexpr (style::is_container_only<Tok>)
        static_assert(detail::ContainerGate<Cfg.is_container>::value);

    constexpr ElemCfg NewCfg = [] {
        ElemCfg c = detail::set_style(Cfg);
        if (style::makes_container<Tok>) c.is_container = true;
        return c;
    }();
    n.style = tok.apply(n.style);
    return ElemNode<T, NewCfg, Cs...>{n.attrs, n.style, std::move(n.children)};
}

} // namespace waya::dsl
