/// dragster/screen.cpp — the TV picture, built from waya layout primitives (no
/// hand-written grid CSS). A side view: blue sky band on top, green strip below
/// with scrolling distance stripes and a start/finish gantry, and the red
/// dragster sliding left-to-right as it covers the quarter mile. Below the TV,
/// the tachometer bar — the instrument the whole game is really played on.
#include "screen.hpp"
#include "theme.hpp"

#include <waya/surface/sugar.hpp>
#include <waya/surface/layout.hpp>
#include <string>
#include <vector>

namespace dr {

using namespace waya::surface;

namespace {

// One scrolling distance stripe on the track, positioned by percentage.
NodeRef stripe(double left_pct) {
    return box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", std::to_string(left_pct) + "%")
        | detail::raw_css("bottom", "18%")
        | detail::raw_css("width", "5%")
        | detail::raw_css("height", "6%")
        | detail::raw_css("background", hxa(lane, "cc"))
        | detail::raw_css("border-radius", "1px");
}

// The dragster itself — a chunky low-poly funny-car silhouette in flat 2600 red,
// facing right, with a big slick rear tyre and a little front wheel.
NodeRef dragster(bool firing) {
    // body
    auto body = box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", "14%") | detail::raw_css("bottom", "34%")
        | detail::raw_css("width", "58%") | detail::raw_css("height", "26%")
        | detail::raw_css("background", "linear-gradient(180deg," + hx(carHi) + "," + hx(car) + ")")
        | detail::raw_css("border-radius", "6px 14px 4px 4px")
        | detail::raw_css("box-shadow", "inset 0 2px 0 " + hxa(chrome, "55"));
    // cockpit / airfoil
    auto wing = box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", "2%") | detail::raw_css("bottom", "56%")
        | detail::raw_css("width", "22%") | detail::raw_css("height", "10%")
        | detail::raw_css("background", hx(bezel))
        | detail::raw_css("border-radius", "2px");
    // rear slick
    auto rear = box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", "6%") | detail::raw_css("bottom", "10%")
        | detail::raw_css("width", "26%") | detail::raw_css("height", "34%")
        | detail::raw_css("background", "radial-gradient(circle at 40% 40%, #333, #000)")
        | detail::raw_css("border-radius", "50%")
        | detail::raw_css("box-shadow", "inset 0 0 0 3px #111");
    // front wheel
    auto front = box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", "60%") | detail::raw_css("bottom", "22%")
        | detail::raw_css("width", "15%") | detail::raw_css("height", "20%")
        | detail::raw_css("background", "radial-gradient(circle at 40% 40%, #333, #000)")
        | detail::raw_css("border-radius", "50%");
    // exhaust flame when the throttle's lit
    auto flame = box()
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", "-8%") | detail::raw_css("bottom", "40%")
        | detail::raw_css("width", "12%") | detail::raw_css("height", "8%")
        | detail::raw_css("background", "linear-gradient(90deg,transparent," + hx(amber) + "," + hx(hot) + ")")
        | detail::raw_css("border-radius", "3px")
        | detail::raw_css("opacity", firing ? "1" : "0")
        | detail::raw_css("filter", "blur(1px)");

    return box(wing, body, rear, front, flame)
        | detail::raw_css("position", "absolute")
        | detail::raw_css("bottom", "0") | detail::raw_css("left", "0")
        | detail::raw_css("width", "26%") | detail::raw_css("height", "100%");
}

} // namespace

NodeRef strip_screen(const Model& m) {
    bool firing = (m.phase == Phase::Race && m.gas) || m.phase == Phase::Count;

    // scrolling stripes: their phase follows m.pos so the ground appears to rush.
    double ph = (double)((m.pos * 3) % 20);
    auto s1 = stripe(5  - ph);
    auto s2 = stripe(30 - ph);
    auto s3 = stripe(55 - ph);
    auto s4 = stripe(80 - ph);

    // finish gantry drifts in from the right as you near the line.
    double gx = 96.0 - (double)m.pos / STRIP_LEN * 12.0;
    auto gantry = box(
        text("FINISH") | fg(ink) | font(9) | term | weight(Weight::black)
            | detail::raw_css("position","absolute") | detail::raw_css("top","6%")
            | detail::raw_css("left","50%") | detail::raw_css("transform","translateX(-50%)")
            | detail::raw_css("white-space","nowrap"))
        | detail::raw_css("position", "absolute")
        | detail::raw_css("left", std::to_string(gx) + "%")
        | detail::raw_css("top", "0") | detail::raw_css("bottom", "18%")
        | detail::raw_css("width", "3%")
        | detail::raw_css("background", "repeating-linear-gradient(0deg,#fff 0 6px,#000 6px 12px)");

    // the car slides across as pos advances (0..STRIP_LEN -> 6%..66% of width).
    double carLeft = 6.0 + (double)m.pos / STRIP_LEN * 60.0;
    auto car_layer = box(dragster(firing))
        | detail::raw_css("position","absolute")
        | detail::raw_css("bottom","0") | detail::raw_css("top","0")
        | detail::raw_css("left", std::to_string(carLeft) + "%")
        | detail::raw_css("width","30%")
        | transition("left .09s linear");

    // sky band (top 55%) + track band (bottom 45%)
    auto sky_band = box()
        | detail::raw_css("position","absolute") | detail::raw_css("top","0")
        | detail::raw_css("left","0") | detail::raw_css("right","0") | detail::raw_css("height","55%")
        | detail::raw_css("background", "linear-gradient(180deg," + hx(sky) + "," + hx(skyLo) + ")");
    auto track_band = box(s1,s2,s3,s4)
        | detail::raw_css("position","absolute") | detail::raw_css("bottom","0")
        | detail::raw_css("left","0") | detail::raw_css("right","0") | detail::raw_css("height","45%")
        | detail::raw_css("background", "linear-gradient(180deg," + hx(track) + "," + hx(trackLo) + ")")
        | detail::raw_css("box-shadow", "inset 0 3px 0 " + hxa(0x000000,"33"));

    // the CRT screen: 16:9-ish TV inside a black bezel, scanlined.
    return box(sky_band, track_band, gantry, car_layer,
               box() | scanlines() | detail::raw_css("position","absolute") | detail::raw_css("inset","0")
                     | detail::raw_css("pointer-events","none"))
        | detail::raw_css("position","relative")
        | detail::raw_css("width","100%")
        | aspect(16.0f / 9.0f)
        | detail::raw_css("max-height","100%")
        | detail::raw_css("overflow","hidden")
        | round(6)
        | detail::raw_css("background", hx(bezel))
        | detail::raw_css("box-shadow",
            "inset 0 0 40px rgba(0,0,0,.6), inset 0 0 0 3px #000, 0 0 0 6px " + hx(bezel))
        | border(2, 0x000000);
}

NodeRef tachometer(const Model& m) {
    // TACH_ZONES segments; a segment is lit if rpm covers its share of REDLINE.
    // The top ~4 zones are the redline — hot-colored, and glowing when you're in
    // them (that's the "keep it near the line without blowing" tension).
    std::vector<NodeRef> segs;
    segs.reserve(TACH_ZONES);
    int lit = (m.rpm * TACH_ZONES) / REDLINE;
    for (int i = 0; i < TACH_ZONES; ++i) {
        bool redzone = i >= TACH_ZONES - 4;
        bool on = i < lit;
        std::uint32_t c = redzone ? hot : (i >= TACH_ZONES - 8 ? amber : good);
        auto seg = box()
            | detail::raw_css("flex","1 1 0")
            | detail::raw_css("height","100%")
            | round(2)
            | detail::raw_css("background", on ? hx(c) : hxa(c, "22"));
        if (on) seg = seg | dglow(c, redzone ? 10 : 5);
        segs.push_back(seg);
    }
    auto bar = box_(std::move(segs))
        | detail::raw_css("display","flex") | detail::raw_css("gap","3px")
        | detail::raw_css("height","100%") | detail::raw_css("align-items","stretch") | w_full;

    return col(
        row(text("TACH") | fg(muted) | font(9) | term | uppercase | tracking_em(0.2f),
            spacer(),
            text(m.rpm > REDLINE ? "REDLINE!" : (m.over > 0 ? "hot" : "")) | fg(hot) | font(9) | term | weight(Weight::black)
        ) | items_center | w_full,
        box(bar) | detail::raw_css("height","18px") | w_full
    ) | gap(4) | w_full;
}

} // namespace dr
