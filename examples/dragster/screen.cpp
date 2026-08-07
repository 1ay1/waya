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
// A long low rail dragster facing RIGHT: skinny front, big fat rear slick, a
// driver cockpit and a rear wing. '#'=black body, 'O'=tyre, '.'=empty.
// 20 wide × 8 tall.
constexpr int SW = 20, SH = 8;
constexpr const char* SPRITE[SH] = {
    ".........#..........",  // wing mast
    ".......#####........",  // rear wing
    "...OO....###....#...",  // cockpit + nose tip
    "..OOOO..######.####.",  // body upper
    ".OOOOOO#############",  // full chassis rail
    ".OOOOOO#########.oo.",  // chassis + front wheel
    "..OOOO.........oooo.",  // rear slick lower + front wheel
    "...OO...........oo..",  // rear slick contact
};

NodeRef sprite() {
    std::vector<NodeRef> cells;
    cells.reserve(SW * SH);
    for (int r = 0; r < SH; ++r)
        for (int c = 0; c < SW; ++c) {
            char ch = SPRITE[r][c];
            NodeRef cell;
            if (ch == '#')      cell = box() | bg(car);
            else if (ch == 'O') cell = box() | bg(0x0a0a0c) | round(pct(50));  // fat rear slick
            else if (ch == 'o') cell = box() | bg(0x1a1a1e) | round(pct(50));  // front wheel
            else                cell = box() | opacity(0.0f);
            cells.push_back(std::move(cell));
        }
    return box_(std::move(cells))
        | grid_cols(SW) | grid_rows(SH) | gap(0)
        | w(pct(100)) | h(pct(100))
        | detail::raw_css("filter", "drop-shadow(2px 3px 0 rgba(0,0,0,.35))");
}

// One lane: purple sky over mint ground, scrolling road markings, and a car
// sliding across by `progress` (0..1). `firing` shows the exhaust flame.
NodeRef lane(double progress, int scroll, bool moving, bool firing) {
    // road centre-line dashes on the ground, scrolling left as you move.
    std::vector<NodeRef> dashes;
    const int N = 10;
    const double off = (scroll % 100) / 100.0 * (108.0 / N);
    for (int i = -1; i < N + 1; ++i) {
        double x = i * (108.0 / N) - off;
        dashes.push_back(
            box() | absolute() | left(pct((float)x)) | bottom(pct(20)) | w(pct(6)) | h(pct(4))
                  | round(px(2)) | bg(rgb(0xffffff).alpha(0.7f)));
    }
    auto ground_band = box_(std::move(dashes))
        | absolute() | bottom(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(48))
        | gradient(ground, groundLo, 180) | overflow("hidden")
        // a soft top highlight = the horizon edge catching light.
        | detail::raw_css("box-shadow", "inset 0 4px 0 " + rgb(0xffffff).alpha(0.18f).css());

    // sky with scrolling distance ticks up top (finish-line markers).
    std::vector<NodeRef> ticks;
    const int T = 14;
    const double toff = (scroll % 100) / 100.0 * (108.0 / T);
    for (int i = -1; i < T + 1; ++i) {
        double x = i * (108.0 / T) - toff;
        ticks.push_back(
            box() | absolute() | left(pct((float)x)) | top(pct(14)) | w(pct(1.6f)) | h(pct(30))
                  | bg(rgb(tick).alpha(0.85f)));
    }
    auto sky_band = box_(std::move(ticks))
        | absolute() | top(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(52))
        | gradient(sky, skyLo, 180) | overflow("hidden");

    // the car sits on the road, slides 3%..76% across as progress runs.
    double cx = 3.0 + progress * 73.0;
    // a soft shadow ellipse under it.
    auto shadow = box()
        | absolute() | left(pct((float)cx + 1)) | bottom(pct(17)) | w(pct(18)) | h(pct(6))
        | round(pct(50)) | bg(rgb(0x000000).alpha(0.28f));
    // exhaust flame trailing behind (to the left).
    auto flame = box()
        | absolute() | left(pct((float)cx - 3)) | bottom(pct(34)) | w(pct(5)) | h(pct(7))
        | round(px(3)) | opacity(firing ? 1.0f : 0.0f)
        | detail::raw_css("background",
            "linear-gradient(90deg, transparent, " + rgb(amber).css() + ", " + rgb(warn).css() + ")")
        | detail::raw_css("filter", "blur(1px)");
    auto car_l = box(sprite())
        | absolute() | left(pct((float)cx)) | bottom(pct(19)) | w(pct(22)) | h(pct(40))
        | (moving ? transition("left .1s linear") : Mod{});

    return box(sky_band, ground_band, shadow, flame, car_l)
        | absolute() | pin() | overflow("hidden");
}

