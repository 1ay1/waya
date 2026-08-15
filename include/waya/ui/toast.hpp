#pragma once
/// \file ui/toast.hpp
/// Toasts as MODEL STATE — a notification queue with a lifecycle.
///
/// `toast()` (in components.hpp) draws one card. But a real notification system
/// is a QUEUE: messages arrive, stack, auto-dismiss after a timeout, and can be
/// dismissed early by the user. Hand-rolled that's a vector plus per-item timer
/// bookkeeping plus id juggling — fiddly, and easy to leak a timer or dismiss
/// the wrong one.
///
/// `Toasts` makes the queue one value in your model. You `push` onto it, `tick`
/// it on a clock, and `dismiss` by id; `toasts_layer(t, DismissMsg{})` renders
/// the stack. Every toast is addressable by a stable id, so dismissal and
/// auto-expiry target exactly the right card even as the list reorders.
///
///   struct Model { Toasts notes; };
///
///   // update:
///   [&](Saved)          { m.notes.push("Saved!", Tone::success); return {m, Cmd::none()}; }
///   [&](Dismiss d)      { m.notes.dismiss(d.id);                  return {m, Cmd::none()}; }
///   [&](Tick)           { m.notes.tick(100ms);                    return {m, Cmd::none()}; }
///
///   // subscribe: run the clock ONLY while a toast is alive
///   static Sub<Msg> subscribe(const Model& m){
///       return m.notes.any() ? Sub<Msg>::every(100, Tick{}) : Sub<Msg>::none();
///   }
///
///   // view: one call renders the fixed top-right stack
///   overlay(main_ui, toasts_layer(m.notes, [](int id){ return Dismiss{id}; }))

#include "../surface/node.hpp"
#include "components.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// One live notification. `id` is stable for its whole life (for dismiss +
/// keyed diffing); `remaining_ms` counts down to auto-expiry (0 = sticky).
/// An OPTIONAL `action_label` turns the card into an actionable notification
/// ("Undo", "View", "Retry") — the app supplies the Msg at render time via
/// `toasts_layer`'s `onAction`, so the token stays valid inside the render's
/// msg-capture scope (it can't be registered here in update()).
struct Toast {
    int id = 0;
    std::string message;
    Tone tone = Tone::neutral;
    double remaining_ms = 0;    // time left until auto-expiry
    bool sticky = false;        // true => never auto-dismisses (dismiss-only)
    std::string action_label;   // "" => no action button; else the button's text
    bool operator==(const Toast&) const = default;

    [[nodiscard]] bool has_action() const { return !action_label.empty(); }
};

/// The notification queue — a plain value in your model.
struct Toasts {
    std::vector<Toast> items;
    int next_id = 1;            // monotonic, so ids never collide across a session
    bool operator==(const Toasts&) const = default;

    /// Enqueue a toast; returns its id (so you can dismiss it programmatically).
    /// `ttl` of 0 makes it sticky (dismiss-only). Default 4s auto-dismiss.
    int push(std::string message, Tone tone = Tone::neutral,
             std::chrono::milliseconds ttl = std::chrono::milliseconds{4000}){
        int id = next_id++;
        bool sticky = ttl.count() <= 0;
        items.push_back({ id, std::move(message), tone, (double)ttl.count(), sticky, {} });
        return id;
    }
    /// Enqueue an ACTIONABLE toast — it carries a labelled button ("Undo",
    /// "Retry", "View") alongside the close. Actionable toasts default to
    /// STICKY (an action the user might take shouldn't vanish mid-reach); pass
    /// a non-zero `ttl` to auto-dismiss anyway. Wire the Msg via
    /// `toasts_layer(..., onAction)`.
    int push_action(std::string message, std::string action_label, Tone tone = Tone::neutral,
                    std::chrono::milliseconds ttl = std::chrono::milliseconds{0}){
        int id = next_id++;
        bool sticky = ttl.count() <= 0;
        items.push_back({ id, std::move(message), tone, (double)ttl.count(), sticky, std::move(action_label) });
        return id;
    }
    /// Sugar for the common tones.
    int success(std::string m, std::chrono::milliseconds ttl = std::chrono::milliseconds{4000}){ return push(std::move(m), Tone::success, ttl); }
    int error(std::string m, std::chrono::milliseconds ttl = std::chrono::milliseconds{6000}){ return push(std::move(m), Tone::danger, ttl); }
    int info(std::string m, std::chrono::milliseconds ttl = std::chrono::milliseconds{4000}){ return push(std::move(m), Tone::primary, ttl); }

