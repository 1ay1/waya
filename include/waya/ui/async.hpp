#pragma once
/// \file ui/async.hpp
/// RemoteData<T> — the four states of a fetch, as ONE value.
///
/// Every screen that loads data over the network passes through the same four
/// states: not-asked-yet, loading, failed, loaded. Hand-rolled, that's a
/// scattered ladder of bools (`m.loading`, `m.error`, `m.data.empty()`) that's
/// easy to get into an illegal combination (loading AND error? data AND
/// loading?) and easy to render inconsistently across screens.
///
/// RemoteData makes those states a sum type — the maya/Elm move of "make
/// illegal states unrepresentable." You store ONE `RemoteData<T>` in your model
/// and transition it in `update`; you can't be loading and failed at once.
///
///   struct Model { RemoteData<std::vector<User>> users; };
///
///   // update:
///   [&](Load)          { m.users = loading(m.users);            return {m, fetchUsers()}; }
///   [&](Loaded r)      { m.users = r.ok() ? loaded(parse(r.body)) : failed<Users>(r.status_text()); }
///   [&](Retry)         { m.users = loading(m.users);            return {m, fetchUsers()}; }
///
///   // view: ONE exhaustive match — no `if(loading) … else if …` ladder.
///   remote(m.users,
///       /*loading*/ []                  { return centered(spinner()); },
///       /*loaded */ [](const Users& us) { return user_list(us); },
///       /*failed */ [](const std::string& e, auto retry){ return error_panel(e, retry); })
///
/// `remote(...)` also has a batteries-included overload that renders sensible
/// default loading / error / empty chrome, so the common case is one line:
///
///   remote(m.users, [](const Users& us){ return user_list(us); }, Retry{})
///
/// Retention: `loading(previous)` KEEPS the last-loaded value while refreshing,
/// so a re-poll shows stale-then-fresh instead of flashing a spinner over
/// content the user was reading (the "stale-while-revalidate" UX, for free).

#include "../surface/node.hpp"
#include "components.hpp"
#include "patterns.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace waya::ui {

using namespace waya::surface;

/// The four states of a remote resource. `T` is the loaded payload.
template <typename T>
struct RemoteData {
    struct NotAsked {};
    struct Loading  { std::optional<T> previous; };   // may retain the last value
    struct Failure  { std::string error; std::optional<T> previous; };
    struct Success  { T value; };
    std::variant<NotAsked, Loading, Failure, Success> state{NotAsked{}};

    bool operator==(const RemoteData&) const = default;

    bool is_not_asked() const { return std::holds_alternative<NotAsked>(state); }
    bool is_loading()   const { return std::holds_alternative<Loading>(state); }
    bool is_failure()   const { return std::holds_alternative<Failure>(state); }
    bool is_success()   const { return std::holds_alternative<Success>(state); }

    /// The loaded value if present (Success, or a retained previous), else null.
    const T* value() const {
        if (auto* s = std::get_if<Success>(&state)) return &s->value;
        if (auto* l = std::get_if<Loading>(&state)) return l->previous ? &*l->previous : nullptr;
        if (auto* f = std::get_if<Failure>(&state)) return f->previous ? &*f->previous : nullptr;
        return nullptr;
    }
    /// The error message if in a Failure state, else empty.
    std::string error() const {
        if (auto* f = std::get_if<Failure>(&state)) return f->error;
        return {};
    }
};

// ── transitions (return a fresh RemoteData; assign it in update) ─────────────
/// Move to Loading, RETAINING any previously-loaded value (stale-while-revalidate).
template <typename T>
RemoteData<T> loading(const RemoteData<T>& from = {}){
    std::optional<T> prev;
    if (auto* v = from.value()) prev = *v;
    return { typename RemoteData<T>::Loading{ std::move(prev) } };
}
/// Move to Success with the loaded value.
template <typename T>
RemoteData<T> loaded(T value){
    return { typename RemoteData<T>::Success{ std::move(value) } };
}
/// Move to Failure with an error message, retaining any prior value so the UI
/// can keep showing stale content behind the error.
template <typename T>
RemoteData<T> failed(std::string error, const RemoteData<T>& from = {}){
    std::optional<T> prev;
    if (auto* v = from.value()) prev = *v;
    return { typename RemoteData<T>::Failure{ std::move(error), std::move(prev) } };
}

// ── views ────────────────────────────────────────────────────────────────────
/// `remote(rd, onLoading, onSuccess, onFailure)` — the fully-custom exhaustive
/// match. Every state has a branch; the compiler makes you handle all of them.
/// `onFailure` receives the error string. NotAsked renders as Loading (the
/// natural "about to fetch" state).
template <typename T, typename OnLoading, typename OnSuccess, typename OnFailure>
NodeRef remote(const RemoteData<T>& rd, OnLoading onLoading, OnSuccess onSuccess, OnFailure onFailure){
    if (auto* s = std::get_if<typename RemoteData<T>::Success>(&rd.state))
        return onSuccess(s->value);
    if (auto* f = std::get_if<typename RemoteData<T>::Failure>(&rd.state))
        return onFailure(f->error);
    // NotAsked + Loading → the loading branch.
    return onLoading();
}

/// `remote(rd, onSuccess, retryMsg)` — the batteries-included overload. Renders
/// a centred spinner while loading, an `empty_state` error card with a Retry
/// button on failure, and `onSuccess(value)` when loaded. `retryMsg` is the Msg
/// dispatched by the error card's button. One line covers the common screen.
template <typename T, typename OnSuccess, typename RetryMsg>
NodeRef remote(const RemoteData<T>& rd, OnSuccess onSuccess, RetryMsg retryMsg){
    return remote(rd,
        /*loading*/ []{
            return box(spinner()) | w_full | pad(48) | center;
        },
        /*success*/ [&](const T& v){ return onSuccess(v); },
        /*failure*/ [&](const std::string& err){
            auto retry = button("Retry", retryMsg, Variant::secondary);
            return empty_state("Couldn't load", err.empty() ? "Something went wrong." : err, retry);
        });
}

} // namespace waya::ui
