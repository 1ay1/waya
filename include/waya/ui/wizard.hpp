#pragma once
/// \file ui/wizard.hpp
/// Wizard — a multi-step flow with per-step gating, as model state.
///
/// A checkout, an onboarding, a multi-page form: a sequence of steps where you
/// can only advance when the current step is valid, and you can go back. Apps
/// hand-roll that with an int + scattered "if step==2 && form.valid()" guards.
///
/// `Wizard` holds the current step index and total; `next`/`back` move within
/// bounds; and the VIEW gates the Next button on your own per-step validity so
/// the wizard itself stays agnostic about what "valid" means (you decide, from
/// your Form<> or whatever state each step owns).
///
///   struct Model { Wizard flow{3}; Form<> details; /* … */ };
///
///   // update: guard next() on the current step's validity
///   [&](Next) { if (step_valid(m)) m.flow.next(); return {m, Cmd::none()}; }
///   [&](Back) { m.flow.back();                    return {m, Cmd::none()}; }
///
///   // view: a progress header + the current step's body + nav
///   col(
///     wizard_steps(m.flow, {"Account","Details","Review"}),   // the progress bar
///     step_body(m),                                            // your switch on m.flow.current()
///     row(button("Back", Back{}) | when_(m.flow.is_first(), disabled()),
///         button(m.flow.is_last() ? "Finish" : "Next", Next{})
///             | when_(!step_valid(m), disabled())))
///
/// The wizard is a tiny value (index + count); the progress header is generated
/// from step labels. Gating lives in YOUR update, where the step's validity is.

#include "../surface/node.hpp"
#include "components.hpp"

#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// A step cursor over a fixed number of steps. One value in your model.
struct Wizard {
    int step = 0;       // current step (0-based)
    int count = 1;      // total steps

    constexpr Wizard() = default;
    constexpr explicit Wizard(int steps) : count(steps < 1 ? 1 : steps) {}

    bool operator==(const Wizard&) const = default;

    [[nodiscard]] int current() const { return step; }
    [[nodiscard]] bool is_first() const { return step <= 0; }
    [[nodiscard]] bool is_last() const { return step >= count - 1; }
    [[nodiscard]] bool is_complete() const { return step >= count; }   // stepped past the end
    /// Fractional progress 0..1 (for a progress bar).
    [[nodiscard]] float progress() const { return count <= 1 ? 1.f : (float)step / (count - 1); }

    /// Advance one step (clamped at the last). Call only after your gate passes.
    void next(){ if (step < count) ++step; }
    /// Go back one step (clamped at the first).
    void back(){ if (step > 0) --step; }
    /// Jump to a specific step (clamped) — for clickable progress dots.
    void go(int s){ step = s < 0 ? 0 : (s > count ? count : s); }
    void reset(){ step = 0; }
};

/// `wizard_steps(wizard, {labels})` — a horizontal progress header: a numbered
/// dot per step (filled when done/current), the label beneath, connected by a
/// rail. Purely presentational, generated from the wizard + labels.
inline NodeRef wizard_steps(const Wizard& w, std::vector<std::string> labels){
    std::vector<NodeRef> items;
    int n = (int)labels.size();
    for (int i = 0; i < n; ++i){
        bool done = i < w.step;
        bool cur  = i == w.step;
        bool active = done || cur;
        auto dot = box(text(done ? "\xe2\x9c\x93" : std::to_string(i + 1))   // ✓ or number
                       | (active ? fg(0xffffff) : fg_muted) | text_size(13) | semibold)
            | size(28) | round(999) | center
            | (active ? bg(0x6366f1) : detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.08))"))
            | (cur ? ring(rgba(0x6366f1, .35f), 4) : Mod{});
        auto label = text(labels[i]) | (active ? fg_text : fg_muted)
            | text_size(12) | (cur ? semibold : Mod{});
        auto cell = col(dot, label) | items_center | gap(6);
        items.push_back(cell);
        // a connector rail between steps (not after the last).
        if (i < n - 1)
            items.push_back(box() | h(2) | grows
                | (i < w.step ? bg(0x6366f1) : detail::raw_css("background","var(--wa-line, rgba(255,255,255,.12))"))
                | detail::raw_css("margin-top","13px")
                | detail::raw_css("margin-left","-6px") | detail::raw_css("margin-right","-6px"));
    }
    return row_(std::move(items)) | items_start | w_full | gap(4)
        | role("progressbar")
        | aria("valuenow", std::to_string(w.step + 1))
        | aria("valuemin", "1") | aria("valuemax", std::to_string(n));
}

} // namespace waya::ui
