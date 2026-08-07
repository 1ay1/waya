/// dragster/screen.cpp — the real Dragster layout: two horizontal lanes seen
/// from the side. Each lane is a purple sky band over mint-green ground with
/// pale distance ticks scrolling past; a black blocky dragster (a pixel sprite)
/// sits in the lane and slides left→right as it covers the strip. Between/around
/// the lanes: the signature green tachometer with its red redline marker.
///
/// Built from waya's real Mods (grid_cols/absolute/left/w/h/bg/gradient/…);
/// raw_css appears only where there is no primitive (a couple of gradients and
/// dvh sizing handled up in app.cpp).
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

// ── the side-view dragster, as a pixel sprite ───────────────────────────────
// Facing right: a fat rear slick on the left, a long body, a small front wheel,
// a driver bump. 1 = filled (black), 0 = empty. 14 wide × 7 tall.
constexpr int SW = 14, SH = 7;
constexpr const char* SPRITE[SH] = {
    "..###.........",   // driver head/rollbar
    ".#####..#####.",   // body top
    "###############",  // full body line
    "###############",  // full body line
    "###############",  // full body line
    ".####...#####.",   // underbody
    "##..##.....##.",   // rear slick + front wheel
};

NodeRef sprite() {
    std::vector<NodeRef> cells;
    cells.reserve(SW * SH);
    for (int r = 0; r < SH; ++r)
        for (int c = 0; c < SW; ++c) {
            bool on = SPRITE[r][c] == '#';
            cells.push_back(box() | (on ? bg(car) : opacity(0.0f)) | round(px(1)));
        }
    return box_(std::move(cells))
        | grid_cols(SW) | grid_rows(SH) | gap(1)
        | w(pct(100)) | h(pct(100));
}

// One lane: purple sky over mint ground, scrolling distance ticks, and a car
// sliding across by `progress` (0..1). `hero` = the player's lane (crisper).
NodeRef lane(double progress, int scroll, bool moving) {
    // distance ticks along the top of the sky, scrolling left as you move.
    std::vector<NodeRef> ticks;
    const int N = 16;
    for (int i = 0; i < N; ++i) {
        double x = ((double)i / N * 108.0 - (scroll % 100) / 100.0 * 6.75);
        if (x < -3) x += 108;
        ticks.push_back(
            box() | absolute() | left(pct((float)x)) | top(pct(0)) | w(pct(2.2f)) | h(pct(26))
                  | bg(tick));
    }
    auto sky_band   = box_(std::move(ticks))
        | absolute() | top(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(52))
        | gradient(sky, skyLo, 180) | overflow("hidden");
    auto ground_band = box()
        | absolute() | bottom(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(48))
        | gradient(ground, groundLo, 180);

    // the car sits low in the lane, slides 4%..78% across as progress runs.
    double cx = 4.0 + progress * 74.0;
    auto car_l = box(sprite())
        | absolute() | left(pct((float)cx)) | bottom(pct(20)) | w(pct(20)) | h(pct(52))
        | (moving ? transition("left .1s linear") : Mod{});

    return box(sky_band, ground_band, car_l)
        | absolute() | pin() | overflow("hidden");
}

// A lane placed in a fixed vertical slice of the screen [top%, height%].
NodeRef lane_at(float topPct, float hPct, double progress, int scroll, bool moving) {
    return box(lane(progress, scroll, moving))
        | absolute() | top(pct(topPct)) | left(pct(0)) | right(pct(0)) | h(pct(hPct))
        | overflow("hidden");
}

} // namespace

NodeRef strip_screen(const Model& m) {
    const bool racing = (m.phase == Phase::Race);
    const int scroll = racing ? (int)(m.pos + m.speed) : (int)m.pos;
    const double you = (double)m.pos     / STRIP_LEN;
    const double opp = (double)m.opp_pos / STRIP_LEN;

    // two lanes stacked: player on top, opponent below, black bands between.
    auto top_lane = lane_at(2,  40, you, scroll, racing);
    auto bot_lane = lane_at(56, 40, opp, scroll, racing);

    // black separator bands top / middle / bottom (the reference's letterbox).
    auto bandT = box() | absolute() | top(pct(0))  | left(pct(0)) | right(pct(0)) | h(pct(2))  | bg(band);
    auto bandM = box() | absolute() | top(pct(48)) | left(pct(0)) | right(pct(0)) | h(pct(8))  | bg(band);
    auto bandB = box() | absolute() | bottom(pct(0))| left(pct(0)) | right(pct(0)) | h(pct(4)) | bg(band);

    return box(top_lane, bot_lane, bandT, bandM, bandB)
        | absolute() | pin() | overflow("hidden") | bg(page);
}

NodeRef tachometer(const Model& m) {
    // the signature Dragster tach: a wide green bar; the top zone is a red
    // redline block; a bright marker sits where the current rpm is.
    const double frac = std::min(1.0, (double)m.rpm / REDLINE);
    const double redStart = 0.8;   // last 20% is the redline

    auto greenFill = box()
        | absolute() | top(pct(0)) | bottom(pct(0)) | left(pct(0))
        | w(pct((float)(frac * 100.0)))
        | bg(m.rpm > REDLINE ? tachRed : tachOk)
        | transition("width .08s linear");
    auto redZone = box()
        | absolute() | top(pct(0)) | bottom(pct(0)) | right(pct(0)) | w(pct((float)((1-redStart)*100)))
        | bg(rgb(tachRed).alpha(0.35f));
    // a bright marker line at the current rpm position.
    auto marker = box()
        | absolute() | top(pct(-15)) | bottom(pct(-15)) | left(pct((float)(frac*100.0)))
        | w(px(3)) | bg(0xffffff)
        | transition("left .08s linear");

    auto bar = box(redZone, greenFill, marker)
        | absolute() | pin();

    return box(bar)
        | w_full | h(px(18)) | round(px(3)) | overflow("hidden")
        | detail::raw_css("position", "relative")
        | bg(rgb(band).alpha(0.85f))
        | detail::raw_css("box-shadow", "inset 0 0 0 2px " + rgb(band).css());
}

} // namespace dr
