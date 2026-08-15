#pragma once
/// \file ui/motion.hpp
/// Easing + interpolation math for MODEL-OWNED animation.
///
/// waya has two animation paths, and they're for different jobs:
///
///   • CSS transitions/keyframes (`transition()`, `spin`, `breathe`, `animate`)
///     — for PURELY VISUAL motion the browser can own frame-by-frame: a hover
///     lift, a spinner, a fade-in. You describe the end state; CSS tweens it.
///     Prefer these — they cost zero server frames.
///
///   • This file — for animation the MODEL owns: a value your `update` drives
///     and your `view` reads. A game position easing toward a target, a gauge
///     that springs, a progress that decelerates, a staggered list reveal. The
///     browser can't tween these because only the server knows the target; you
///     step them on a `Sub::every` tick and the diff ships the delta.
///
/// This is maya's `anim` math (easing curves, lerp, a tween value) transposed
/// to waya's server-owned loop. It's PURE — `ease::out_cubic(0.5)` is a
/// function, `Tween` is a value with `==` — so an animated model stays testable
/// and the whole thing is header-only and allocation-free.
///
///   struct Model { Tween x{0}; };                    // 0 = home
///   // update:
///   [&](Open) { m.x.to(1.0, 300ms); return {m, Cmd::none()}; }
///   [&](Tick) { m.x.step(16ms);     return {m, Cmd::none()}; }
///   // subscribe: run a clock only WHILE something is animating
///   static Sub<Msg> subscribe(const Model& m){
///       return m.x.animating() ? Sub<Msg>::every(16, Tick{}) : Sub<Msg>::none();
///   }
///   // view:
///   panel | translate(0, (1.0 - m.x.value()) * -20)   // slide down as x→1

#include <chrono>
#include <cmath>
#include <cstdint>

namespace waya::ui {

// ── easing curves ────────────────────────────────────────────────────────────
// Pure functions [0,1] → [0,1] (some overshoot for "back"/"elastic"). The names
// mirror the CSS/Penner vocabulary so a curve reads the same on both paths.
namespace ease {

using Fn = double(*)(double);

inline double clamp01(double t){ return t < 0 ? 0 : (t > 1 ? 1 : t); }

inline double linear(double t){ return t; }

inline double in_quad(double t){ return t*t; }
inline double out_quad(double t){ return 1 - (1-t)*(1-t); }
inline double in_out_quad(double t){ return t < 0.5 ? 2*t*t : 1 - std::pow(-2*t+2, 2)/2; }

inline double in_cubic(double t){ return t*t*t; }
inline double out_cubic(double t){ double u = 1-t; return 1 - u*u*u; }
inline double in_out_cubic(double t){ return t < 0.5 ? 4*t*t*t : 1 - std::pow(-2*t+2, 3)/2; }

inline double in_quart(double t){ return t*t*t*t; }
inline double out_quart(double t){ double u = 1-t; return 1 - u*u*u*u; }

inline double in_expo(double t){ return t == 0 ? 0 : std::pow(2, 10*t - 10); }
inline double out_expo(double t){ return t == 1 ? 1 : 1 - std::pow(2, -10*t); }

/// Smoothstep — the natural ease-in-out for a phase in [0,1].
inline double smoothstep(double t){ t = clamp01(t); return t*t*(3 - 2*t); }

/// `out_back` overshoots past 1 then settles — the "pop" for entrances.
inline double out_back(double t){
    const double c1 = 1.70158, c3 = c1 + 1;
    double u = t - 1;
    return 1 + c3*u*u*u + c1*u*u;
}
/// `out_elastic` — a springy, decaying oscillation to 1.
inline double out_elastic(double t){
    if (t == 0 || t == 1) return t;
    const double c4 = (2*3.14159265358979323846) / 3;
    return std::pow(2, -10*t) * std::sin((t*10 - 0.75) * c4) + 1;
}
/// A gentle bounce landing (a ball settling).
inline double out_bounce(double t){
    const double n1 = 7.5625, d1 = 2.75;
    if (t < 1/d1)        return n1*t*t;
    else if (t < 2/d1){  t -= 1.5/d1;  return n1*t*t + 0.75; }
    else if (t < 2.5/d1){t -= 2.25/d1; return n1*t*t + 0.9375; }
    else {               t -= 2.625/d1; return n1*t*t + 0.984375; }
}

} // namespace ease

// ── lerp ─────────────────────────────────────────────────────────────────────
/// Linear interpolation. `lerp(a, b, t)` — t in [0,1]. Works for any numeric
/// type; overloaded for the common float/double/int-position cases.
inline double lerp(double a, double b, double t){ return a + (b - a) * t; }
inline float  lerp(float a, float b, double t){ return a + (b - a) * (float)t; }

/// `eased(a, b, t, curve)` — lerp with a curve applied to t. `eased(0, 100,
/// p, ease::out_cubic)` decelerates from 0 to 100 as p goes 0→1.
inline double eased(double a, double b, double t, ease::Fn curve){
    return lerp(a, b, curve(ease::clamp01(t)));
}

// ── Tween — a model-owned animated scalar ────────────────────────────────────
/// A value that eases from where it is toward a target over a duration. YOU own
/// it (it's in your Model) and step it on a tick; it never touches a clock or a
/// frame loop itself — that keeps it pure and testable, and lets `subscribe`
/// decide when to run the clock (only while `animating()`).
///
/// The design mirrors maya's Motion but inverts control: on the web the server
/// drives frames, so Tween is a passive value the update loop steps, not a
/// self-ticking object. `to()` continues from the LIVE value (no jump mid-flight).
struct Tween {
    double from = 0, target = 0, elapsed_ms = 0, dur_ms = 0;
    ease::Fn curve = ease::out_cubic;

