/// dragster/console.cpp — the woodgrain console: a timing/telemetry readout strip
/// and the tactile control buttons (throttle, shift, start). The buttons fire the
/// same Msgs as the keyboard, so touch and keys are interchangeable.
#include "console.hpp"
#include "theme.hpp"

#include <waya/surface/sugar.hpp>
#include <waya/surface/layout.hpp>
#include <cstdio>
#include <string>

namespace dr {

using namespace waya::surface;

namespace {

// a glowing readout: small label above, big value below.
NodeRef readout(std::string label, std::string value, std::uint32_t c = amber) {
    return col(
        text(std::move(label)) | fg(muted) | font(9) | term | uppercase | tracking_em(0.18f),
        text(std::move(value)) | fg(c) | font(24) | term | weight(Weight::black)
            | tabular_nums | text_glow(c, 8) | detail::raw_css("letter-spacing","1px")
    ) | gap(2) | items_center | pad_x(14) | pad_y(8) | round(6)
      | detail::raw_css("background", "#0b0b10")
      | detail::raw_css("box-shadow", "inset 0 0 14px rgba(0,0,0,.8)")
      | detail::raw_css("border", "1px solid " + hxa(amber, "1a"));
}

// a chunky console push-button that dispatches a Msg on tap.
template <class Msg>
NodeRef pushbtn(std::string label, Msg msg, std::uint32_t c, int wpx = 0) {
    auto b = box(text(std::move(label)) | fg(0x120806) | font(15) | term | weight(Weight::black))
        | detail::raw_css("min-width", "64px")
        | pad_x(16) | pad_y(16) | round(10) | center | pointer | select_none
        | detail::raw_css("background", "linear-gradient(180deg," + hx(c) + "," + hxa(c,"cc") + ")")
        | detail::raw_css("box-shadow",
            "inset 0 1px 0 rgba(255,255,255,.35), 0 5px 0 rgba(0,0,0,.5), 0 8px 12px -5px rgba(0,0,0,.7)")
        | detail::raw_css("border", "1px solid " + hxa(0x000000, "44"))
        | transition("transform .06s ease, box-shadow .06s ease")
        | on(Active, detail::raw_css("transform", "translateY(3px)"),
                     detail::raw_css("box-shadow", "inset 0 1px 0 rgba(255,255,255,.25), 0 1px 0 rgba(0,0,0,.5)"))
        | tap(std::move(msg));
    if (wpx) b = b | detail::raw_css("width", std::to_string(wpx) + "px");
    return b;
}

} // namespace

NodeRef brand_plate() {
    return row(
        col(text("DRAGSTER") | fg(amber) | font(20) | term | weight(Weight::black) | tracking_em(0.14f) | text_glow(amber, 6),
            text("activision \u00b7 1980") | fg(muted) | font(9) | term | uppercase | tracking_em(0.22f)) | gap(1),
        spacer(),
        text("\u25a0\u25a0\u25a0") | fg(good) | font(12) | text_glow(good, 4)
    ) | items_center | w_full;
}

NodeRef readouts(const Model& m) {
    char et[16]; std::snprintf(et, sizeof et, "%.2f", secs(m.elapsed));
    char bt[16];
    if (m.best_frames > 0) std::snprintf(bt, sizeof bt, "%.2f", secs(m.best_frames));
    else                   std::snprintf(bt, sizeof bt, "--.--");
    std::uint32_t etc = (m.phase == Phase::Finished) ? good
                      : (m.phase == Phase::Blown || m.phase == Phase::Fouled) ? hot : amber;
    return row(
        readout("time", et, etc),
        readout("best", bt, good),
        readout("gear", m.gear ? std::to_string(m.gear) : "N", chrome)
    ) | gap(10) | justify_center | w_full | wrap;
}

NodeRef controls(const Model& m) {
    // throttle (toggle), shift up, and start/restart.
    auto gas = pushbtn(m.gas ? "THROTTLE ◉" : "THROTTLE", GasTog{}, hot, 0);
    auto shift = pushbtn("SHIFT ▲", Shift{}, amber, 0);
    auto start = pushbtn(m.phase == Phase::Race || m.phase == Phase::Count ? "STAGED" : "START",
                         Start{}, good, 0);
    return row(gas, shift, spacer(), start)
        | items_center | w_full | gap(12) | wrap;
}

} // namespace dr
