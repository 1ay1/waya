#pragma once
// waya::dsl — Phase 0 spike: type-state HTML DSL with content-model enforcement
//
// Proves the core claim of DESIGN.md §3: invalid HTML does not compile.
//
//   p_(text("hi"))     // ok    — phrasing content inside <p>
//   p_(div_())         // ERROR — <div> is not permitted inside <p>
//   tr_(td_())         // ok
//   div_(td_())        // ERROR — <td> may only appear inside <tr>
//   br_(text("x"))     // ERROR — <br> is a void element
//
// The mechanism is maya's: state lives in a structural NTTP, and `requires`
// clauses gate the state transitions. See reference/maya/include/maya/dsl.hpp:543
// for the original (`requires (Cfg.has_border)` gating border colour).

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace waya {

// ── Compile-time string (NTTP-able) ─────────────────────────────────────────
// Same shape as maya's Str<N>: a structural type usable as a template param.

template <std::size_t N>
struct Str {
    char data[N]{};
    consteval Str(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
    }
    [[nodiscard]] constexpr std::string_view view() const { return {data, N - 1}; }
    [[nodiscard]] static constexpr std::size_t size() { return N - 1; }
};

// ── HTML5 content categories ────────────────────────────────────────────────
// https://html.spec.whatwg.org/multipage/dom.html#kinds-of-content

enum class Cat : uint32_t {
    None       = 0,
    Metadata   = 1u << 0,
    Flow       = 1u << 1,
    Sectioning = 1u << 2,
    Heading    = 1u << 3,
    Phrasing   = 1u << 4,
    Embedded   = 1u << 5,
    Interactive= 1u << 6,
    Palpable   = 1u << 7,
    // Structural pseudo-categories for elements with strict parents.
    TableRow   = 1u << 8,   // <tr>
    TableCell  = 1u << 9,   // <td>, <th>
    TableSect  = 1u << 10,  // <thead>, <tbody>, <tfoot>
    ListItem   = 1u << 11,  // <li>
    HtmlSect   = 1u << 12,  // <head>, <body> — only inside <html>
    Any        = 0xFFFFFFFFu,
};

constexpr Cat operator|(Cat a, Cat b) {
    return static_cast<Cat>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr Cat operator&(Cat a, Cat b) {
    return static_cast<Cat>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
constexpr bool any(Cat c) { return static_cast<uint32_t>(c) != 0; }

// ── Tags ────────────────────────────────────────────────────────────────────

enum class Tag : uint16_t {
    html, head, title, meta, link, body,
    div, p, span, a, br, img, hr,
    h1, h2, h3,
    ul, ol, li,
    table, thead, tbody, tr, td, th,
    button, input, label, form,
};

// ── Element traits ──────────────────────────────────────────────────────────
// In the real framework this file is GENERATED from the WHATWG spec table
// (tools/gen_elements.py). Hand-written here for the spike.

template <Tag T> struct Traits;

#define WAYA_ELEM(TAG, NAME, CATS, PERMITS, VOIDNESS)                      \
    template <> struct Traits<Tag::TAG> {                                  \
        static constexpr std::string_view name = NAME;                     \
        static constexpr Cat categories = CATS;                            \
        static constexpr Cat permits    = PERMITS;                         \
        static constexpr bool is_void   = VOIDNESS;                        \
    }

//         tag     name       categories                         permits                       void
WAYA_ELEM(html,  "html",   Cat::None,                          Cat::HtmlSect,                false);
WAYA_ELEM(head,  "head",   Cat::HtmlSect,                      Cat::Metadata,                false);
WAYA_ELEM(title, "title",  Cat::Metadata,                      Cat::Phrasing,                false);
WAYA_ELEM(meta,  "meta",   Cat::Metadata,                      Cat::None,                    true);
WAYA_ELEM(link,  "link",   Cat::Metadata,                      Cat::None,                    true);
WAYA_ELEM(body,  "body",   Cat::HtmlSect,                      Cat::Flow,                    false);

WAYA_ELEM(div,   "div",    Cat::Flow | Cat::Palpable,          Cat::Flow,                    false);
WAYA_ELEM(p,     "p",      Cat::Flow | Cat::Palpable,          Cat::Phrasing,                false);
WAYA_ELEM(span,  "span",   Cat::Flow | Cat::Phrasing | Cat::Palpable, Cat::Phrasing,         false);
WAYA_ELEM(a,     "a",      Cat::Flow | Cat::Phrasing | Cat::Interactive | Cat::Palpable, Cat::Flow, false);
WAYA_ELEM(br,    "br",     Cat::Flow | Cat::Phrasing,          Cat::None,                    true);
WAYA_ELEM(img,   "img",    Cat::Flow | Cat::Phrasing | Cat::Embedded | Cat::Palpable, Cat::None, true);
WAYA_ELEM(hr,    "hr",     Cat::Flow,                          Cat::None,                    true);

WAYA_ELEM(h1,    "h1",     Cat::Flow | Cat::Heading | Cat::Palpable, Cat::Phrasing,          false);
WAYA_ELEM(h2,    "h2",     Cat::Flow | Cat::Heading | Cat::Palpable, Cat::Phrasing,          false);
WAYA_ELEM(h3,    "h3",     Cat::Flow | Cat::Heading | Cat::Palpable, Cat::Phrasing,          false);

WAYA_ELEM(ul,    "ul",     Cat::Flow | Cat::Palpable,          Cat::ListItem,                false);
WAYA_ELEM(ol,    "ol",     Cat::Flow | Cat::Palpable,          Cat::ListItem,                false);
WAYA_ELEM(li,    "li",     Cat::ListItem,                      Cat::Flow,                    false);

WAYA_ELEM(table, "table",  Cat::Flow | Cat::Palpable,          Cat::TableSect | Cat::TableRow, false);
WAYA_ELEM(thead, "thead",  Cat::TableSect,                     Cat::TableRow,                false);
WAYA_ELEM(tbody, "tbody",  Cat::TableSect,                     Cat::TableRow,                false);
WAYA_ELEM(tr,    "tr",     Cat::TableRow,                      Cat::TableCell,               false);
WAYA_ELEM(td,    "td",     Cat::TableCell,                     Cat::Flow,                    false);
WAYA_ELEM(th,    "th",     Cat::TableCell,                     Cat::Flow,                    false);

WAYA_ELEM(button,"button", Cat::Flow | Cat::Phrasing | Cat::Interactive | Cat::Palpable, Cat::Phrasing, false);
WAYA_ELEM(input, "input",  Cat::Flow | Cat::Phrasing | Cat::Interactive, Cat::None,          true);
WAYA_ELEM(label, "label",  Cat::Flow | Cat::Phrasing | Cat::Interactive | Cat::Palpable, Cat::Phrasing, false);
WAYA_ELEM(form,  "form",   Cat::Flow | Cat::Palpable,          Cat::Flow,                    false);

#undef WAYA_ELEM

// ── Element config (the type-state, carried as a structural NTTP) ───────────
// Mirrors maya's BoxCfg. Every `| attr` returns a node with an updated Cfg,
// so the accumulated state is visible to `requires` clauses.
//
// DESIGN NOTE (found empirically in the Phase 0 spike): keep this struct
// SMALL. GCC prints the full NTTP value in every instantiation backtrace, so a
// config holding three fixed char arrays turned a one-line diagnostic into a
// 31-line wall of `std::array<char, 256>()`. Attribute VALUES therefore live
// in a separate compile-time pool keyed by index; the config carries only
// indices and flags, and prints as `ElemCfg{1, 2, 0, 3}`.

struct ElemCfg {
    bool has_id   = false;
    bool has_cls  = false;
    bool has_href = false;
};

// Attribute values ride along as string_views into static storage (the Str<S>
// template parameter's data), so they cost nothing at runtime and stay
// constexpr-usable — while keeping ElemCfg tiny for readable diagnostics.
struct Attrs {
    std::string_view id{};
    std::string_view cls{};
    std::string_view href{};
};

// ── Nodes ───────────────────────────────────────────────────────────────────

struct TextNode {
    std::string_view s;
    static constexpr Cat categories = Cat::Phrasing | Cat::Flow | Cat::Palpable;
};

template <Str S>
struct StaticTextNode {
    static constexpr Cat categories = Cat::Phrasing | Cat::Flow | Cat::Palpable;
};

template <Tag T, ElemCfg Cfg, typename... Cs>
struct ElemNode {
    static constexpr Cat categories = Traits<T>::categories;
    static constexpr Tag tag        = T;
    static constexpr ElemCfg cfg    = Cfg;
    Attrs             attrs{};
    std::tuple<Cs...> children;
};

// Category extraction for any node type.
template <typename N>
inline constexpr Cat categories_of = N::categories;

// ── The gate ────────────────────────────────────────────────────────────────
// This one concept is the entire guarantee of DESIGN.md §3.

template <Tag Parent, typename Child>
concept PermittedChild = any(Traits<Parent>::permits & categories_of<Child>);

// ── Diagnostics (C++26 P2741: static_assert with a computed message) ────────
//
// DESIGN.md risk #2 is error-message quality. If the content model is enforced
// only by a `requires` clause on the factory, the compiler reports an
// overload-resolution failure and prints ~37 lines of substitution noise.
//
// Instead we let the factory ACCEPT any child and diagnose inside the body,
// where a single static_assert fires with a sentence we compose ourselves.
// GCC 16 prints exactly one error line for this.

namespace detail {

// A tiny constexpr string builder — the message must be a core constant
// expression with data()/size(), which std::string is not (yet) in this role.
template <std::size_t N>
struct FixedStr {
    std::array<char, N> buf{};
    std::size_t         len = 0;
    constexpr void append(std::string_view s) {
        for (char c : s) if (len < N - 1) buf[len++] = c;
    }
    [[nodiscard]] constexpr const char* data() const { return buf.data(); }
    [[nodiscard]] constexpr std::size_t size() const { return len; }
};

consteval std::string_view describe(Cat c) {
    if (any(c & Cat::Phrasing))  return "phrasing content (text, <span>, <a>, <img>, …)";
    if (any(c & Cat::Flow))      return "flow content (<div>, <p>, <table>, text, …)";
    if (any(c & Cat::Metadata))  return "metadata content (<title>, <meta>, <link>)";
    if (any(c & Cat::TableCell)) return "<td> and <th>";
    if (any(c & Cat::TableRow))  return "<tr>";
    if (any(c & Cat::TableSect)) return "<thead>, <tbody>, <tfoot>, <tr>";
    if (any(c & Cat::ListItem))  return "<li>";
    if (any(c & Cat::HtmlSect))  return "<head> and <body>";
    return "no children";
}

// "waya: <div> is not permitted inside <p>. <p> permits phrasing content..."
template <Tag Parent, Tag Child>
consteval auto nesting_error() {
    FixedStr<512> s;
    s.append("waya: <");         s.append(Traits<Child>::name);
    s.append("> is not permitted inside <");
    s.append(Traits<Parent>::name);
    s.append(">. The HTML5 content model for <");
    s.append(Traits<Parent>::name);
    s.append("> permits ");      s.append(describe(Traits<Parent>::permits));
    s.append(". See https://html.spec.whatwg.org/#the-");
    s.append(Traits<Parent>::name);
    s.append("-element");
    return s;
}

template <Tag T>
consteval auto void_error() {
    FixedStr<256> s;
    s.append("waya: <"); s.append(Traits<T>::name);
    s.append("> is a void element and cannot have children.");
    return s;
}

template <Tag T>
consteval auto text_error() {
    FixedStr<384> s;
    s.append("waya: text is not permitted inside <");
    s.append(Traits<T>::name);
    s.append(">. It permits "); s.append(describe(Traits<T>::permits));
    s.append(".");
    return s;
}

template <Tag T>
consteval auto attr_error(std::string_view attr) {
    FixedStr<256> s;
    s.append("waya: the '"); s.append(attr);
    s.append("' attribute is not valid on <"); s.append(Traits<T>::name);
    s.append(">.");
    return s;
}

// Text nodes have no Tag; report them by name.
template <typename C> consteval std::string_view child_name() {
    return "text";
}

} // namespace detail

// ── Factories ───────────────────────────────────────────────────────────────
// The factories ACCEPT any child, then diagnose inside the body. This is the
// difference between a 37-line overload-resolution dump and a 1-line sentence.

// Reports a child's tag for the diagnostic; text nodes report as `text`.
template <typename C> struct TagOf { static constexpr bool has = false; };
template <Tag T, ElemCfg Cfg, typename... Cs>
struct TagOf<ElemNode<T, Cfg, Cs...>> {
    static constexpr bool has = true;
    static constexpr Tag  value = T;
};

// Fires exactly one static_assert naming both elements.
//
// Two deliberate tricks keep GCC's output to a single line:
//   1. The assert condition is a plain `bool` constant, NOT the concept. If we
//      assert on `PermittedChild<...>` directly, GCC helpfully appends the
//      whole concept-satisfaction derivation.
//   2. The check is triggered by instantiating a TYPE, not by calling a
//      constexpr function, which avoids "in 'constexpr' expansion of ..."
//      frames naming every node in the tree.

template <Tag Parent, typename Child,
          bool Ok = PermittedChild<Parent, Child>,
          bool IsElem = TagOf<Child>::has>
struct CheckChild {
    static_assert(Ok, detail::nesting_error<Parent, TagOf<Child>::value>());
    static constexpr bool value = true;
};

// Text (and other tagless nodes) in a slot that permits none.
template <Tag Parent, typename Child>
struct CheckChild<Parent, Child, false, false> {
    static_assert(sizeof(Child) == 0, detail::text_error<Parent>());
    static constexpr bool value = true;
};

// Satisfied cases: nothing to report.
template <Tag Parent, typename Child, bool IsElem>
struct CheckChild<Parent, Child, true, IsElem> {
    static constexpr bool value = true;
};

template <Tag T, ElemCfg Cfg = ElemCfg{}, typename... Cs>
constexpr auto make(Cs... cs) {
    static_assert(!Traits<T>::is_void, detail::void_error<T>());
    // Reading ::value forces instantiation of each checker, which is what
    // fires the static_assert — without adding constexpr-expansion frames.
    static_assert((CheckChild<T, Cs>::value && ...));
    return ElemNode<T, Cfg, Cs...>{{}, {std::move(cs)...}};
}

// Void elements: constrained to take no children at all.
template <Tag T, ElemCfg Cfg = ElemCfg{}>
    requires (Traits<T>::is_void)
constexpr auto make_void() {
    return ElemNode<T, Cfg>{};
}

[[nodiscard]] constexpr TextNode text(std::string_view s) { return {s}; }

namespace dsl {

// Non-void element factories. Unconstrained on purpose — `make` diagnoses.
#define WAYA_FACTORY(NAME, TAG)                                             \
    template <typename... Cs>                                               \
    constexpr auto NAME(Cs... cs) {                                         \
        return make<Tag::TAG>(std::move(cs)...);                            \
    }

WAYA_FACTORY(html_,  html)
WAYA_FACTORY(head_,  head)
WAYA_FACTORY(title_, title)
WAYA_FACTORY(body_,  body)
WAYA_FACTORY(div_,   div)
WAYA_FACTORY(p_,     p)
WAYA_FACTORY(span_,  span)
WAYA_FACTORY(a_,     a)
WAYA_FACTORY(h1_,    h1)
WAYA_FACTORY(h2_,    h2)
WAYA_FACTORY(h3_,    h3)
WAYA_FACTORY(ul_,    ul)
WAYA_FACTORY(ol_,    ol)
WAYA_FACTORY(li_,    li)
WAYA_FACTORY(table_, table)
WAYA_FACTORY(thead_, thead)
WAYA_FACTORY(tbody_, tbody)
WAYA_FACTORY(tr_,    tr)
WAYA_FACTORY(td_,    td)
WAYA_FACTORY(th_,    th)
WAYA_FACTORY(button_,button)
WAYA_FACTORY(label_, label)
WAYA_FACTORY(form_,  form)
#undef WAYA_FACTORY

// Void elements — passing children produces a named diagnostic, not an
// arity error.
template <typename... Cs> constexpr auto br_(Cs...) {
    static_assert(sizeof...(Cs) == 0, detail::void_error<Tag::br>());
    return make_void<Tag::br>();
}
template <typename... Cs> constexpr auto hr_(Cs...) {
    static_assert(sizeof...(Cs) == 0, detail::void_error<Tag::hr>());
    return make_void<Tag::hr>();
}
template <typename... Cs> constexpr auto img_(Cs...) {
    static_assert(sizeof...(Cs) == 0, detail::void_error<Tag::img>());
    return make_void<Tag::img>();
}

// ── Attribute pipes (maya's operator| pattern) ──────────────────────────────

template <Str S> struct ClsTag {};
template <Str S> struct IdTag {};
template <Str S> struct HrefTag {};

template <Str S> inline constexpr ClsTag<S>  cls{};
template <Str S> inline constexpr IdTag<S>   id_{};
template <Str S> inline constexpr HrefTag<S> href{};

namespace detail_cfg {
    consteval ElemCfg with_cls(ElemCfg c)  { c.has_cls  = true; return c; }
    consteval ElemCfg with_id(ElemCfg c)   { c.has_id   = true; return c; }
    consteval ElemCfg with_href(ElemCfg c) { c.has_href = true; return c; }
}

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, ClsTag<S>) {
    Attrs a = n.attrs;
    a.cls = S.view();
    return ElemNode<T, detail_cfg::with_cls(Cfg), Cs...>{a, std::move(n.children)};
}

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, IdTag<S>) {
    Attrs a = n.attrs;
    a.id = S.view();
    return ElemNode<T, detail_cfg::with_id(Cfg), Cs...>{a, std::move(n.children)};
}

// TYPE-STATE: `href` is only valid on elements that accept it. Piping href
// onto a <span> is a compile error — the direct analogue of maya's
// `requires (Cfg.has_border)` gating border colour.
template <Tag T>
concept AcceptsHref = (T == Tag::a || T == Tag::link);

template <Tag T, bool Ok = AcceptsHref<T>>
struct CheckHref {
    static_assert(Ok, detail::attr_error<T>("href"));
    static constexpr bool value = true;
};
template <Tag T> struct CheckHref<T, true> { static constexpr bool value = true; };

template <Tag T, ElemCfg Cfg, typename... Cs, Str S>
constexpr auto operator|(ElemNode<T, Cfg, Cs...> n, HrefTag<S>) {
    static_assert(CheckHref<T>::value);
    Attrs a = n.attrs;
    a.href = S.view();
    return ElemNode<T, detail_cfg::with_href(Cfg), Cs...>{a, std::move(n.children)};
}

} // namespace dsl

// ── Escaping ────────────────────────────────────────────────────────────────

constexpr void escape_html_into(std::string& out, std::string_view s) {
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += c;        break;
        }
    }
}

