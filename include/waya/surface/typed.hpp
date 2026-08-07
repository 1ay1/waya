#pragma once
/// \file typed.hpp
/// Type-state style gates \u2014 waya's signature "impossible CSS doesn't compile"
/// layer, the exact transpose of maya's `requires (Cfg.has_border)` border rule
/// onto the box model.
///
/// The problem: `gap`, `justify`, `align`, `grow` only mean anything inside a
/// flex/grid container; `width`/`height` are ignored on an inline element. In
/// plain CSS (and in the untyped `Mod` layer) these are silent no-ops \u2014 the #1
/// class of real CSS bug. Here they are a COMPILE ERROR with a one-line, waya-
/// authored message.
///
///   row(a, b) | gap(12)        // OK   \u2014 row is a flex context
///   text("x") | gap(12)        // ERROR: gap requires a flex/grid container
///   grid(a) | justify(center)  // OK
///   box(a)  | grow(1)          // ERROR: grow only applies to a flex child
///
/// HOW. A builder returns a `Styled<Ctx>` \u2014 a NodeRef plus a phantom `Ctx` tag
/// (Block / Flex / Grid / Inline) describing the layout context it establishes.
/// A gated mod (`Gate<Req>`) carries the context it REQUIRES. `styled | gate`
/// is constrained: it `static_assert`s that `Ctx` satisfies `Req`, then applies.
/// The tag is phantom \u2014 zero runtime cost, erased to a bare NodeRef on demand.
///
/// This layer is OPT-IN. `#include <waya/surface/typed.hpp>` and use the typed
/// builders (`Row`/`Col`/`Grid`/`Box`/`Text`) to get the gates; the untyped
/// `row`/`col`/... stay exactly as they were for code that doesn't want them.

#include "node.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

