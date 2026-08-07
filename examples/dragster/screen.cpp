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

// (positions that change every frame use the framework's class-stable inline
// helpers — at_left/at_width — so a 30fps animation doesn't mint a CSS class
// per value; see node.hpp animate_style.)

// ── the side-view dragster ───────────────────────────────────────────────
// A clean, recognizable rail dragster facing RIGHT, built from real shapes (not
// a pixel grid): a big rear slick, a small front wheel on a long axle beam, a
// low tapered body, a cockpit bubble and a rear wing. `hue` tints the livery so
// the two racers differ; `firing` adds an exhaust flame.
NodeRef sprite(std::uint32_t hue, bool firing) {
    waya::Color H = waya::rgb(hue);
    auto rimC = 0x3a3f52u;

    // a wheel: dark tyre + a subtle rim ring + a bright hub.
    auto wheel = [&](float leftPct, float botPct, float dPct){
        return box(
            box() | absolute() | pin() | round(pct(50))
                  | detail::raw_css("background","radial-gradient(circle at 38% 35%, #2b2f3d 0%, #0c0d12 62%, #05060a 100%)"),
            box() | absolute() | pin() | round(pct(50)) | detail::raw_css("box-shadow","inset 0 0 0 2px "+rgb(rimC).css()),
            box() | absolute() | left(pct(32)) | top(pct(32)) | w(pct(36)) | h(pct(36)) | round(pct(50))
                  | detail::raw_css("background","radial-gradient(circle at 40% 40%, #6b7288, #2a2e3c)"))
            | absolute() | left(pct(leftPct)) | bottom(pct(botPct)) | w(pct(dPct))
            | detail::raw_css("aspect-ratio","1/1")
            | detail::raw_css("filter","drop-shadow(0 3px 3px rgba(0,0,0,.4))");
    };

    // the long chassis beam from rear axle to front wheel.
    auto beam = box()
        | absolute() | left(pct(16)) | bottom(pct(30)) | w(pct(60)) | h(pct(7)) | round(px(3))
        | gradient(H.lighten(0.1f).opaque(), H.darken(0.25f).opaque(), 180)
        | detail::raw_css("box-shadow","inset 0 1px 0 rgba(255,255,255,.25)");

    // the driver body / engine cowling behind the rear axle (the fat back end).
    auto body = box()
        | absolute() | left(pct(2)) | bottom(pct(30)) | w(pct(30)) | h(pct(26))
        | detail::raw_css("border-radius","10px 4px 4px 10px")
        | gradient(H.lighten(0.18f).opaque(), H.darken(0.2f).opaque(), 180)
        | detail::raw_css("box-shadow","inset 0 2px 0 rgba(255,255,255,.3), inset 0 -3px 6px rgba(0,0,0,.35)");

    // cockpit bubble + roll hoop.
    auto cockpit = box()
        | absolute() | left(pct(30)) | bottom(pct(52)) | w(pct(16)) | h(pct(20))
        | detail::raw_css("border-radius","8px 8px 0 0")
        | detail::raw_css("background","linear-gradient(180deg, rgba(180,220,255,.9), rgba(90,130,180,.7))")
        | detail::raw_css("box-shadow","inset 0 1px 0 rgba(255,255,255,.6)");

    // rear wing on a mast.
    auto wing = box(
        box() | absolute() | left(pct(40)) | top(pct(0)) | w(pct(6)) | h(pct(38)) | bg(H.darken(0.2f).opaque()))
        | absolute() | left(pct(0)) | top(pct(4)) | w(pct(26)) | h(pct(12)) | round(px(3))
        | gradient(H.lighten(0.1f).opaque(), H.darken(0.3f).opaque(), 180)
        | detail::raw_css("box-shadow","0 2px 4px rgba(0,0,0,.4)");

    // nose cone tapering to the front wheel.
    auto nose = box()
        | absolute() | left(pct(64)) | bottom(pct(31)) | w(pct(20)) | h(pct(9))
        | detail::raw_css("border-radius","3px 8px 8px 3px")
        | gradient(H.lighten(0.12f).opaque(), H.darken(0.25f).opaque(), 180);

    // exhaust flame off the back when firing.
    auto flame = box()
        | absolute() | left(pct(-10)) | bottom(pct(38)) | w(pct(16)) | h(pct(10)) | round(px(6))
        | opacity(firing ? 1.0f : 0.0f)
        | detail::raw_css("background","linear-gradient(90deg, transparent, "+rgb(amber).css()+", "+rgb(warn).css()+")")
        | detail::raw_css("filter","blur(1.5px)");

    return box(flame, wing, beam, wheel(4, 6, 40), body, cockpit, nose, wheel(66, 18, 22))
        | detail::raw_css("position","relative")
        | w(pct(100)) | h(pct(100));
}

