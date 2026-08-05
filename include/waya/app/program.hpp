#pragma once
/// \file program.hpp
/// The `Program` concept and the `overload` visitor helper — maya's Elm
/// architecture, ported to the web.
///
/// A Program is a value type describing an application:
///
///   Model  — plain state. No signals, no shared_ptr, no mutation.
///   Msg    — a closed sum type (std::variant). Every event is an alternative.
///
///   init()               -> std::pair<Model, Cmd<Msg>>   (or just Model)
///   update(Model, Msg)   -> std::pair<Model, Cmd<Msg>>   pure transition
///   view(const Model&)   -> Node                          pure rendering
///   subscribe(const Model&) -> Sub<Msg>                   (optional)
///
/// Output = Input + Effect. Effects are Cmd values, never performed directly.
/// The runtime interprets them. Because update is pure, you test it with `==`
/// and no server — see tests/test_program.cpp.

#include "cmd.hpp"
#include "sub.hpp"
#include "../dsl/node.hpp"

#include <concepts>
#include <utility>

namespace waya {

/// The classic overload helper for `std::visit(overload{...}, msg)`.
template <typename... Fs> struct overload : Fs... { using Fs::operator()...; };
template <typename... Fs> overload(Fs...) -> overload<Fs...>;

namespace detail {

template <typename P>
concept HasSimpleInit = requires {
    { P::init() } -> std::convertible_to<typename P::Model>;
};
template <typename P>
concept HasFullInit = requires {
    { P::init() } -> std::convertible_to<
        std::pair<typename P::Model, Cmd<typename P::Msg>>>;
};

template <typename P>
concept HasSubscribe = requires(const typename P::Model& m) {
    { P::subscribe(m) } -> std::convertible_to<Sub<typename P::Msg>>;
};

} // namespace detail

/// A well-formed waya application. `static_assert(Program<App>)` gives a precise
/// go/no-go before you wire it into the runtime.
template <typename P>
concept Program =
    requires { typename P::Model; typename P::Msg; }
    && (detail::HasSimpleInit<P> || detail::HasFullInit<P>)
    && requires(typename P::Model m, typename P::Msg msg) {
        { P::update(std::move(m), std::move(msg)) } -> std::convertible_to<
            std::pair<typename P::Model, Cmd<typename P::Msg>>>;
    }
    && requires(const typename P::Model& m) {
        { P::view(m) };   // returns something renderable (a node)
    };

// ── Uniform accessors so the runtime doesn't care which init/subscribe form ──

template <typename P>
[[nodiscard]] std::pair<typename P::Model, Cmd<typename P::Msg>> program_init() {
    if constexpr (detail::HasFullInit<P>)
        return P::init();
    else
        return { P::init(), Cmd<typename P::Msg>::none() };
}

template <typename P>
[[nodiscard]] Sub<typename P::Msg> program_subscribe(const typename P::Model& m) {
    if constexpr (detail::HasSubscribe<P>)
        return P::subscribe(m);
    else
        return Sub<typename P::Msg>::none();
}

} // namespace waya
