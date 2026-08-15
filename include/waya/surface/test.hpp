#pragma once
/// \file test.hpp
/// A first-class unit-testing harness for waya Programs. Because a waya app is
/// pure (init/update/view with no I/O), you can drive the ENTIRE thing in a
/// plain test — no browser, no socket, no runtime. This header makes that a
/// one-liner instead of hand-plumbing `detail::dispatch`.
///
///   #include <waya/surface/test.hpp>
///   using namespace waya::surface;
///
///   auto app = test::harness<Counter>();   // runs init(), holds the Model
///   app.send(Counter::Inc{});              // dispatch a Msg (pure update)
///   assert(app.model().n == 1);
///   assert(app.text_contains("1"));        // render view() and query it
///   assert(app.count(Kind::button) == 2);  // structural queries
///
/// It also records the Cmd every update returns, so you can assert on effects
/// WITHOUT running them (Cmd is ==-comparable):
///
///   app.send(Counter::Save{});
///   assert(app.last_cmd() == Cmd<Msg>::after(300, Counter::Saved{}));
///
/// And you can drive the app the way a USER does — by the label on screen, not
/// the Msg. `click`/`fill` render the view, find the node you point at, and
/// dispatch the Msg it's WIRED to through the same path the live runtime uses.
/// So they catch the bug `send()` can't: a button wired to the wrong Msg, or
/// not wired at all.
///
///   app.click("Increment");                 // finds the button, fires its Msg
///   app.fill("ada@x.com", /*near=*/"Email");// types into the wired input
///   assert(app.can_click("Save"));          // is the Save button live?
///   // app.click("Nope") throws — a missing/unwired target fails loudly.
///
/// Everything here is header-only and dependency-free; drop it in any test.

