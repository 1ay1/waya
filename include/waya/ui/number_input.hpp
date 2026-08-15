#pragma once
/// \file ui/number_input.hpp
/// Numeric form controls: `stepper`, `number_field`, and `star_rating`.
///
/// The core `input(...) | type("number")` is a bare numeric box. These are the
/// batteries: a −/value/+ spinner clamped to a range, a labelled numeric field
/// with min/max/step, and a click-to-set star rating. Each is a pure function
/// of a number and emits a Msg carrying the new value — either a bare Msg
/// (steppers/stars, which know the value they'd set) or a value→Msg mapper
/// (the text field, whose value comes from the DOM). Nothing mutates your
/// model; you parse + store in `update`.
///
///   struct Model { int qty = 1; double price = 9.99; int stars = 0; };
///   struct SetQty { int v; }; struct SetPrice { std::string v; }; struct Rate { int v; };
///
///   stepper(m.qty, [](int v){ return SetQty{v}; }, 0, 99)
///   number_field("Price", m.price, [](std::string v){ return SetPrice{v}; }, 0, 1e6, 0.01)
///   star_rating(m.stars, [](int v){ return Rate{v}; })
///
/// In update: `[&](SetQty s){ m.qty = s.v; ... }` — the stepper already clamped.

#include "../surface/node.hpp"
#include "../surface/sugar.hpp"
#include "components.hpp"
#include "icons.hpp"

#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── stepper — a −/value/+ spinner clamped to [min,max] ────────────────────────
/// `stepper(value, to_msg, min, max, step)` — a compact numeric spinner. The
/// `−` and `+` buttons deliver `to_msg(clamped(value ∓ step))`, so the value
/// never leaves the range and `update` just stores it. Disabled at the ends.
///
///   stepper(m.qty, [](int v){ return SetQty{v}; }, 0, 99)
template <typename ToMsg>
inline NodeRef stepper(long value, ToMsg to_msg, long min = 0, long max = 999, long step = 1) {
    long lo = value - step < min ? min : value - step;
    long hi = value + step > max ? max : value + step;
    bool at_lo = value <= min, at_hi = value >= max;

    auto btn = [&](const char* glyph, long target, bool off, std::string label_) {
        auto b = box(text(glyph) | text_size(16) | semibold)
            | w(30) | h(30) | round(8) | center | items_center
            | detail::raw_css("background", "var(--wa-raised, rgba(255,255,255,.06))")
            | detail::raw_css("user-select", "none")
            | aria_label(std::move(label_));
        if (off) return b | fg_muted | detail::raw_css("opacity", "0.4") | aria("disabled", "true");
        return b | fg_text | pointer | tap(to_msg(target))
             | hover_bg(0xffffff, 0.10f);
    };

    return row(
        btn("\xe2\x88\x92", lo, at_lo, "Decrease"),           // − (U+2212)
        box(text(std::to_string(value)) | fg_text | semibold)
            | min_w(44) | center | items_center
            | detail::raw_css("font-variant-numeric", "tabular-nums"),
        btn("+", hi, at_hi, "Increase"))
        | items_center | gap(6) | role("group") | aria_label("Number stepper");
}

// ── number_field — a labelled numeric input with min/max/step ─────────────────
/// `number_field(label, value, to_msg, min, max, step, hint)` — a themed
/// numeric text field. `to_msg` maps the live value string to a Msg (parse it
/// in `update`; the browser enforces min/max/step). Accepts int or floating
/// values via the templated `Num`.
template <typename Num, typename ToMsg>
inline NodeRef number_field(std::string label, Num value, ToMsg to_msg,
                            double min = 0, double max = 1e9, double step = 1,
                            std::string hint = "") {
    auto ctrl = input(detail::numstr((double)value)) | type("number") | input_skin()
        | attr("min", detail::numstr(min))
        | attr("max", detail::numstr(max))
        | attr("step", detail::numstr(step))
        | on_input(std::move(to_msg))
        | detail::raw_css("width", "100%") | detail::raw_css("box-sizing", "border-box");
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

// ── percent_field — a 0..100 field that shows a % suffix ──────────────────────
/// `percent_field(label, value, to_msg, hint)` — a number field constrained to
/// 0..100 with a trailing "%". Same value→Msg mapper contract.
template <typename ToMsg>
inline NodeRef percent_field(std::string label, double value, ToMsg to_msg, std::string hint = "") {
    auto ctrl = row(
        input(detail::numstr(value)) | type("number")
            | attr("min", "0") | attr("max", "100") | attr("step", "1")
            | on_input(std::move(to_msg))
            | bg_transparent() | detail::raw_css("border", "none")
            | detail::raw_css("outline", "none") | fg_text | grows
            | detail::raw_css("width", "100%"),
        text("%") | fg_muted)
        | items_center | gap(4) | input_skin();
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

// ── star_rating — click a star to set the score ───────────────────────────────
/// `star_rating(value, to_msg, out_of, size)` — `out_of` stars; the first
/// `value` are filled. Clicking the Nth star delivers `to_msg(N)`; clicking the
/// currently-set star again clears it (to 0). Read-only if `to_msg` is omitted
/// via the overload below.
template <typename ToMsg>
inline NodeRef star_rating(int value, ToMsg to_msg, int out_of = 5, float size = 22) {
    std::vector<NodeRef> stars;
    stars.reserve((std::size_t)out_of);
    for (int i = 1; i <= out_of; ++i) {
        bool on = i <= value;
        int target = (i == value) ? 0 : i;   // click the set star again → clear
        stars.push_back(
            box(icon("star", (int)size))
                | (on ? fg(0xfbbf24) : fg_muted)
                | (on ? Mod{} : detail::raw_css("opacity", "0.5"))
                | pointer | tap(to_msg(target))
                | detail::raw_css("transition", "transform .1s ease")
                | hover_lift(2)
                | aria_label(std::to_string(i) + " star" + (i == 1 ? "" : "s")));
    }
    auto rowc = box(); rowc->kids = std::move(stars); rowc->style.flow = Flow::row; finalize(*rowc);
    return rowc | items_center | gap(2) | role("radiogroup")
        | aria_label("Rating: " + std::to_string(value) + " of " + std::to_string(out_of));
}

/// Read-only star display (no interaction) — for showing an average score.
inline NodeRef stars(double value, int out_of = 5, float size = 18) {
    std::vector<NodeRef> items;
    items.reserve((std::size_t)out_of);
    for (int i = 1; i <= out_of; ++i) {
        bool on = (double)i <= value + 0.5;   // round to nearest for display
        items.push_back(box(icon("star", (int)size))
            | (on ? fg(0xfbbf24) : fg_muted)
            | (on ? Mod{} : detail::raw_css("opacity", "0.4")));
    }
    auto rowc = box(); rowc->kids = std::move(items); rowc->style.flow = Flow::row; finalize(*rowc);
    return rowc | items_center | gap(2) | role("img")
        | aria_label(detail::numstr(value) + " of " + std::to_string(out_of) + " stars");
}

} // namespace waya::ui