namespace waya::tui {

// The typed dialect. Open ONLY this namespace (`using namespace waya::tui`) for
// a complete, gated vocabulary: the six context-sensitive mods below are
// compile-time gated; every other mod is re-exported verbatim from the surface
// vocabulary via using-DECLARATIONS (which name a specific entity and so never
// collide with the gated definitions — unlike a using-directive).

// value types + builders used inside typed views
using waya::surface::Node;   using waya::surface::NodeRef;   using waya::surface::Style;
using waya::surface::Mod;     using waya::surface::Len;
using waya::surface::Justify; using waya::surface::Align;     using waya::surface::Wrap;
using waya::surface::Flow;    using waya::surface::Weight;    using waya::surface::Cursor;
using waya::surface::Pos;     using waya::surface::Unit;
using waya::surface::sty;     using waya::surface::finalize;
using waya::surface::px;      using waya::surface::pct;       using waya::surface::rem;
using waya::surface::vw;      using waya::surface::vh;
using waya::surface::row;     using waya::surface::col;       using waya::surface::grid;
using waya::surface::stack;   using waya::surface::box;        using waya::surface::text;
using waya::surface::image;   using waya::surface::path;

// the untyped vocabulary that ISN'T context-gated — re-exported as-is.
using waya::surface::fg;      using waya::surface::bg;
using waya::surface::pad;     using waya::surface::pad_x;      using waya::surface::pad_y;
using waya::surface::margin;  using waya::surface::round;      using waya::surface::border;
using waya::surface::w;       using waya::surface::h;          using waya::surface::size;
using waya::surface::font;    using waya::surface::weight;     using waya::surface::bold;
using waya::surface::semibold;using waya::surface::medium;    using waya::surface::italic;
using waya::surface::center;  using waya::surface::between;
using waya::surface::pointer; using waya::surface::opacity;    using waya::surface::shadow;
using waya::surface::css;     using waya::surface::attr;       using waya::surface::tap;
using waya::surface::on;      using waya::surface::on_input;   using waya::surface::on_change;
using waya::surface::leading; using waya::surface::tracking;   using waya::surface::truncate;
using waya::surface::line_clamp; using waya::surface::uppercase; using waya::surface::lowercase;
using waya::surface::capitalize; using waya::surface::tabular_nums;
using waya::surface::no_select;  using waya::surface::no_pointer;
using waya::surface::min_w;   using waya::surface::max_w;      using waya::surface::min_h;
using waya::surface::max_h;   using waya::surface::aspect;
using waya::surface::transition; using waya::surface::hover_lift; using waya::surface::press;
using waya::surface::overload;   using waya::surface::round;      using waya::surface::pad_x;
using waya::surface::pad_y;
using waya::surface::sticky;     using waya::surface::sticky_top; using waya::surface::sticky_bottom;
using waya::surface::fixed;      using waya::surface::absolute;   using waya::surface::relative;
using waya::surface::positioned; using waya::surface::pin;        using waya::surface::inset;
using waya::surface::top;        using waya::surface::bottom;     using waya::surface::left;
using waya::surface::right;      using waya::surface::z;
using waya::surface::pin_top_right;   using waya::surface::pin_top_left;
using waya::surface::pin_bottom_right; using waya::surface::pin_bottom_left;
using waya::surface::scroll;     using waya::surface::scroll_x;   using waya::surface::scroll_y;
using waya::surface::clip;       using waya::surface::no_scrollbar; using waya::surface::overflow;
using waya::surface::translate;  using waya::surface::rotate;     using waya::surface::scale;
using waya::surface::backdrop_blur; using waya::surface::blur;    using waya::surface::opacity;
using waya::surface::line_clamp; using waya::surface::uppercase;  using waya::surface::lowercase;
using waya::surface::on_key;     using waya::surface::on_enter;   using waya::surface::on_escape;
using waya::surface::on_shortcut;using waya::surface::hotkey;     using waya::surface::on_keydown;
using waya::surface::sr_only;    using waya::surface::autofocus;  using waya::surface::focus_ring;
using waya::surface::focus_within; using waya::surface::aria_label; using waya::surface::role;
using waya::surface::aria;       using waya::surface::group;      using waya::surface::group_hidden;
using waya::surface::ripple;     using waya::surface::name;       using waya::surface::placeholder;
using waya::surface::key;        using waya::surface::animated;   using waya::surface::memo;
using waya::surface::component;  using waya::surface::tap;        using waya::surface::disabled;

// ── Layout contexts (phantom tags) ──────────────────────────────────────────
namespace ctx {
struct Block  {};   // a normal block box: width/height/padding apply; no gap/justify
struct Flex   {};   // a flex container: gap/justify/align/wrap apply to it
struct Grid   {};   // a grid container: gap/justify/align + grid-template apply
struct Inline {};   // inline text: width/height are ignored; typography applies
} // namespace ctx

// ── Capabilities a context provides (type-theoretic predicates) ───────────
// Each requirement is a TAG type with a `satisfied_by<Ctx>` predicate. A context
// satisfies a requirement when the tag says so. This keeps the gate a plain
// type parameter (no template-template gymnastics) and reads declaratively.
namespace req {
/// gap/justify/align/wrap: needs a container that lays out children (flex OR grid).
struct Container {
    template <typename C> static constexpr bool satisfied_by =
        std::same_as<C, ctx::Flex> || std::same_as<C, ctx::Grid>;
    static constexpr const char* need = "a flex or grid container (use Row/Col/Grid)";
};
/// grow/shrink: a child inside a container.
struct FlexItem {
    template <typename C> static constexpr bool satisfied_by =
        std::same_as<C, ctx::Flex> || std::same_as<C, ctx::Grid>;
    static constexpr const char* need = "a child of a flex/grid container";
};
/// width/height/padding/box-model: any non-inline box.
struct BoxModel {
    template <typename C> static constexpr bool satisfied_by = !std::same_as<C, ctx::Inline>;
    static constexpr const char* need = "a box element (not inline text)";
};
} // namespace req

// concept forms, for readable call-site constraints
template <typename C> concept FlexContext = std::same_as<C, ctx::Flex>;
template <typename C> concept GridContext = std::same_as<C, ctx::Grid>;
template <typename C> concept ContainerContext = req::Container::satisfied_by<C>;

// ── The typed node handle ───────────────────────────────────────────────────
/// A NodeRef tagged with the layout context it establishes. Convertible to a
/// plain NodeRef (the tag is erased), so it drops into any untyped API and into
/// a children pack transparently.
template <typename Ctx>
struct Styled {
    NodeRef node;
    using context = Ctx;

