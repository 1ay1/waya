#pragma once
/// \file timetravel.hpp
/// Time-travel debugging for waya Programs — record every step, then scrub the
/// timeline: step back/forward, jump to any point, replay a captured session,
/// and export a trace as a reproducible bug report.
///
/// This is almost free in waya's design: `update` is pure and effects are data,
/// so the ENTIRE history of an app is just the message log — replay it from
/// init() and you land in the exact same state, deterministically. No snapshots
/// of hidden mutable state, no heisenbugs.
///
///   #include <waya/surface/timetravel.hpp>
///   using namespace waya::surface;
///
///   auto tl = debug::timeline<Counter>();
///   tl.send(Counter::Inc{});
///   tl.send(Counter::Inc{});
///   tl.send(Counter::Dec{});
///   assert(tl.model().n == 1);
///
///   tl.back();                 // step to before the Dec — model().n == 2
///   tl.back();                 // n == 1
///   tl.jump(0);                // back to init()      — n == 0
///   tl.forward();              // replay the next step — n == 1
///
///   // capture a session and replay it elsewhere (bug reports, regression tests)
///   std::string trace = tl.export_trace();   // human-readable step log
///   auto view_at_step2 = tl.view_at(2);       // render any point in history
///
/// Every operation reconstructs state by REPLAYING the pure update from init()
/// up to the cursor, so a jump can never desync from what really happened.

#include "node.hpp"
#include "program.hpp"
#include "effect.hpp"
#include "diff.hpp"
#include "validate.hpp"

#include <string>
#include <vector>
#include <cstddef>

namespace waya::surface::debug {

/// The debugger: a message log plus a cursor into it. The model at any point is
/// derived by replaying update() from init() up to the cursor — never cached in
/// a way that could drift from the real computation.
template <typename P>
    requires SurfaceProgram<P>
class Timeline {
public:
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    /// One recorded interaction: the message, the input value it carried, and a
    /// short human label for the trace. (The resulting model is recomputed on
    /// demand, so nothing here can go stale.)
    struct Step {
        Msg         msg;
        std::string value;
        std::string label;   // optional annotation for export_trace()
    };

    Timeline() { rebuild(); }

    // ── recording ───────────────────────────────────────────────────────────

    /// Record + apply a Msg. If the cursor isn't at the end (you stepped back
    /// then acted), the "future" is truncated — a fresh branch, like an editor's
    /// undo history. Returns *this for chaining.
    Timeline& send(Msg msg, std::string value = {}, std::string label = {}) {
        if (cursor_ < log_.size()) log_.resize(cursor_);   // drop redone-away future
        log_.push_back({ std::move(msg), std::move(value), std::move(label) });
        cursor_ = log_.size();
        step_to(cursor_);
        return *this;
    }
    Timeline& send_all(std::vector<Msg> msgs) {
        for (auto& m : msgs) send(std::move(m));
        return *this;
    }

    // ── scrubbing ────────────────────────────────────────────────────────────

    /// Move the cursor to `n` (0 == init(), log_.size() == latest) and rebuild
    /// the model by replaying. Clamped to the valid range.
    Timeline& jump(std::size_t n) {
        cursor_ = n > log_.size() ? log_.size() : n;
        step_to(cursor_);
        return *this;
    }
    /// Step one message back toward init(). No-op at the start.
    Timeline& back() { if (cursor_ > 0) jump(cursor_ - 1); return *this; }
    /// Step one message forward (re-applying a stepped-back message). No-op at end.
    Timeline& forward() { if (cursor_ < log_.size()) jump(cursor_ + 1); return *this; }
    /// Jump all the way back to init().
    Timeline& reset() { return jump(0); }
    /// Jump to the latest recorded state.
    Timeline& latest() { return jump(log_.size()); }

    // ── inspection ────────────────────────────────────────────────────────────

