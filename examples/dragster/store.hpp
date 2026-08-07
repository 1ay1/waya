/// dragster/store.hpp — the game model for DRAGSTER (Activision, 1980).
///
/// A drag race: the whole game is ~6 seconds long. Stage on the line, wait out
/// the christmas-tree countdown (shift too early = a red-light FOUL), then
/// feather the throttle to keep RPM high but UNDER the redline while shifting up
/// through 4 gears. Over-rev too long and the engine BLOWS. The score is your
/// elapsed time — lower is better (the real-world record is 5.57s).
///
/// Pure data + the Msg set; all logic lives in store.cpp.
#pragma once

#include <waya/surface/effect.hpp>
#include <cstdint>
#include <utility>
#include <variant>

namespace dr {

using namespace waya::surface;

inline constexpr int   STRIP_LEN   = 100;   // finish line, in "feet" units
inline constexpr int   GEARS       = 4;     // shift up through 4 gears
inline constexpr int   REDLINE     = 100;   // tach tops out here (0..100)
inline constexpr int   BLOW_FRAMES = 18;    // frames over redline before it blows
inline constexpr int   TACH_ZONES  = 20;    // tachometer LED segments

enum class Phase {
    Ready,      // attract / staged, waiting for Start
    Count,      // christmas-tree countdown (shifting here = foul)
    Race,       // green light — go!
    Blown,      // engine grenaded (over-revved)
    Fouled,     // red-lit (shifted during the countdown)
    Finished,   // crossed the line — show the time
};

struct Model {
    Phase phase = Phase::Ready;

    // drivetrain
    int   gear   = 0;           // 0 = neutral/staged, 1..GEARS
    int   rpm    = 0;           // 0..REDLINE+ (over REDLINE risks a blow)
    int   over   = 0;           // consecutive frames spent past the redline
    bool  gas    = false;       // throttle currently held

    // race state
    int   pos     = 0;          // distance down the strip, 0..STRIP_LEN
    int   opp_pos = 0;          // the opponent's distance (lane 2)
    int   speed   = 0;          // current speed (derived from gear+rpm)
    int   count   = 0;          // countdown frames remaining (Count phase)
    long  frame   = 0;          // global tick counter
    long  elapsed = 0;          // race frames since green (the score, /FPS)

    // records (best = lowest finishing time in frames; 0 = none yet)
    long  best_frames = 0;

    static Model staged();      // a fresh staged car (store.cpp)
};

// ── messages ─────────────────────────────────────────────────────────────
struct Tick    {};              // the game loop
struct GasTog  {};              // toggle the throttle on/off (hold w/ Space)
struct Shift   {};              // shift up one gear
struct Start   {};              // stage / begin the countdown / restart

using Msg = std::variant<Tick, GasTog, Shift, Start>;

std::pair<Model, Cmd<Msg>> update(Model m, Msg msg);   // in store.cpp

// helpers shared with the view
double       secs(long frames);           // frames -> seconds (store.cpp)
const char*  phase_word(Phase p);         // short status word

} // namespace dr
