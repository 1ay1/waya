#pragma once
/// \file scale.hpp
/// Composition helpers for HUGE apps. The problem at scale: one `Model`, one
/// `Msg` enum, and one 3000-line `update` where every feature's messages collide
/// and nobody can find anything. The fix — the same one Elm/Redux use — is to
/// split the app into FEATURE MODULES, each owning:
///   • a slice of the shared Model (a struct field),
///   • a contiguous BLOCK of msg ids (so ids never collide),
///   • its own small `update` that only knows its slice.
///
/// waya makes the msg-block discipline a one-liner with `Msg` ranges and a
/// `combine(...)` reducer that routes each message to the owning feature. Your
/// top-level `update` becomes a table, not a monster:
///
///   enum : int { AuthBase = 100, CartBase = 200, FeedBase = 300 };
///
///   auto update(Model m, int msg, std::string v) {
///       return combine(std::move(m), msg, v,
///           feature(AuthBase, auth_update),   // handles 100..199
///           feature(CartBase, cart_update),   // handles 200..299
///           feature(FeedBase, feed_update));  // handles 300..399
///   }
///
/// Each feature's `update(Model&, int local_msg, value) -> Cmd<int>` sees msgs
/// RELATIVE to its base (0,1,2…), so a feature is written once and dropped in at
/// any base. No collisions, no giant switch, and each feature stays ~50 lines.

#include "effect.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace waya::surface {

/// One feature's registration: the msg-id block it owns and its reducer. The
/// reducer receives the message id RELATIVE to `base` (so it uses 0,1,2…), the
/// value, mutates the shared model in place, and returns any Cmd.
template <typename Model>
struct Feature {
    int base;                                                     ///< first msg id owned
    int span;                                                     ///< how many ids (default 100)
    std::function<Cmd<int>(Model&, int local, const std::string&)> update;
    bool owns(int msg) const { return msg >= base && msg < base + span; }
};

/// `feature(Base, reducer)` — register a feature owning `Base .. Base+span-1`.
/// `reducer(Model&, int local_msg, value) -> Cmd<int>`.
template <typename Model, typename Fn>
Feature<Model> feature(int base, Fn reducer, int span = 100) {
    return Feature<Model>{ base, span,
        [reducer = std::move(reducer)](Model& m, int local, const std::string& v) -> Cmd<int> {
            if constexpr (std::is_invocable_r_v<Cmd<int>, Fn, Model&, int, const std::string&>)
                return reducer(m, local, v);
            else if constexpr (std::is_invocable_r_v<Cmd<int>, Fn, Model&, int>)
                return reducer(m, local);
            else { reducer(m, local, v); return Cmd<int>::none(); }   // void reducer
        } };
}

/// `combine(model, msg, value, features…)` — route `msg` to the feature that
/// owns it (calling its reducer with the LOCAL msg id), returning the updated
/// model + its Cmd. An unowned msg is a no-op. This is the whole top-level
/// `update` of a big app: a flat list of features, each self-contained.
template <typename Model, typename... Fs>
std::pair<Model, Cmd<int>> combine(Model m, int msg, const std::string& value, Fs... features) {
    Cmd<int> cmd = Cmd<int>::none();
    bool handled = false;
    // fold over the features; the first that owns the msg handles it.
    ([&]{
        if (!handled && features.owns(msg)) {
            handled = true;
            cmd = features.update(m, msg - features.base, value);
        }
    }(), ...);
    return { std::move(m), std::move(cmd) };
}

/// `combine(model, msg, features…)` — value-less overload.
template <typename Model, typename... Fs>
std::pair<Model, Cmd<int>> combine(Model m, int msg, Fs... features) {
    return combine(std::move(m), msg, std::string{}, std::move(features)...);
}

} // namespace waya::surface
