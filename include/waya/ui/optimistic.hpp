#pragma once
/// \file ui/optimistic.hpp
/// Optimistic<T> — show a change instantly, roll back if the server rejects it.
///
/// The responsive-UI pattern: when a user toggles a like, sends a message, or
/// reorders a list, you don't want to wait for the server round-trip to show
/// the result. You apply the change LOCALLY at once (optimistically), fire the
/// request, and — if it fails — roll back to what was really committed.
///
/// Hand-rolled that means keeping two copies (the confirmed value and the
/// in-flight guess) plus a "pending" flag, and remembering to restore the old
/// one on error. `Optimistic<T>` makes it one value that does the bookkeeping:
///
///   struct Model { Optimistic<bool> liked{false}; };
///
///   // update:
///   [&](Like)      { m.liked.apply(!m.liked.value());          // show it NOW
///                    return {m, postLike(m.liked.value())}; }
///   [&](LikeOk)    { m.liked.confirm();     return {m, Cmd::none()}; }  // it stuck
///   [&](LikeFail)  { m.liked.rollback();    return {m, Cmd::none()}; }  // undo it
///
///   // view: read the (possibly-optimistic) value; pending() dims it
///   heart | (m.liked.value() ? filled : outline)
///         | (m.liked.pending() ? opacity(.6f) : Mod{})
///
/// `value()` returns the optimistic guess while in flight, the committed value
/// otherwise — so the view always reads the right thing with one accessor.
/// `confirm()` promotes the guess to committed; `rollback()` discards it.

#include <utility>

namespace waya::ui {

/// A value with an optional in-flight optimistic overlay.
template <typename T>
struct Optimistic {
    T committed{};              // the last server-confirmed value
    T pending_value{};          // the optimistic guess (valid only while pending_)
    bool pending_ = false;      // a change is applied locally, awaiting the server

    constexpr Optimistic() = default;
    constexpr explicit Optimistic(T v) : committed(std::move(v)) {}

    bool operator==(const Optimistic&) const = default;

    /// The value to SHOW: the optimistic guess while in flight, else committed.
    [[nodiscard]] const T& value() const { return pending_ ? pending_value : committed; }
    /// The value the server has actually confirmed (ignores any in-flight guess).
    [[nodiscard]] const T& settled() const { return committed; }
    /// True while a change is applied locally but not yet confirmed.
    [[nodiscard]] bool pending() const { return pending_; }

    /// Apply `guess` optimistically — `value()` returns it immediately. Pair
    /// with the Cmd that asks the server to make it real.
    void apply(T guess){ pending_value = std::move(guess); pending_ = true; }
    /// The server accepted it: promote the guess to committed truth.
    void confirm(){ if (pending_){ committed = std::move(pending_value); pending_ = false; } }
    /// The server accepted a DIFFERENT authoritative value (e.g. a normalised
    /// or server-computed result) — take it as truth, clearing the guess.
    void confirm(T authoritative){ committed = std::move(authoritative); pending_ = false; }
    /// The server rejected it: discard the guess, snap back to committed.
    void rollback(){ pending_ = false; }
    /// Force a committed value with no pending change (a fresh load).
    void set(T v){ committed = std::move(v); pending_ = false; }
};

} // namespace waya::ui