    /// erase the tag \u2014 hand a plain NodeRef to the runtime / untyped builders.
    operator NodeRef() const { return node; }
    Node& operator*()  const { return *node; }
    Node* operator->() const { return node.get(); }
};

template <typename Ctx> Styled<Ctx> as_styled(NodeRef n) { return Styled<Ctx>{ std::move(n) }; }

// ── Gated mods ───────────────────────────────────────────────────────
/// A Mod tagged with the requirement `Req` (a req:: tag) it needs of its
/// context. Applied via the constrained operator| below.
template <typename Req>
struct Gate { Mod mod; };

template <typename Req>
Gate<Req> gated(Mod m) { return Gate<Req>{ std::move(m) }; }

// ── The constrained pipe ────────────────────────────────────────
// A plain Mod on a Styled node applies in any context and preserves the tag.
template <typename Ctx>
Styled<Ctx> operator|(Styled<Ctx> s, const Mod& m) {
    m.apply(*s.node);
    finalize(*s.node);
    return s;
}

// A gated Mod: this overload is viable ONLY when the context satisfies the
// requirement. When it does, apply and preserve the tag.
template <typename Ctx, typename Req>
    requires (Req::template satisfied_by<Ctx>)
Styled<Ctx> operator|(Styled<Ctx> s, const Gate<Req>& g) {
    g.mod.apply(*s.node);
    finalize(*s.node);
    return s;
}

// The diagnostic overload: viable only when the requirement is NOT met, so it
// wins by being the sole candidate and fires a readable static_assert naming
// exactly what context the style needs.
template <typename Ctx, typename Req>
    requires (!Req::template satisfied_by<Ctx>)
Styled<Ctx> operator|(Styled<Ctx> s, const Gate<Req>&) {
    static_assert(Req::template satisfied_by<Ctx>,
        "waya: this style needs a different layout context. "
        "gap/justify/align/wrap need a flex or grid container (Row/Col/Grid); "
        "grow/shrink need a child of one. You applied it to a Box/Text.");
    return s;
}

// ── Typed builders ─────────────────────────────────────────────────
// Each returns a Styled<Ctx> carrying the context it establishes. Children are
// taken as anything convertible to NodeRef (incl. other Styled<...>), so typed
// and untyped trees compose freely.
template <typename... Cs> Styled<ctx::Flex>  Row(Cs... cs)  { return as_styled<ctx::Flex>(row(NodeRef(cs)...)); }
template <typename... Cs> Styled<ctx::Flex>  Col(Cs... cs)  { return as_styled<ctx::Flex>(col(NodeRef(cs)...)); }
template <typename... Cs> Styled<ctx::Grid>  Grid(Cs... cs) { return as_styled<ctx::Grid>(grid(NodeRef(cs)...)); }
// A ZStack renders `display:grid` (one overlaid cell), so its context is Grid,
// not Flex — tagging it Flex was a type-lie about the layout it establishes.
template <typename... Cs> Styled<ctx::Grid>  Stack(Cs... cs){ return as_styled<ctx::Grid>(stack(NodeRef(cs)...)); }
template <typename... Cs> Styled<ctx::Block> Box(Cs... cs)  { return as_styled<ctx::Block>(box(NodeRef(cs)...)); }
inline Styled<ctx::Inline> Text(std::string s){ return as_styled<ctx::Inline>(text(std::move(s))); }
inline Styled<ctx::Inline> Text(long long v)  { return as_styled<ctx::Inline>(text(v)); }

// ── Gated mods ─ ONLY the container properties (which apply to the node ITSELF,
// whose context the type system knows). gap/justify/align/wrap on a plain Box or
// Text are meaningless — so they're a compile error here.
//
// NOTE grow/shrink are deliberately NOT gated: they're flex-ITEM properties that
// depend on the PARENT being a container, which a child's own type can't see (a
// child is always Block/Inline regardless of parent). CSS ignores flex-grow on a
// non-item harmlessly, so gating on the child's context would be wrong. They're
// re-exported plain from the surface vocabulary.
using waya::surface::grow;
using waya::surface::shrink;

// gap takes a plain number (px) or a unit variant — NOT a surface::Len, whose
// namespace would drag the untyped surface::gap into ADL and clash. This keeps
// the gated gap the sole candidate.
inline Gate<req::Container> gap(float v){ return gated<req::Container>(sty([=](Style& s){ s.gap=px(v); })); }
inline Gate<req::Container> gap(int v)  { return gated<req::Container>(sty([=](Style& s){ s.gap=px((float)v); })); }
inline Gate<req::Container> gap_rem(float v){ return gated<req::Container>(sty([=](Style& s){ s.gap=rem(v); })); }
inline Gate<req::Container> gap_em (float v){ return gated<req::Container>(sty([=](Style& s){ s.gap={v,Unit::em}; })); }

// Container alignment as NAMED gate values (no enum argument, so no ADL pulls in
// the untyped surface::justify). Reads like maya's named style values.
namespace detail_gate {
    inline Gate<req::Container> j(Justify v){ return gated<req::Container>(sty([=](Style& s){ s.justify=v; })); }
    inline Gate<req::Container> a(Align v){ return gated<req::Container>(sty([=](Style& s){ s.align=v; })); }
}
inline const Gate<req::Container> justify_start   = detail_gate::j(Justify::start);
inline const Gate<req::Container> justify_center  = detail_gate::j(Justify::center);
inline const Gate<req::Container> justify_end     = detail_gate::j(Justify::end);
inline const Gate<req::Container> justify_between = detail_gate::j(Justify::between);
inline const Gate<req::Container> justify_around  = detail_gate::j(Justify::around);
inline const Gate<req::Container> justify_evenly  = detail_gate::j(Justify::evenly);
inline const Gate<req::Container> align_start     = detail_gate::a(Align::start);
inline const Gate<req::Container> align_center    = detail_gate::a(Align::center);
inline const Gate<req::Container> align_end       = detail_gate::a(Align::end);
inline const Gate<req::Container> align_stretch   = detail_gate::a(Align::stretch);
inline const Gate<req::Container> align_baseline  = detail_gate::a(Align::baseline);
inline const Gate<req::Container> wrap_on         = gated<req::Container>(sty([](Style& s){ s.wrap=Wrap::wrap; }));
inline const Gate<req::Container> wrap_off        = gated<req::Container>(sty([](Style& s){ s.wrap=Wrap::nowrap; }));

} // namespace waya::tui