// A lane placed in a fixed vertical slice of the screen [top%, height%].
NodeRef lane_at(float topPct, float hPct, double progress, int scroll, bool moving, bool firing) {
    return box(lane(progress, scroll, moving, firing))
        | absolute() | top(pct(topPct)) | left(pct(0)) | right(pct(0)) | h(pct(hPct))
        | overflow("hidden");
}

} // namespace

NodeRef strip_screen(const Model& m) {
    const bool racing = (m.phase == Phase::Race);
    const bool firing = racing && m.gas;
    const int scroll = racing ? (int)(m.pos + m.speed) : (int)m.pos;
    const double you = (double)m.pos     / STRIP_LEN;
    const double opp = (double)m.opp_pos / STRIP_LEN;

    // two lanes stacked, separated by a slim black band. Player on top.
    auto top_lane = lane_at(0,  47, you, scroll, racing, firing);
    auto bot_lane = lane_at(53, 47, opp, scroll, true,   racing);

    // the black centre band (between the lanes) — where the HUD digits sit.
    auto bandM = box() | absolute() | top(pct(47)) | left(pct(0)) | right(pct(0)) | h(pct(6)) | bg(band);

    return box(top_lane, bot_lane, bandM)
        | absolute() | pin() | overflow("hidden") | bg(page);
}

NodeRef tachometer(const Model& m) {
    // A segmented "shift-light" tach like a real race car: 16 chunky blocks that
    // fill with rpm — green → amber → red as you climb, the top blocks are the
    // redline. Lit blocks glow; unlit are faint. Dramatic and readable at a
    // glance, and it screams "shift NOW" as the reds light.
    const int N = 16;
    const int lit = std::min(N, (int)((double)m.rpm / REDLINE * N + 0.5));
    const bool over = m.rpm > REDLINE;

    std::vector<NodeRef> blocks;
    blocks.reserve(N);
    for (int i = 0; i < N; ++i) {
        std::uint32_t c = i >= N - 4 ? warn : (i >= N - 8 ? amber : tachOk);
        bool on = i < lit;
        auto blk = box() | grow() | h_full | round(px(2))
                 | bg(on ? rgb(c) : rgb(c).alpha(0.12f));
        if (on) blk = blk | glow(c, i >= N - 4 ? 9 : 5);
        blocks.push_back(std::move(blk));
    }
    auto bar = row_(std::move(blocks)) | gap(3) | h_full | align(Align::stretch) | w_full;

    return col(
        row(
            text("TACH") | fg(rgb(0xffffff).alpha(0.55f)) | font(10) | term | weight(Weight::bold) | uppercase | tracking_em(0.22f),
            spacer(),
            text(over ? "REDLINE!" : (lit >= N - 4 ? "SHIFT!" : ""))
                | fg(over ? warn : amber) | font(11) | term | weight(Weight::black) | tracking_em(0.12f)
                | glow_text(over ? warn : amber, 8)
        ) | items_center | w_full,
        box(bar) | h(px(22)) | w_full | pad(px(4)) | round(px(6))
            | bg(0x0a0b10)
            | detail::raw_css("box-shadow", "inset 0 1px 3px rgba(0,0,0,.7), inset 0 0 0 1px rgba(255,255,255,.05)")
    ) | gap(5) | w_full;
}

} // namespace dr
