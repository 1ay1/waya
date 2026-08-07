/// dragster/app.cpp — the waya Program tying DRAGSTER together: init/update/
/// subscribe/view + the SSR meta. A 30fps server clock ticks the game while the
/// countdown or race is live; the whole thing is one Surface app served live.
///
/// Controls:  SPACE = throttle (toggle/hold)   ↑ / SHIFT = shift up
///            ENTER = start / restart
///
/// The car is played on the TACHOMETER: build revs, shift up before the redline
/// blows the motor, and cross the line in the fewest seconds. Best time wins.
#include "store.hpp"
#include "screen.hpp"
#include "console.hpp"
#include "theme.hpp"

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <chrono>
#include <string>

using namespace waya::surface;
using namespace dr;

struct Dragster {
    using Model = dr::Model;
    using Msg   = dr::Msg;

    static Model init() { return Model::staged(); }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) { return dr::update(std::move(m), std::move(msg)); }

    // 30fps clock while the countdown or race is live.
    static Sub<Msg> subscribe(const Model& m) {
        bool live = (m.phase == Phase::Count || m.phase == Phase::Race);
        return live ? Sub<Msg>::every(std::chrono::milliseconds(33), Tick{})
                    : Sub<Msg>::none();
    }

    // a full-screen dim overlay with a title + prompt.
    static NodeRef overlay(std::string title, std::string sub, std::uint32_t tc, bool showStart = true) {
        auto prompt = showStart
            ? box(text("\u25b6  PRESS ENTER / TAP TO RACE") | fg(0x120806) | font(13) | term | weight(Weight::black))
                | pad_x(18) | pad_y(12) | round(8) | pointer
                | detail::raw_css("background", "linear-gradient(180deg," + hx(good) + ",#2f8a24)")
                | detail::raw_css("box-shadow", "0 6px 16px -4px " + hxa(good,"88"))
                | detail::raw_css("animation", "dr-glow 1.4s ease-in-out infinite")
                | tap(Start{})
            : (box() | detail::raw_css("display","none"));
        return col(
            text(std::move(title)) | fg(tc) | font(34) | term | weight(Weight::black)
                | tracking_em(0.06f) | text_glow(tc, 14) | text_center,
            text(std::move(sub)) | fg(ink) | font(14) | term | text_center,
            prompt
        ) | gap(16) | items_center | center
          | detail::raw_css("position","absolute") | detail::raw_css("inset","0")
          | detail::raw_css("z-index","10")
          | detail::raw_css("background","rgba(6,6,12,.82)")
          | detail::raw_css("backdrop-filter","blur(2px)") | round(6);
    }

    // the christmas-tree staging lights during the countdown.
    static NodeRef tree(const Model& m) {
        // three amber bulbs stage down, then green. count runs 60 -> 0.
        int stage = 3 - (m.count / 20);            // 0,1,2 amber, then GO
        auto bulb = [&](int idx, std::uint32_t c){
            bool lit = stage > idx;
            return box() | detail::raw_css("width","34px") | detail::raw_css("height","34px")
                 | detail::raw_css("border-radius","50%")
                 | detail::raw_css("background", lit ? hx(c) : hxa(c,"22"))
                 | (lit ? dglow(c, 12) : detail::raw_css("box-shadow","none"))
                 | detail::raw_css("border","2px solid #000");
        };
        return col(
            row(bulb(0, amber), bulb(1, amber), bulb(2, amber)) | gap(10) | center,
            text(m.count <= 6 ? "GREEN!" : "GET SET\u2026") | fg(m.count<=6?good:amber)
                | font(20) | term | weight(Weight::black) | text_glow(m.count<=6?good:amber, 10) | text_center
        ) | gap(14) | items_center | center
          | detail::raw_css("position","absolute") | detail::raw_css("inset","0")
          | detail::raw_css("z-index","9")
          | detail::raw_css("background","rgba(6,6,12,.55)");
    }

    static NodeRef fx() {
        return markup(
            "<style>"
            "@keyframes dr-glow{0%,100%{filter:brightness(1)}50%{filter:brightness(1.3)}}"
            "html,body{overscroll-behavior:none;overflow:hidden;margin:0;height:100%}"
            "</style>");
    }

    static NodeRef view(const Model& m) {
        char best[16];
        if (m.best_frames > 0) std::snprintf(best, sizeof best, "%.2fs", secs(m.best_frames));
        else                   std::snprintf(best, sizeof best, "no time yet");

        // pick the overlay for the current phase (none while racing).
        NodeRef ov =
            m.phase == Phase::Ready
                ? overlay("DRAGSTER", "throttle \u00b7 shift before the redline \u00b7 best time wins", amber)
          : m.phase == Phase::Count
                ? tree(m)
          : m.phase == Phase::Finished
                ? overlay("FINISH!", std::string("time ") +
                          [&]{ char t[16]; std::snprintf(t,sizeof t,"%.2fs",secs(m.elapsed)); return std::string(t); }()
                          + "   \u00b7   best " + best, good)
          : m.phase == Phase::Blown
                ? overlay("ENGINE BLOWN", "you held it past the redline \u2014 restage", hot)
          : m.phase == Phase::Fouled
                ? overlay("RED-LIGHT FOUL", "you shifted before the green \u2014 restage", hot)
          : (box() | detail::raw_css("display","none"));

        // the TV: the strip screen with the phase overlay on top.
        auto tv = box(strip_screen(m), ov)
            | detail::raw_css("position","relative") | w_full;

        // the console: brand, TV, tach, readouts, controls, help.
        auto console = col(
            brand_plate(),
            box() | detail::raw_css("height","1px") | detail::raw_css("background", hxa(0x000000,"55")) | w_full,
            box(tv) | detail::raw_css("flex","1 1 0") | detail::raw_css("min-height","0") | w_full
                | detail::raw_css("display","flex") | detail::raw_css("align-items","center")
                | detail::raw_css("justify-content","center"),
            tachometer(m),
            readouts(m),
            controls(m),
            text("space throttle   \u2191/shift up   enter start") | fg(muted) | font(10) | term | text_center | w_full
        ) | gap(12) | pad(20) | round(20)
          | detail::raw_css("box-sizing","border-box")
          | detail::raw_css("width","min(480px, 100%)")
          | detail::raw_css("max-width","100%")
          | detail::raw_css("height","100%")
          | detail::raw_css("max-height","100%")
          | detail::raw_css("min-height","0")
          | detail::raw_css("background",
              "linear-gradient(160deg," + hx(woodHi) + " 0%," + hx(woodLo) + " 60%,#2a1810 100%)")
          | detail::raw_css("box-shadow",
              "inset 0 2px 0 rgba(255,255,255,.12), inset 0 -3px 8px rgba(0,0,0,.5), 0 30px 70px -30px rgba(0,0,0,.9)")
          | detail::raw_css("border","1px solid " + hxa(0x000000,"55"));

        return box(fx(), console)
            | detail::raw_css("height","100dvh")
            | detail::raw_css("width","100dvw")
            | detail::raw_css("box-sizing","border-box")
            | detail::raw_css("display","flex")
            | detail::raw_css("align-items","center")
            | detail::raw_css("justify-content","center")
            | detail::raw_css("overflow","hidden")
            | detail::raw_css("padding",
                "max(12px, env(safe-area-inset-top)) "
                "max(12px, env(safe-area-inset-right)) "
                "max(12px, env(safe-area-inset-bottom)) "
                "max(12px, env(safe-area-inset-left))")
            | detail::raw_css("background",
                "radial-gradient(1000px 700px at 50% 0%, #1a1a24, transparent 60%), " + hx(dr::page))
            // global keyboard
            | hotkey(" ",          GasTog{})
            | hotkey("ArrowUp",    Shift{})
            | hotkey("Shift",      Shift{})
            | hotkey("w",          Shift{})
            | hotkey("Enter",      Start{})
            | hotkey("r",          Start{})
            | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "Dragster 1980 \u00b7 playable in C++";
        mt.description = "A playable homage to Activision's Dragster (1980, David Crane) \u2014 a "
                         "best-time drag race on a tachometer, rendered server-side in C++ with waya.";
        return mt;
    }
};

int main() { return live<Dragster>({ .port = 8081, .page_bg = dr::page, .title = "Dragster" }); }
