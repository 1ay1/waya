/// autorace/board.cpp — the LED matrix, built with waya's real grid + sizing
/// mods (grid_cols / grid_rows / aspect / gap) instead of hand-written CSS.
/// A LANES × ROWS grid of red LED cells inside a black window; the whole window
/// keeps the LANES:ROWS aspect ratio and every cell is an equal grid fraction,
/// so it stays crisp at any size.
#include "board.hpp"
#include "theme.hpp"

#include <waya/surface/sugar.hpp>

namespace ar {

using namespace waya::surface;

namespace {
enum Cell { EMPTY, CAR, PLAYER, FLASH };

NodeRef led(Cell c) {
    std::uint32_t col = ledOff; bool glow = false; float op = 1.f;
    switch (c) {
        case CAR:    col = ledOn;  glow = true; break;
        case PLAYER: col = ledHot; glow = true; break;
        case FLASH:  col = ledHot; glow = true; break;
        default:     col = ledOff; op = 0.55f;  break;   // dim unlit LED
    }
    auto cell = box()
        | round(3)
        | bg(col)
        | opacity(op)
        | transition("background .08s linear");
    if (glow) cell = cell | led_glow(col, 8);
    return cell;   // no fixed size — the grid stretches it to its cell
}
} // namespace

NodeRef led_board(const Model& m) {
    bool flashing = (m.phase == Phase::Crash) && ((m.frame / 2) % 2 == 0);

    std::vector<NodeRef> cells;
    cells.reserve(ROWS * LANES);
    for (int r = 0; r < ROWS; ++r) {
        for (int l = 0; l < LANES; ++l) {
            Cell c = EMPTY;
            for (const auto& car : m.cars) if (car.row == r && car.lane == l) { c = CAR; break; }
            if (r == PLAYER_ROW && l == m.player_lane) {
                c = flashing ? FLASH : PLAYER;
                if (m.phase == Phase::Crash && !flashing) c = EMPTY;   // blink out
            }
            cells.push_back(led(c));
        }
    }

    // The matrix: a real grid with N equal columns AND N equal rows, so every
    // cell is a uniform fraction that fills the window (no auto-height blowup).
    auto matrix = box_(std::move(cells))
        | grid_cols(LANES) | grid_rows(ROWS)
        | gap(5) | w_full | h_full;

    // The black LED window. It keeps the LANES:ROWS aspect and is FIT (never
    // forced) inside its flex cell: aspect-ratio drives the shape while
    // max-width/max-height cap both axes to the cell, so it can only ever
    // shrink to fit — it never pushes the handheld past the viewport.
    return box(matrix)
        | aspect((float)LANES / (float)ROWS)
        | max_w(pct(100))
        | detail::raw_css("max-height", "100%")
        | detail::raw_css("height", "100%")
        | detail::raw_css("width", "auto")
        | detail::raw_css("min-height", "0")
        | pad(10) | round(8)
        | bg(window)
        | detail::raw_css("box-shadow",
            "inset 0 0 30px rgba(0,0,0,.9), inset 0 0 0 2px " + hx(bezel) + ", 0 0 0 5px " + hx(bezel))
        | border(1, line);
}

} // namespace ar
