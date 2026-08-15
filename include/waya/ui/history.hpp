#pragma once
/// \file ui/history.hpp
/// History<T> — undo / redo for any model value.
///
/// Undo is the feature every editor wants and nobody enjoys wiring: a stack of
/// past states, a stack of redo states, and the discipline to snapshot at the
/// right moments and clear redo on a fresh edit. `History<T>` is that machinery
/// as a value you wrap your (sub)model in.
///
///   struct Doc { std::string text; /* … */ };
///   struct Model { History<Doc> doc; };
///
///   // update: edit through push(); undo/redo walk the timeline
///   [&](Edit e) { m.doc.push(with_text(m.doc.get(), e.value)); return {m, Cmd::none()}; }
///   [&](Undo)   { m.doc.undo();  return {m, Cmd::none()}; }
///   [&](Redo)   { m.doc.redo();  return {m, Cmd::none()}; }
///
///   // view: read the present with get(); gate the buttons on can_undo/redo
///   editor(m.doc.get());
///   button("Undo", Undo{}) | when_(!m.doc.can_undo(), disabled());
///
/// `push` records the CURRENT present onto the past and makes the new value the
/// present, clearing any redo branch (the standard "editing after undo forks
/// the timeline" behaviour). A `limit` caps the past so a long session doesn't
/// grow without bound. It's a pure value with ==, so an undoable model stays
/// testable and time-travels cleanly.

#include <cstddef>
#include <utility>
#include <vector>

namespace waya::ui {

/// A value with undo/redo timelines. `T` must be copyable and equality-
/// comparable (for `==` on the whole History).
template <typename T>
struct History {
    T present{};
    std::vector<T> past;        // oldest … most-recent-before-present
    std::vector<T> future;      // states undone, most-recent-undo last
    std::size_t limit = 100;    // max past depth (0 = unbounded)

    constexpr History() = default;
    constexpr explicit History(T initial) : present(std::move(initial)) {}

    bool operator==(const History&) const = default;

    /// The current state (what the view renders).
    [[nodiscard]] const T& get() const { return present; }
    /// Mutable access WITHOUT recording history (a live drag you'll commit
    /// later). Prefer `push` for anything the user should be able to undo.
    [[nodiscard]] T& mut() { return present; }

    /// Commit `next` as the new present: the old present joins the past, and any
    /// redo branch is discarded (editing after undo forks the timeline). A `next`
    /// equal to the present is a no-op (don't clutter history with non-edits).
    void push(T next){
        if (next == present) return;
        past.push_back(std::move(present));
        present = std::move(next);
        future.clear();
        if (limit && past.size() > limit)
            past.erase(past.begin(), past.begin() + (past.size() - limit));
    }

    [[nodiscard]] bool can_undo() const { return !past.empty(); }
    [[nodiscard]] bool can_redo() const { return !future.empty(); }

    /// Step back: present -> future, last past -> present. No-op if empty.
    void undo(){
        if (past.empty()) return;
        future.push_back(std::move(present));
        present = std::move(past.back());
        past.pop_back();
    }
    /// Step forward: present -> past, last future -> present. No-op if empty.
    void redo(){
        if (future.empty()) return;
        past.push_back(std::move(present));
        present = std::move(future.back());
        future.pop_back();
    }

    /// Replace the present and WIPE both timelines (a fresh document load).
    void reset(T value){ present = std::move(value); past.clear(); future.clear(); }
    /// How many undo steps are available.
    [[nodiscard]] std::size_t depth() const { return past.size(); }
};

} // namespace waya::ui