    /// The model at the current cursor.
    const Model& model() const { return model_; }
    /// The Cmd the update at the cursor produced (Cmd::none() at init).
    const Cmd<Msg>& last_cmd() const { return last_cmd_; }
    /// Render the current model.
    NodeRef view() const { return P::view(model_); }
    /// Where the cursor sits (0..step_count).
    std::size_t cursor() const { return cursor_; }
    /// Total recorded steps.
    std::size_t step_count() const { return log_.size(); }
    /// Is the cursor at the very start / end?
    bool at_start() const { return cursor_ == 0; }
    bool at_end()   const { return cursor_ == log_.size(); }
    /// The recorded steps (for building a custom timeline UI).
    const std::vector<Step>& log() const { return log_; }

    /// Compute the model at an arbitrary point WITHOUT moving the cursor. Handy
    /// for rendering a preview of any frame in a debugger UI.
    Model model_at(std::size_t n) const {
        auto [m, cmd] = replay(n > log_.size() ? log_.size() : n);
        (void)cmd; return m;
    }
    /// Render the view at an arbitrary point without moving the cursor.
    NodeRef view_at(std::size_t n) const { return P::view(model_at(n)); }

    /// The surface patch between two points in history — exactly the delta the
    /// runtime would have streamed to the browser going from step `a` to `b`.
    /// Great for "what did this message actually change on screen?"
    Patch diff_between(std::size_t a, std::size_t b) const {
        return diff(view_at(a), view_at(b));
    }
    /// The patch the CURRENT step produced (from the previous state to now).
    Patch last_patch() const {
        return cursor_ == 0 ? Patch{} : diff_between(cursor_ - 1, cursor_);
    }

    // ── export / import (reproducible bug reports & regression tests) ─────────

    /// A human-readable trace: one line per step with its label + a validity
    /// marker, so a captured session reads like a script and drops into a bug.
    std::string export_trace() const {
        std::string o = "waya timeline (" + std::to_string(log_.size()) + " steps)\n";
        o += "  #0  init()" + std::string(verify(view_at(0)) ? "" : "   [INVALID VIEW]") + "\n";
        for (std::size_t i = 0; i < log_.size(); ++i) {
            o += "  #" + std::to_string(i + 1) + "  ";
            o += log_[i].label.empty() ? "msg" : log_[i].label;
            if (!log_[i].value.empty()) o += " = \"" + log_[i].value + "\"";
            if (!verify(view_at(i + 1))) o += "   [INVALID VIEW]";
            o += "\n";
        }
        o += "cursor at #" + std::to_string(cursor_) + "\n";
        return o;
    }

    /// Find the FIRST step whose rendered view fails structural validation, or
    /// step_count()+1 if the whole history is sound. Turns "somewhere in this
    /// 200-message session the UI broke" into an exact index.
    std::size_t first_invalid() const {
        for (std::size_t i = 0; i <= log_.size(); ++i)
            if (!verify(view_at(i))) return i;
        return log_.size() + 1;
    }

private:
    // Replay update() from init() up to `n` steps; returns (model, last cmd).
    std::pair<Model, Cmd<Msg>> replay(std::size_t n) const {
        auto [m, cmd] = detail::init_of<P, Model, Msg>();
        for (std::size_t i = 0; i < n && i < log_.size(); ++i) {
            auto r = detail::dispatch<P, Model, Msg>(std::move(m), log_[i].msg, log_[i].value);
            m = std::move(r.first);
            cmd = std::move(r.second);
        }
        return { std::move(m), std::move(cmd) };
    }
    void step_to(std::size_t n) {
        auto [m, cmd] = replay(n);
        model_ = std::move(m);
        last_cmd_ = std::move(cmd);
    }
    void rebuild() { cursor_ = 0; step_to(0); }

    std::vector<Step> log_;
    std::size_t       cursor_ = 0;
    Model             model_{};
    Cmd<Msg>          last_cmd_ = Cmd<Msg>::none();
};

/// `timeline<P>()` — construct a time-travel debugger (runs init()).
template <typename P>
Timeline<P> timeline() { return Timeline<P>{}; }

} // namespace waya::surface::debug
