#pragma once
/// \file program.hpp
/// The Program — the "ideas" half of the runtime, with ZERO transport
/// dependency. A waya app is the Elm shape: a Model, a Msg, and pure functions
///
///   static Model init();                         // (or -> pair<Model,Cmd>)
///   static Model update(Model, Msg);             // (or +value, or -> pair<…,Cmd>)
///   static NodeRef view(const Model&);
///   static Sub<Msg> subscribe(const Model&);     // optional
///   static Meta meta(const Model&);              // optional
///
/// This header defines the `SurfaceProgram` concept, readable diagnostics, and
/// the small adapters that call an app's hooks regardless of which optional
/// shape it uses. None of it touches sockets, sessions, or the client — the
/// runtime (surface/live.hpp) drives these; a different backend could too. This
/// is the maya/Elm loop as pure data-flow, isolated from how it's served.

#include "node.hpp"
#include "effect.hpp"   // Cmd / Sub
#include "meta.hpp"     // Meta

#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

namespace waya::surface {

/// A Surface Program: Model + Msg + `view` (required), plus optional `init` /
/// `update` / `subscribe` / `meta`. `Msg` is the app's OWN type — typically a
/// `std::variant` of message structs (maya/Elm), carrying payloads and matched
/// with std::visit. The runtime maps each wired Msg to an opaque wire token
/// internally, so the app is fully type-safe; you never write an int message id.
template <typename P>
concept SurfaceProgram =
    requires { typename P::Model; typename P::Msg; }
    && requires(const typename P::Model& m) { { P::view(m) } -> std::convertible_to<NodeRef>; };

/// `check_program<P>()` — granular, readable diagnostics for a malformed Program.
/// The concept gives one opaque "constraint not satisfied"; these fire a specific
/// message naming EXACTLY which piece is wrong or misspelled. Called at the top
/// of live<P>() so a mistake is a one-liner, not a template wall. (A typo'd
/// `update` used to silently compile as "no update".)
template <typename P>
constexpr void check_program() {
    static_assert(requires { typename P::Model; },
        "waya: your Program needs a nested type `Model` (your app's state).");
    static_assert(requires { typename P::Msg; },
        "waya: your Program needs a nested type `Msg` (a variant of message structs).");
    static_assert(requires(const typename P::Model& m) { { P::view(m) } -> std::convertible_to<NodeRef>; },
        "waya: your Program needs `static NodeRef view(const Model&)`. Check the name, "
        "the const-ref parameter, and that it returns a NodeRef (col/row/box/text...).");
    static_assert(requires { { P::init() } -> std::convertible_to<typename P::Model>; }
               || requires { { P::init() } -> std::convertible_to<std::pair<typename P::Model, Cmd<typename P::Msg>>>; },
        "waya: your Program needs `static Model init()` (or `static std::pair<Model,Cmd<Msg>> init()`).");

    // `update` is optional (a static app has none), but if the app defines ANY
    // callable named `update`, it MUST match one of the four supported shapes.
    // Otherwise a typo’d signature (wrong param order, missing const, returning
    // the wrong type) silently compiles as “no update” and every message is a
    // no-op — a maddening bug. We detect “has some update” vs “has a VALID
    // update” and fire a targeted error on the mismatch.
    {
        using M = typename P::Model;
        using Msg = typename P::Msg;
        using C = Cmd<Msg>;
        constexpr bool has_any_update =
            requires(M m, Msg g, std::string v){ P::update(m, g, v); } ||
            requires(M m, Msg g){ P::update(m, g); };
        constexpr bool has_valid_update =
            requires(M m, Msg g){ { P::update(m, g) } -> std::convertible_to<M>; } ||
            requires(M m, Msg g){ { P::update(m, g) } -> std::convertible_to<std::pair<M, C>>; } ||
            requires(M m, Msg g, std::string v){ { P::update(m, g, v) } -> std::convertible_to<M>; } ||
            requires(M m, Msg g, std::string v){ { P::update(m, g, v) } -> std::convertible_to<std::pair<M, C>>; };
        static_assert(!has_any_update || has_valid_update,
            "waya: your `update` doesn't match a supported shape. It must be one of:\n"
            "  static Model update(Model, Msg);\n"
            "  static Model update(Model, Msg, std::string value);      // for input values\n"
            "  static std::pair<Model,Cmd<Msg>> update(Model, Msg);     // with effects\n"
            "  static std::pair<Model,Cmd<Msg>> update(Model, Msg, std::string value);\n"
            "Check the parameter order/types and the return type.");
    }
}

namespace detail {

/// Call P::update and return (Model, Cmd). Supports FOUR update shapes so apps
/// range from trivial to full effectful, and the runtime doesn't care which:
///   update(Model, Msg)                       → no value, no effects
///   update(Model, Msg, std::string value)    → value (inputs), no effects
///   update(Model, Msg)         -> (Model,Cmd) → effects
///   update(Model, Msg, string) -> (Model,Cmd) → value + effects
template <typename P, typename Model, typename Msg>
std::pair<Model, Cmd<Msg>> dispatch(Model m, Msg msg, const std::string& value){
    using C = Cmd<Msg>;
    if constexpr (requires(Model mm, Msg mg, std::string v){ { P::update(mm,mg,v) } -> std::convertible_to<std::pair<Model,C>>; }) {
        auto r = P::update(std::move(m), msg, value); return { std::move(r.first), std::move(r.second) };
    } else if constexpr (requires(Model mm, Msg mg, std::string v){ { P::update(mm,mg,v) } -> std::convertible_to<Model>; }) {
        return { P::update(std::move(m), msg, value), C::none() };
    } else if constexpr (requires(Model mm, Msg mg){ { P::update(mm,mg) } -> std::convertible_to<std::pair<Model,C>>; }) {
        auto r = P::update(std::move(m), msg); return { std::move(r.first), std::move(r.second) };
    } else {
        return { P::update(std::move(m), msg), C::none() };
    }
}

/// init returns (Model, Cmd) — supports init()->Model or ->(Model,Cmd).
template <typename P, typename Model, typename Msg>
std::pair<Model, Cmd<Msg>> init_of(){
    if constexpr (requires{ { P::init() } -> std::convertible_to<std::pair<Model,Cmd<Msg>>>; }) {
        auto r = P::init(); return { std::move(r.first), std::move(r.second) };
    } else return { P::init(), Cmd<Msg>::none() };
}

/// subscriptions — P::subscribe(Model)->Sub<Msg> if it exists, else none.
template <typename P, typename Model, typename Msg>
Sub<Msg> subs_of(const Model& m){
    if constexpr (requires{ { P::subscribe(m) } -> std::convertible_to<Sub<Msg>>; }) return P::subscribe(m);
    else return Sub<Msg>::none();
}

/// SEO metadata — P::meta(Model)->Meta if it exists, else a blank Meta (the
/// shell then uses its default title and index,follow robots).
template <typename P, typename Model>
Meta meta_of(const Model& m){
    if constexpr (requires{ { P::meta(m) } -> std::convertible_to<Meta>; }) return P::meta(m);
    else return Meta{};
}

} // namespace detail
} // namespace waya::surface