    /// Remove a toast by id (a user hitting its close button, or a programmatic
    /// dismiss). No-op if it already expired.
    void dismiss(int id){
        for (auto it = items.begin(); it != items.end(); ++it)
            if (it->id == id){ items.erase(it); return; }
    }
    /// Advance every non-sticky toast's clock; drop the ones that hit zero.
    /// Call on your `Sub::every` tick.
    void tick(std::chrono::milliseconds dt){
        double d = (double)dt.count();
        for (auto& t : items) if (!t.sticky){ t.remaining_ms -= d; if (t.remaining_ms < 0) t.remaining_ms = 0; }
        std::erase_if(items, [](const Toast& t){ return !t.sticky && t.remaining_ms <= 0; });
    }
    /// Clear the whole queue.
    void clear(){ items.clear(); }

    /// True while any toast is on screen — drive the tick clock only then, so
    /// an app with no notifications costs zero frames.
    [[nodiscard]] bool any() const { return !items.empty(); }
    /// True while any toast is still counting down (a sticky-only queue can be
    /// non-empty yet need no clock).
    [[nodiscard]] bool ticking() const {
        for (auto& t : items) if (!t.sticky) return true;
        return false;
    }
    [[nodiscard]] std::size_t size() const { return items.size(); }
};

namespace toast_detail {
/// Build one toast card. `action` is an already-built button node (or nullptr).
inline NodeRef card(const Toast& t, NodeRef close, NodeRef action){
    std::vector<NodeRef> parts;
    parts.push_back(dot(t.tone));
    parts.push_back(text(t.message) | fg_text | text_size(14));
    parts.push_back(box() | grows);
    if (action) parts.push_back(std::move(action));
    parts.push_back(std::move(close));
    auto rowc = box(); rowc->kids = std::move(parts); rowc->style.flow = Flow::row; finalize(*rowc);
    return rowc
        | gap(10) | items_center | pad_x(16) | pad_y(12) | round(12)
        | detail::raw_css("background","var(--wa-surface, #141b2e)")
        | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("box-shadow","0 10px 30px rgba(0,0,0,.4)")
        | detail::raw_css("pointer-events","auto") | slide_in(220)
        | key("toast-" + std::to_string(t.id))
        | role("status") | aria("live", "polite");
}
inline NodeRef close_btn_for(int id, int tok){
    auto c = text("\xc3\x97")                    // ×
        | fg_muted | text_size(16) | pointer
        | pad_x(4) | aria_label("Dismiss notification") | role("button");
    c->on_tap = tok;
    (void)id;
    return c;
}
inline NodeRef stack(std::vector<NodeRef> cards){
    auto n = box(); n->kids = std::move(cards); n->style.flow = Flow::col;
    auto& s = n->style;
    s.pos = Pos::fixed;
    s.top = {16,Unit::px}; s.right = {16,Unit::px};
    s.has_z = true; s.z = 1100; s.gap = {10,Unit::px};
    s.extra.emplace_back("pointer-events", "none");
    finalize(*n);
    return n | aria("live", "polite");
}
} // namespace toast_detail

/// `toasts_layer(queue, onDismiss)` — render the queue as the fixed top-right
/// stack. Each card carries a close button wired to `onDismiss(id)`. Keyed by
/// toast id so the diff moves/removes cards precisely as the queue changes.
template <typename ToMsg>
inline NodeRef toasts_layer(const Toasts& q, ToMsg onDismiss){
    std::vector<NodeRef> cards;
    cards.reserve(q.items.size());
    for (const auto& t : q.items)
        cards.push_back(toast_detail::card(t,
            toast_detail::close_btn_for(t.id, detail::register_msg(onDismiss(t.id))), nullptr));
    return toast_detail::stack(std::move(cards));
}

/// `toasts_layer(queue, onDismiss, onAction)` — same stack, but an actionable
/// toast (pushed via `push_action`) also renders a labelled button wired to
/// `onAction(id)`. Non-actionable toasts render exactly as the two-arg form.
template <typename ToMsg, typename OnAction>
inline NodeRef toasts_layer(const Toasts& q, ToMsg onDismiss, OnAction onAction){
    std::vector<NodeRef> cards;
    cards.reserve(q.items.size());
    for (const auto& t : q.items){
        NodeRef action = nullptr;
        if (t.has_action()){
            auto [accent, _] = impl::tone_colors(t.tone); (void)_;
            action = box(text(t.action_label)
                        | text_size(13) | semibold)
                | detail::raw_css("color", accent) | pointer | pad_x(10) | pad_y(5) | round(7)
                | role("button") | aria_label(t.action_label)
                | detail::raw_css("white-space","nowrap")
                | hover_bg(0xffffff, 0.08f);
            action->on_tap = detail::register_msg(onAction(t.id));
        }
        cards.push_back(toast_detail::card(t,
            toast_detail::close_btn_for(t.id, detail::register_msg(onDismiss(t.id))),
            std::move(action)));
    }
    return toast_detail::stack(std::move(cards));
}

} // namespace waya::ui
