/// dragster/screen.cpp — the LCD panel. Everything is drawn as a grid of square
/// "segments" that are either LIT (near-black) or a faint GHOST (the unlit shape
/// you can still see on a real LCD). A top-down view down the strip: the
/// dragster sits at the bottom, lane dashes and distance ticks scroll toward you
/// to sell speed, and a progress bar counts down to the finish line. Plus the
/// tachometer, drawn in the same segment style.
///
/// Built from waya's real Mods (grid_cols/grid_rows/gap/bg/round/aspect/…);
/// raw_css only for the LCD sheen (a gradient overlay) — everything else is a Mod.
#include "screen.hpp"
#include "theme.hpp"

#include <waya/color.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/layout.hpp>
#include <string>
#include <vector>

namespace dr {

using namespace waya::surface;

namespace {

using waya::rgb;

constexpr int COLS = 9;    // lanes across the strip
constexpr int ROWS = 11;   // rows down the strip (near = bottom)
constexpr int CAR_COL = 4; // the dragster's lane (centre)
constexpr int CAR_ROW = 9; // the dragster's row (near the bottom)

// One LCD cell: LIT (dark segment) or GHOST (faint unlit shape). `warm` tints a
// lit cell toward the redline brown.
NodeRef cell(bool lit, bool warm = false) {
    return box()
        | round(px(2))
        | bg(lit ? rgb(warm ? hot : seg) : rgb(ghost).alpha(0.22f));
}

// The dragster sprite, as a set of (col,row) offsets from CAR_COL/CAR_ROW.
// A little top-down funny car: nose, body, two rear slicks.
bool is_car(int c, int r) {
    int dc = c - CAR_COL, dr = r - CAR_ROW;
    // nose (row -2), body (rows -1..0), rear axle (row +0 wide)
    if (dr == -2 && dc == 0) return true;                 // nose
    if (dr == -1 && (dc == 0)) return true;               // body
    if (dr ==  0 && (dc >= -1 && dc <= 1)) return true;   // cabin + axle
    if (dr ==  1 && (dc == -1 || dc == 1)) return true;   // rear slicks
    return false;
}

} // namespace

NodeRef strip_screen(const Model& m) {
    // How far the road has scrolled (in cells). Faster gears scroll faster.
    int scroll = (m.phase == Phase::Race) ? (int)(m.pos + m.speed) : (int)m.pos;

    // Build the ROWS×COLS matrix.
    std::vector<NodeRef> cells;
    cells.reserve(ROWS * COLS);
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            bool lit = false;
            // the dragster
            if (is_car(c, r)) lit = true;
            // lane dashes down the two lane-lines (cols 2 and 6), scrolling.
            else if ((c == 2 || c == 6) && ((r + scroll) % 3 == 0)) lit = true;
            // centre-line dashes (col 4) scrolling at the same cadence, offset.
            else if (c == 4 && ((r + scroll) % 4 == 0) && !is_car(c, r)) lit = true;
            cells.push_back(cell(lit));
        }
    }

    auto matrix = box_(std::move(cells))
        | grid_cols(COLS) | grid_rows(ROWS)
        | gap(4) | w_full | h_full;

    // The LCD panel fills the whole window: olive screen, matte inner frame, a
    // soft sheen on top. The HUD (brand, timing, tach, buttons) floats over it
    // in dark LCD ink, so the whole page reads as one big handheld screen.
    return box(matrix, box() | sheen() | absolute() | pin() | no_pointer)
        | absolute() | pin() | overflow("hidden")
        | pad(rem(1.1f)) | gradient(lcd, lcdLo, 160)
        | detail::raw_css("box-shadow", "inset 0 0 0 3px " + rgb(frame).css()
                          + ", inset 0 0 60px " + rgb(0x000000).alpha(0.16f).css());
}

NodeRef tachometer(const Model& m) {
    // A row of LCD segments; the top ~4 are the redline (warm). Lit fills with
    // rpm — the same segment language as the screen, so it reads as one device.
    std::vector<NodeRef> segs;
    segs.reserve(TACH_ZONES);
    const int lit = (m.rpm * TACH_ZONES) / REDLINE;
    for (int i = 0; i < TACH_ZONES; ++i) {
        const bool redzone = i >= TACH_ZONES - 4;
        segs.push_back(
            box() | grow() | h_full | round(px(2))
                  | bg(i < lit ? rgb(redzone ? hot : seg) : rgb(ghost).alpha(0.22f)));
    }
    auto bar = row_(std::move(segs)) | gap(3) | h_full | align(Align::stretch) | w_full;

    return col(
        row(text("TACH") | fg(inkSoft) | font(9) | uppercase | tracking_em(0.2f) | term,
            spacer(),
            text(m.rpm > REDLINE ? "REDLINE" : (m.over > 0 ? "hot" : ""))
                | fg(ink) | font(9) | weight(Weight::black) | term
        ) | items_center | w_full,
        box(bar) | h(px(16)) | w_full
            | pad(px(3)) | round(px(4)) | bg(rgb(lcdLo).alpha(0.5f))
            | detail::raw_css("box-shadow", "inset 0 0 0 1px " + rgb(frame).css())
    ) | gap(4) | w_full;
}

} // namespace dr