#include "node.hpp"
#include "program.hpp"
#include "effect.hpp"
#include "validate.hpp"
#include "msg.hpp"
#include "dom.hpp"

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace waya::surface::test {

/// Walk a rendered tree, invoking `fn` on every node (pre-order).
inline void walk(const NodeRef& n, const std::function<void(const Node&)>& fn) {
    if (!n) return;
    fn(*n);
    for (auto& k : n->kids) walk(k, fn);
}

/// Collect the visible text of a tree (all `text` nodes, in order, space-joined).
inline std::string text_of(const NodeRef& n) {
    std::string out;
    walk(n, [&](const Node& nd) {
        if (nd.kind == Kind::text && !nd.text.empty()) {
            if (!out.empty()) out += ' ';
            out += nd.text;
        }
    });
    return out;
}

/// Count nodes of a given kind in a tree.
inline int count_kind(const NodeRef& n, Kind k) {
    int c = 0;
    walk(n, [&](const Node& nd) { if (nd.kind == k) ++c; });
    return c;
}

/// Find the first node whose visible text CONTAINS `needle` (a button/link by
/// its label). Used by `click("Save")`.
inline NodeRef find_text(const NodeRef& n, std::string_view needle) {
    NodeRef hit;
    std::function<void(const NodeRef&)> go = [&](const NodeRef& cur) {
        if (hit || !cur) return;
        // a text node's content, OR a button/link's own label (stored in ->text).
        bool holds_label = (cur->kind == Kind::text || cur->kind == Kind::button);
        if (holds_label && cur->text.find(needle) != std::string::npos) { hit = cur; return; }
        for (auto& k : cur->kids) go(k);
    };
    go(n);
    return hit;
}

/// Find the first node carrying a given key (keyed lists / addressable nodes).
inline NodeRef find_key(const NodeRef& n, std::string_view key) {
    NodeRef hit;
    std::function<void(const NodeRef&)> go = [&](const NodeRef& cur) {
        if (hit || !cur) return;
        if (cur->key == key) { hit = cur; return; }
        for (auto& k : cur->kids) go(k);
    };
    go(n);
    return hit;
}

/// Find the first node carrying a wired handler that satisfies `pred` — the
/// nearest ancestor of a label often holds the tap, so callers walk up.
inline NodeRef find_where(const NodeRef& n, const std::function<bool(const Node&)>& pred) {
    NodeRef hit;
    std::function<void(const NodeRef&)> go = [&](const NodeRef& cur) {
        if (hit || !cur) return;
        if (pred(*cur)) { hit = cur; return; }
        for (auto& k : cur->kids) go(k);
    };
    go(n);
    return hit;
}

/// True if `anc`'s subtree contains `target` (used to attribute a label's click
/// to the nearest tappable ancestor).
inline bool subtree_contains(const NodeRef& anc, const Node* target) {
    bool found = false;
    walk(anc, [&](const Node& nd){ if (&nd == target) found = true; });
    return found;
}

/// The harness: a live-runtime-free driver for one Program `P`. Holds the
/// current Model, replays init(), dispatches Msgs through the real update
/// (all four update shapes supported), and re-renders view() on demand.
template <typename P>
    requires SurfaceProgram<P>
class Harness {
public:
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    Harness() {
        auto [m, cmd] = detail::init_of<P, Model, Msg>();
        model_ = std::move(m);
        last_cmd_ = std::move(cmd);
    }

    /// Dispatch a Msg (no input value) through the real update; records its Cmd.
    Harness& send(Msg msg) { return send(std::move(msg), std::string{}); }

    /// Dispatch a Msg WITH an input value (as a wired input/change would send).
    Harness& send(Msg msg, std::string value) {
        auto [m, cmd] = detail::dispatch<P, Model, Msg>(std::move(model_), std::move(msg), value);
        model_ = std::move(m);
        last_cmd_ = std::move(cmd);
        return *this;
    }

    /// Dispatch a whole sequence in order (fluent scenario setup).
    Harness& send_all(std::vector<Msg> msgs) {
        for (auto& m : msgs) send(std::move(m));
        return *this;
    }

    /// The current model (mutable ref so tests can also poke it if needed).
    const Model& model() const { return model_; }
    Model&       model()       { return model_; }

    /// The Cmd the most recent update returned (Cmd::none() after init w/o cmd).
    const Cmd<Msg>& last_cmd() const { return last_cmd_; }

    /// Render the current model to a node tree (calls the app's real view()).
    NodeRef view() const { return P::view(model_); }

    // ── INTERACTION: drive the app the way a user does ──────────────────
    // `send(Msg)` tests update() in isolation. These test the WIRING too: they
    // render the view, find the node you point at, resolve the token it carries
    // back to a Msg through the SAME path the live runtime uses, and dispatch
    // that. So a button wired to the wrong Msg — or not wired at all — fails
    // here, which `send()` can never catch.

    /// Click the nearest tappable node at/above the element whose label CONTAINS
    /// `label`. Throws if no such node is wired — the test SHOULD fail loudly if
    /// the button it means to press isn't there or isn't clickable.
    Harness& click(std::string_view label) {
        auto [node, ok] = resolve_tap(label);
        if (!ok) throw std::runtime_error("harness.click: no wired tap target for label '" + std::string(label) + "'");
        return send(std::move(node), std::string{});
    }
    /// True if a wired, clickable target for `label` exists (assert without
    /// dispatching — “is the Save button live?”).
    bool can_click(std::string_view label) {
        return resolve_tap(label).second;
    }
    /// Type `value` into the first text input/textarea and fire its on_input
    /// (as a real keystroke would). Optional `near` narrows to the input whose
    /// placeholder/value contains that text, for multi-field forms.
    Harness& fill(std::string value, std::string_view near = {}) {
        auto [msg, ok] = resolve_input(value, near);
        if (!ok) throw std::runtime_error("harness.fill: no wired text input found");
        return send(std::move(msg), std::move(value));
    }

private:
    // Resolve the nearest tappable node containing `label` to its wired Msg.
    // Renders under a fresh capture so the token is live, then maps token->Msg
    // through the SAME resolve_msg the live runtime uses — so this exercises the
    // real wiring, not a shortcut.
    std::pair<Msg,bool> resolve_tap(std::string_view label) {
        detail::begin_msg_capture();
        NodeRef t = P::view(model_);
        NodeRef lbl = find_text(t, label);
        const Node* target = lbl ? lbl.get() : nullptr;
        int tok = -1;
        std::function<void(const NodeRef&)> go = [&](const NodeRef& cur){
            if (!cur) return;
            // the label's nearest tapped ancestor (deepest match wins), OR a
            // tapped node whose own text is the label.
            if (cur->on_tap >= 0 && ((target && (cur.get()==target || subtree_contains(cur, target)))
                                     || (!target && cur->text.find(label)!=std::string::npos)))
                tok = cur->on_tap;
            for (auto& k : cur->kids) go(k);
        };
        go(t);
        if (tok < 0) return { Msg{}, false };
        auto m = detail::resolve_msg<Msg>(tok, std::string{});
        if (!m) return { Msg{}, false };
        return { std::move(*m), true };
    }
    std::pair<Msg,bool> resolve_input(const std::string& value, std::string_view near) {
        detail::begin_msg_capture();
        NodeRef t = P::view(model_);
        int tok = -1;
        std::function<void(const NodeRef&)> go = [&](const NodeRef& cur){
            if (tok >= 0 || !cur) return;
            bool is_text_input = (cur->kind==Kind::input || cur->kind==Kind::textarea);
            bool matches = near.empty()
                || cur->placeholder.find(near)!=std::string::npos
                || cur->text.find(near)!=std::string::npos;
            if (is_text_input && cur->on_input>=0 && matches) { tok = cur->on_input; return; }
            for (auto& k : cur->kids) go(k);
        };
        go(t);
        if (tok < 0) return { Msg{}, false };
        auto m = detail::resolve_msg<Msg>(tok, value);
        if (!m) return { Msg{}, false };
        return { std::move(*m), true };
    }

public:

    // ── convenience queries over the freshly-rendered view ──────────────────
    std::string text() const { return text_of(view()); }
    bool text_contains(std::string_view needle) const {
        return text().find(needle) != std::string::npos;
    }
    int count(Kind k) const { return count_kind(view(), k); }
    NodeRef find_key(std::string_view key) const { return test::find_key(view(), key); }

    /// Assert the rendered view is structurally sound (WHATWG + waya rules).
    /// Returns "" when valid, or the violation report — assert on emptiness.
    std::string validate() const { return explain(view()); }
    bool valid() const { return verify(view()); }

private:
    Model model_{};
    Cmd<Msg> last_cmd_ = Cmd<Msg>::none();
};

/// `harness<P>()` — construct a Harness (runs init()). The function form reads
/// nicely at a call site: `auto app = test::harness<Counter>();`
template <typename P>
Harness<P> harness() { return Harness<P>{}; }

// ── testing a self-contained WIDGET (not a full Program) ─────────────────
/// A widget is `{ State, Msg, view(State)->NodeRef, update(State,Msg)->(State,
/// Cmd) }` — not a whole Program — so `Harness<P>` (which needs static
/// init/update/view) can't drive it without a wrapper. `WidgetHarness` holds the
/// widget's pieces as VALUES and gives the same by-label driving
/// (click/fill/send) + rendered-output queries, so a widget author unit-tests a
/// widget in isolation with zero Program boilerplate:
///
///   auto w = test::widget_harness(Ticker::State{},
///               &Ticker::view, &Ticker::update);
///   w.click("reset");                 // drive by its OWN rendered label
///   assert(w.state().n == 0);         // its own state updated
///   assert(w.text_contains("0"));     // its own view reflects it
///
/// The update must return `pair<State, Cmd<Msg>>` (the standard shape). Cmds are
/// recorded in `last_cmd()` so a widget's effects are assertable too.
template <typename State, typename Msg, typename ViewFn, typename UpdateFn>
class WidgetHarness {
public:
    WidgetHarness(State s, ViewFn v, UpdateFn u)
        : state_(std::move(s)), view_fn_(std::move(v)), update_fn_(std::move(u)) {}

    /// Dispatch a Msg (optionally with an input value) through the widget's
    /// real update; records the returned Cmd.
    WidgetHarness& send(Msg msg, std::string /*value*/ = {}) {
        auto [s, cmd] = update_fn_(std::move(state_), std::move(msg));
        state_ = std::move(s); last_cmd_ = std::move(cmd);
        return *this;
    }
    WidgetHarness& send_all(std::vector<Msg> msgs){ for (auto& m : msgs) send(std::move(m)); return *this; }

    /// Click the widget's node whose rendered label contains `label` — resolved
    /// through the SAME token→Msg path the live runtime uses.
    WidgetHarness& click(std::string_view label) {
        auto [msg, ok] = resolve_tap(label);
        if (!ok) throw std::runtime_error("widget_harness.click: no wired tap for label '" + std::string(label) + "'");
        return send(std::move(msg));
    }
    /// Type into the widget's (first matching) text field.
    WidgetHarness& fill(std::string value, std::string_view near = {}) {
        auto [msg, ok] = resolve_input(value, near);
        if (!ok) throw std::runtime_error("widget_harness.fill: no wired text input found");
        return send(std::move(msg), std::move(value));
    }

    const State& state() const { return state_; }
    State& state() { return state_; }
    const Cmd<Msg>& last_cmd() const { return last_cmd_; }
    NodeRef view() const { detail::begin_msg_capture(); return view_fn_(state_); }
    std::string text() const { return text_of(view()); }
    bool text_contains(std::string_view needle) const { return text().find(needle) != std::string::npos; }
    int count(Kind k) const { return count_kind(view(), k); }
    bool valid() const { return verify(view()); }
    std::string validate() const { return explain(view()); }

private:
    std::pair<Msg,bool> resolve_tap(std::string_view label) {
        detail::begin_msg_capture();
        NodeRef t = view_fn_(state_);
        NodeRef lbl = find_text(t, label);
        const Node* target = lbl ? lbl.get() : nullptr;
        int tok = -1;
        std::function<void(const NodeRef&)> go = [&](const NodeRef& cur){
            if (!cur) return;
            if (cur->on_tap >= 0 && ((target && (cur.get()==target || subtree_contains(cur, target)))
                                     || (!target && cur->text.find(label)!=std::string::npos)))
                tok = cur->on_tap;
            for (auto& k : cur->kids) go(k);
        };
        go(t);
        if (tok < 0) return { Msg{}, false };
        auto m = detail::resolve_msg<Msg>(tok, std::string{});
        return m ? std::pair<Msg,bool>{ std::move(*m), true } : std::pair<Msg,bool>{ Msg{}, false };
    }
    std::pair<Msg,bool> resolve_input(const std::string& value, std::string_view near) {
        detail::begin_msg_capture();
        NodeRef t = view_fn_(state_);
        int tok = -1;
        std::function<void(const NodeRef&)> go = [&](const NodeRef& cur){
            if (tok >= 0 || !cur) return;
            bool is_text_input = (cur->kind==Kind::input || cur->kind==Kind::textarea);
            bool matches = near.empty() || cur->placeholder.find(near)!=std::string::npos
                        || cur->text.find(near)!=std::string::npos;
            if (is_text_input && cur->on_input>=0 && matches) { tok = cur->on_input; return; }
            for (auto& k : cur->kids) go(k);
        };
        go(t);
        if (tok < 0) return { Msg{}, false };
        auto m = detail::resolve_msg<Msg>(tok, value);
        return m ? std::pair<Msg,bool>{ std::move(*m), true } : std::pair<Msg,bool>{ Msg{}, false };
    }

    State state_;
    ViewFn view_fn_;
    UpdateFn update_fn_;
    Cmd<Msg> last_cmd_ = Cmd<Msg>::none();
};

/// `widget_harness(state, view_fn, update_fn)` — drive a self-contained widget
/// by its rendered labels, no Program needed. `State` and `Msg` are deduced from
/// the widget's `update` signature (`pair<State,Cmd<Msg>>(State, Msg)`); pass the
/// widget's own `&Widget::view` and `&Widget::update`.
template <typename State, typename Msg, typename ViewFn>
auto widget_harness(State s, ViewFn v, std::pair<State,Cmd<Msg>>(*u)(State, Msg)) {
    return WidgetHarness<State, Msg, ViewFn, std::pair<State,Cmd<Msg>>(*)(State, Msg)>(
        std::move(s), std::move(v), u);
}

} // namespace waya::surface::test