// One lane: purple sky over mint ground, scrolling road markings, and a car
// sliding across by `progress` (0..1). `firing` shows the exhaust flame; `hue`
// tints the car livery so the two racers differ.
NodeRef lane(double progress, int scroll, bool moving, bool firing, std::uint32_t hue) {
    // road centre-line dashes on the ground, scrolling left as you move.
    std::vector<NodeRef> dashes;
    const int N = 10;
    dashes.reserve(N + 2);
    const double off = (scroll % 100) / 100.0 * (108.0 / N);
    for (int i = -1; i < N + 1; ++i) {
        double x = i * (108.0 / N) - off;
        dashes.push_back(
            box() | absolute() | bottom(pct(20)) | w(pct(6)) | h(pct(4))
                  | round(px(2)) | bg(rgb(0xffffff).alpha(0.7f))
                  | at_left(x));   // dynamic left -> inline (stable class)
    }
    auto ground_band = box_(std::move(dashes))
        | absolute() | bottom(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(48))
        | gradient(ground, groundLo, 180) | overflow("hidden")
        // a soft top highlight = the horizon edge catching light.
        | detail::raw_css("box-shadow", "inset 0 4px 0 " + rgb(0xffffff).alpha(0.18f).css());

    // sky with scrolling distance ticks up top (finish-line markers).
    std::vector<NodeRef> ticks;
    const int T = 14;
    ticks.reserve(T + 2);
    const double toff = (scroll % 100) / 100.0 * (108.0 / T);
    for (int i = -1; i < T + 1; ++i) {
        double x = i * (108.0 / T) - toff;
        ticks.push_back(
            box() | absolute() | top(pct(14)) | w(pct(1.6f)) | h(pct(30))
                  | bg(rgb(tick).alpha(0.85f))
                  | at_left(x));   // dynamic left -> inline (stable class)
    }
    auto sky_band = box_(std::move(ticks))
        | absolute() | top(pct(0)) | left(pct(0)) | right(pct(0)) | h(pct(52))
        | gradient(sky, skyLo, 180) | overflow("hidden");

    // the car sits on the road, slides 3%..74% across as progress runs.
    double cx = 3.0 + progress * 71.0;
    // a soft shadow ellipse under it.
    auto shadow = box()
        | absolute() | bottom(pct(15)) | w(pct(20)) | h(pct(7))
        | round(pct(50)) | bg(rgb(0x000000).alpha(0.3f))
        | detail::raw_css("filter", "blur(2px)")
        | at_left(cx + 2);           // dynamic left -> inline
    auto car_l = box(sprite(hue, firing))
        | absolute() | bottom(pct(16)) | w(pct(26)) | h(pct(52))
        | (moving ? transition("left .1s linear") : Mod{})
        | at_left(cx);               // dynamic left -> inline

    return box(sky_band, ground_band, shadow, car_l)
        | absolute() | pin() | overflow("hidden");
}

// A lane placed in a fixed vertical slice of the screen [top%, height%].
NodeRef lane_at(float topPct, float hPct, double progress, int scroll, bool moving, bool firing, std::uint32_t hue) {
    return box(lane(progress, scroll, moving, firing, hue))
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

    // two lanes stacked, separated by a slim black band. Player on top (cyan
    // livery), opponent below (amber livery).
    auto top_lane = lane_at(0,  47, you, scroll, racing, firing, cyan);
    auto bot_lane = lane_at(53, 47, opp, scroll, true,   racing, amber);

    // the black centre band (between the lanes) — where the HUD digits sit.
    auto bandM = box() | absolute() | top(pct(47)) | left(pct(0)) | right(pct(0)) | h(pct(6)) | bg(band);

    return box(top_lane, bot_lane, bandM)
        | absolute() | pin() | overflow("hidden") | bg(page);
}

NodeRef tachometer_bar(const Model& m) {
    // A segmented "shift-light" tach like a real race car: 16 chunky blocks that
    // fill with rpm — green → amber → red as you climb, the top blocks are the
    // redline. Lit blocks glow; unlit are faint. No caption — the strip supplies
    // it. A little "SHIFT!"/"REDLINE!" tag rides at the right end.
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
    auto bar = row_(std::move(blocks)) | gap(3) | h_full | align(Align::stretch) | grow();

    auto cue = text(over ? "REDLINE" : (lit >= N - 4 ? "SHIFT" : ""))
        | fg(over ? warn : amber) | font(10) | term | weight(Weight::black) | tracking_em(0.12f)
        | glow_text(over ? warn : amber, 8);

    // caption + cue on one line, then the bar full width beneath. The bar height
    // is fluid (clamp) so it scales with the viewport instead of a fixed 20px.
    return col(
        row(
            text("TACH") | fg(rgb(0xffffff).alpha(0.5f)) | font(9) | term | weight(Weight::bold) | uppercase | tracking_em(0.2f),
            spacer(),
            cue
        ) | items_center | w_full,
        box(bar) | w_full | pad(px(3)) | round(px(5)) | bg(0x0a0b10)
            | detail::raw_css("height", "22px")
            | detail::raw_css("flex", "0 0 auto")
            | detail::raw_css("box-shadow", "inset 0 1px 3px rgba(0,0,0,.7), inset 0 0 0 1px rgba(255,255,255,.05)")
    ) | gap(5) | w_full;
}

NodeRef tachometer(const Model& m) {
    // tachometer_bar already carries the TACH caption + cue row.
    return tachometer_bar(m);
}

} // namespace dr
