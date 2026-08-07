/// dragster/store.cpp — the game loop. Dragster is all about the tachometer:
/// hold the throttle to build RPM, but past the redline the engine starts to
/// grenade; shift up at the right moment to drop RPM into the power band and
/// gain speed. Every Tick (the 30fps server clock):
///   * Ready/Finished/Blown/Fouled: idle (waiting for Start).
///   * Count: run the christmas-tree countdown; a Shift here is a FOUL.
///   * Race: throttle raises rpm, no-throttle bleeds it; over-redline accrues
///     `over` frames until BLOW_FRAMES => Blown; speed follows gear*rpm; pos
///     advances by speed; crossing STRIP_LEN => Finished (record the time).
#include "store.hpp"

#include <waya/surface/sugar.hpp>
#include <algorithm>
#include <string>

namespace dr {

using namespace waya::surface;

inline constexpr int FPS = 30;

double secs(long frames) { return (double)frames / FPS; }

const char* phase_word(Phase p) {
    switch (p) {
        case Phase::Ready:    return "STAGED";
        case Phase::Count:    return "SET";
        case Phase::Race:     return "GO";
        case Phase::Blown:    return "BLOWN";
        case Phase::Fouled:   return "FOUL";
        case Phase::Finished: return "FINISH";
    }
    return "";
}

Model Model::staged() {
    Model m;
    m.phase = Phase::Ready;
    return m;
}

// The rpm the engine settles toward per gear when the throttle is floored. A
// higher gear pulls harder (more speed) but the rpm it can reach is lower, so
// you must shift up to keep climbing without blowing — the core Dragster loop.
static int target_rpm(int gear) {
    switch (gear) {
        case 1: return 118;   // 1st over-revs fast if you hold it
        case 2: return 112;
        case 3: return 108;
        case 4: return 104;
        default: return 0;    // neutral: no drive
    }
}

// Speed contributed at a given gear & rpm. Peak power sits just below redline;
// bogging (low rpm after a too-early shift) costs speed. Scaled so a clean run
// (shifting near the redline through all 4 gears) crosses the strip in ~6s at
// 30fps — in the spirit of the original's ~5.5–7s times.
static int gear_speed(int gear, int rpm) {
    if (gear <= 0) return 0;
    int band = REDLINE - std::abs(rpm - 85);   // power band centred ~85% redline
    if (band < 0) band = 0;
    return gear * 3 + band / 5;
}

std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
    return std::visit(overload{

        [&](Start) -> std::pair<Model, Cmd<Msg>> {
            // From any non-racing state, (re)stage and start the countdown.
            if (m.phase == Phase::Count || m.phase == Phase::Race)
                return {m, Cmd<Msg>::none()};
            long best = m.best_frames;
            m = Model::staged();
            m.best_frames = best;
            m.phase = Phase::Count;
            m.count = FPS * 2;          // ~2s christmas tree
            m.gear  = 0;
            return {m, Cmd<Msg>::none()};
        },

        [&](GasTog) -> std::pair<Model, Cmd<Msg>> {
            m.gas = !m.gas;
            return {m, Cmd<Msg>::none()};
        },

        [&](Shift) -> std::pair<Model, Cmd<Msg>> {
            if (m.phase == Phase::Count) {
                // Shifting before the green is a red-light foul (the infamous
                // rule the fake 5.51 record claimed to beat).
                m.phase = Phase::Fouled;
                return {m, Cmd<Msg>::none()};
            }
            if (m.phase != Phase::Race) return {m, Cmd<Msg>::none()};
            if (m.gear < GEARS) {
                m.gear++;
                // A clean upshift drops rpm into the next gear's band.
                m.rpm = std::max(40, m.rpm - 34);
                m.over = 0;
            }
            return {m, Cmd<Msg>::none()};
        },

        [&](Tick) -> std::pair<Model, Cmd<Msg>> {
            m.frame++;

            if (m.phase == Phase::Count) {
                if (--m.count <= 0) {
                    m.phase = Phase::Race;
                    m.gear  = 1;          // drop into first on the green
                    m.rpm   = 55;
                    m.elapsed = 0;
                }
                return {m, Cmd<Msg>::none()};
            }

            if (m.phase != Phase::Race) return {m, Cmd<Msg>::none()};

            m.elapsed++;

            // Throttle: floored -> climb toward the gear's target rpm; released
            // -> bleed rpm down. Neutral has no target so it just bleeds.
            int tgt = m.gas ? target_rpm(m.gear) : 0;
            if (m.rpm < tgt)      m.rpm = std::min(tgt, m.rpm + 6);
            else if (m.rpm > tgt) m.rpm = std::max(tgt, m.rpm - 4);

            // Redline / engine blow: sustained over-rev grenades the motor.
            if (m.rpm > REDLINE) {
                if (++m.over >= BLOW_FRAMES) { m.phase = Phase::Blown; return {m, Cmd<Msg>::none()}; }
            } else if (m.over > 0) {
                m.over--;
            }

            // Speed & distance. Distance accrues slowly so the quarter-mile
            // takes several seconds — the drama is in the tach, not raw pace.
            m.speed = gear_speed(m.gear, m.rpm);
            m.pos  += std::max(0, m.speed) / 20;

            if (m.pos >= STRIP_LEN) {
                m.pos = STRIP_LEN;
                m.phase = Phase::Finished;
                if (m.best_frames == 0 || m.elapsed < m.best_frames)
                    m.best_frames = m.elapsed;
            }
            return {m, Cmd<Msg>::none()};
        },

    }, msg);
}

} // namespace dr