    constexpr Tween() = default;
    constexpr explicit Tween(double v) : from(v), target(v) {}

    bool operator==(const Tween& o) const {
        return from==o.from && target==o.target && elapsed_ms==o.elapsed_ms
            && dur_ms==o.dur_ms && curve==o.curve;
    }

    /// Retarget over `dur`, continuing from the current value (no visual jump).
    /// A zero/negative duration snaps.
    void to(double t, std::chrono::milliseconds dur, ease::Fn c = ease::out_cubic){
        from = value();            // continue from where we are
        target = t;
        dur_ms = (double)dur.count();
        elapsed_ms = 0;
        curve = c ? c : ease::linear;
        if (dur_ms <= 0){ from = target; }
    }
    /// Jump instantly, killing motion.
    void snap(double v){ from = target = v; elapsed_ms = dur_ms = 0; }

    /// Advance by a frame's worth of time. Call on your `Sub::every` tick.
    void step(std::chrono::milliseconds dt){
        if (!animating()) return;
        elapsed_ms += (double)dt.count();
        if (elapsed_ms >= dur_ms) elapsed_ms = dur_ms;
    }

    /// The current, eased value.
    [[nodiscard]] double value() const {
        if (dur_ms <= 0) return target;
        double p = ease::clamp01(elapsed_ms / dur_ms);
        return lerp(from, target, curve(p));
    }
    /// True while still in flight — drive the clock only when this is true.
    [[nodiscard]] bool animating() const { return dur_ms > 0 && elapsed_ms < dur_ms; }
    [[nodiscard]] double target_value() const { return target; }
};

// ── stagger — index-phased fan-out ───────────────────────────────────────────
/// `stagger(elapsed_ms, i, step_ms, dur_ms, curve)` — the eased [0,1] progress
/// of item `i` in a cascade where each item starts `step_ms` after the last
/// (list mount, menu open row-by-row). Clamp-safe at both ends.
///
///   for (int i = 0; i < n; ++i){
///       double p = stagger(m.since_open_ms, i, 40, 300);
///       rows.push_back(row(i) | opacity((float)p) | translate(0, (1-p)*8));
///   }
inline double stagger(double elapsed_ms, int index, double step_ms, double dur_ms,
                      ease::Fn curve = ease::out_cubic){
    double start = index * step_ms;
    double raw = dur_ms <= 0 ? 1 : (elapsed_ms - start) / dur_ms;
    return (curve ? curve : ease::linear)(ease::clamp01(raw));
}
/// True once every item in an N-stagger has finished (stop the clock).
inline bool stagger_done(double elapsed_ms, int count, double step_ms, double dur_ms){
    if (count <= 0) return true;
    return elapsed_ms >= (count - 1) * step_ms + dur_ms;
}

} // namespace waya::ui
