/// matrix/store.hpp — the Model + Msg + update. The whole animation is server
/// state advanced by a tick: rain columns fall, the terminal types out lines,
/// the breach meter climbs. Pure data + reducer.
#pragma once

#include <waya/surface/effect.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace mtx {

using namespace waya::surface;

// One falling column of glyphs.
struct Column {
    float head = 0;        // y position of the bright head (in cells)
    float speed = 1;       // cells per tick
    int   len = 10;        // trail length
    std::uint32_t seed = 0;// glyph randomness seed for this column
};

struct Model {
    // rain
    std::vector<Column> cols;
    long frame = 0;
    // terminal
    std::vector<std::string> log;   // fully-revealed lines
    std::string typing;             // the line currently being typed
    std::size_t type_pos = 0;       // chars revealed of the next scripted line
    int script_line = 0;            // index into the script
    // hud
    int breach = 0;                 // 0..100 breach progress
    int nodes_pwned = 0;
    bool running = true;
    int  alert = 0;                 // 0 nominal, 1 warning, 2 critical

    static Model boot();            // seed columns + reset terminal (in store.cpp)
};

// ── messages ─────────────────────────────────────────────────────────────
struct Tick   {};
struct Toggle {};                   // pause/resume
struct Reboot {};                   // restart the sequence
struct Escalate {};                 // bump alert level (a button)

using Msg = std::variant<Tick, Toggle, Reboot, Escalate>;

std::pair<Model, Cmd<Msg>> update(Model m, Msg msg);   // in store.cpp

} // namespace mtx