// ── Rendering ───────────────────────────────────────────────────────────────
// constexpr-friendly so whole pages can be rendered at compile time.

constexpr void render_into(std::string& out, const TextNode& t) {
    escape_html_into(out, t.s);
}

template <Str S>
constexpr void render_into(std::string& out, const StaticTextNode<S>&) {
    escape_html_into(out, S.view());
}

template <Tag T, ElemCfg Cfg, typename... Cs>
constexpr void render_into(std::string& out, const ElemNode<T, Cfg, Cs...>& e) {
    out += '<';
    out += Traits<T>::name;
    if constexpr (Cfg.has_id) {
        out += " id=\"";
        escape_html_into(out, e.attrs.id);
        out += '"';
    }
    if constexpr (Cfg.has_cls) {
        out += " class=\"";
        escape_html_into(out, e.attrs.cls);
        out += '"';
    }
    if constexpr (Cfg.has_href) {
        out += " href=\"";
        escape_html_into(out, e.attrs.href);
        out += '"';
    }
    out += '>';
    if constexpr (!Traits<T>::is_void) {
        std::apply([&](const auto&... cs) { (render_into(out, cs), ...); },
                   e.children);
        out += "</";
        out += Traits<T>::name;
        out += '>';
    }
}

template <typename N>
[[nodiscard]] constexpr std::string render(const N& n) {
    std::string out;
    render_into(out, n);
    return out;
}

template <typename N>
[[nodiscard]] constexpr std::string render_document(const N& n) {
    std::string out = "<!DOCTYPE html>";
    render_into(out, n);
    return out;
}

} // namespace waya
