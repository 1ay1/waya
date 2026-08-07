/// autorace/app.cpp — AUTO RACE (1976), fully playable in waya.
///
/// A homage to Mattel Auto Race, the first handheld electronic game: a red-LED
/// grid racer. Steer between 3 lanes, shift gears for speed, dodge oncoming
/// traffic, rack up distance before the 99-second clock runs out.
///
/// Component-based:
///   theme.hpp        the 1976 handheld palette + LED-glow helpers
///   store.hpp/.cpp   the game model + loop (tick moves cars, scores, collides)
///   board.hpp/.cpp   the LED matrix render
///   console.hpp/.cpp readouts + physical control buttons
///   app.cpp          the case, the 30fps tick, global keyboard, overlays
///
///   waya run autorace
///
/// CONTROLS  \u2190/\u2192 steer \u00b7 \u2191/\u2193 (or gas/brake) gear \u00b7 Space start/restart.
/// Everything runs server-side; the browser sends key events and paints deltas.

#include "store.hpp"
#include "theme.hpp"
#include "board.hpp"
#include "console.hpp"

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <chrono>

using namespace waya::surface;
using namespace ar;

struct AutoRace {
    using Model = ar::Model;
    using Msg   = ar::Msg;

    static Model init() { return Model{}; }   // Attract screen
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) { return ar::update(std::move(m), std::move(msg)); }

    // 30 fps game clock while a race (or its crash flash) is live.
    static Sub<Msg> subscribe(const Model& m) {
        bool live = (m.phase == Phase::Playing || m.phase == Phase::Crash);
        return live ? Sub<Msg>::every(std::chrono::milliseconds(33), Tick{})
                    : Sub<Msg>::none();
    }

    // a full-screen dim overlay with a message + prompt
    static NodeRef overlay(std::string title, std::string sub, std::uint32_t tc) {
        return col(
            text(std::move(title)) | fg(tc) | font(30) | term | weight(Weight::black)
                | tracking_em(0.08f) | text_glow(tc, 12) | text_center,
            text(std::move(sub)) | fg(ink) | font(14) | term | text_center,
            box(text("\u25b6  PRESS SPACE / TAP TO START") | fg(0x0b0603) | font(13) | term | weight(Weight::black))
                | pad_x(18) | pad_y(12) | round(8) | pointer
                | detail::raw_css("background", "linear-gradient(180deg," + hx(amber) + ",#c77f10)")
                | detail::raw_css("box-shadow", "0 6px 16px -4px " + hxa(amber,"88"))
                | detail::raw_css("animation", "ar-glow 1.4s ease-in-out infinite")
                | tap(Start{})
        ) | gap(16) | items_center | center
          | detail::raw_css("position","absolute") | detail::raw_css("inset","0")
          | detail::raw_css("z-index","10")
          | detail::raw_css("background","rgba(5,2,1,.82)")
          | detail::raw_css("backdrop-filter","blur(2px)") | round(8);
    }

    static NodeRef fx() {
        return markup(
            "<style>"
            "@keyframes ar-glow{0%,100%{filter:brightness(1)}50%{filter:brightness(1.25)}}"
            "html,body{overscroll-behavior:none;overflow:hidden;margin:0;height:100%}"
            "</style>");
    }

    static NodeRef view(const Model& m) {
        // the LED window, with the state overlay on top when not racing
        NodeRef window = box(led_board(m),
            m.phase == Phase::Attract
                ? overlay("AUTO RACE", "steer \u00b7 shift \u00b7 survive 99s", amber)
            : m.phase == Phase::GameOver
                ? overlay(m.time_left<=0 ? "TIME!" : "CRASH!",
                          "score " + std::to_string(m.score) + "  \u00b7  best " + std::to_string(std::max(m.best,(int)m.score)),
                          ledOn)
                : (box() | detail::raw_css("display","none")))
            | detail::raw_css("position","relative") | w_full;

        // the handheld case: brand, readouts, LED window, controls. It fills the
        // viewport HEIGHT and the LED window flex-grows to take whatever space is
        // left after the fixed readouts/controls — so nothing ever scrolls.
        auto handheld = col(
            brand_plate(),
            box() | detail::raw_css("height","1px") | detail::raw_css("background", hxa(0x000000,"55")) | w_full,
            readouts(m),
            box(window) | detail::raw_css("flex","1 1 0") | detail::raw_css("min-height","0") | w_full
                | detail::raw_css("display","flex") | detail::raw_css("align-items","center")
                | detail::raw_css("justify-content","center"),
            controls(m),
            text("\u2190/\u2192 steer   \u2191/\u2193 gear   space start") | fg(muted) | font(10) | term | text_center | w_full
        ) | gap(14) | pad(20) | round(22)
          | detail::raw_css("box-sizing","border-box")
          | detail::raw_css("width","min(440px, 100%)")
          // Size to the case's natural content, but never taller/wider than the
          // space the page gives it. 100% here = the flex parent (the page box,
          // already inset by its own padding + safe-area), so the case can only
          // shrink to fit the real visible viewport — never overflow it.
          | detail::raw_css("max-width","100%")
          | detail::raw_css("height","100%")
          | detail::raw_css("max-height","100%")
          | detail::raw_css("min-height","0")
          | detail::raw_css("background",
              "linear-gradient(160deg," + hx(caseHi) + " 0%," + hx(caseLo) + " 55%,#280d09 100%)")
          | detail::raw_css("box-shadow",
              "inset 0 2px 0 rgba(255,255,255,.12), inset 0 -3px 8px rgba(0,0,0,.5), 0 30px 70px -30px rgba(0,0,0,.9)")
          | detail::raw_css("border","1px solid " + hxa(0x000000,"55"));

        // page: center the handheld (which sizes to the viewport), and bind the
        // GLOBAL keyboard here so keys work without focusing anything.
        return box(fx(), handheld)
            // 100dvh/dvw = the DYNAMIC viewport (excludes the mobile URL bar),
            // so the app fills exactly what's visible and never spills under the
            // browser chrome the way 100vh does. Padding uses safe-area insets
            // so nothing hides behind a notch / home indicator.
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
                "radial-gradient(1000px 700px at 50% 0%, #2a1a12, transparent 60%), " + hx(ar::page))
            // global controls
            | hotkey("ArrowLeft",  SteerL{})
            | hotkey("ArrowRight", SteerR{})
            | hotkey("a",          SteerL{})
            | hotkey("d",          SteerR{})
            | hotkey("ArrowUp",    GearUp{})
            | hotkey("ArrowDown",  GearDn{})
            | hotkey("w",          GearUp{})
            | hotkey("s",          GearDn{})
            | hotkey(" ",          Start{})
            | hotkey("Enter",      Start{})
            | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "Auto Race 1976 \u00b7 playable in C++";
        mt.description = "A fully-playable homage to Mattel Auto Race (1976), the first handheld "
                         "electronic game \u2014 a red-LED grid racer rendered server-side in C++ with waya.";
        return mt;
    }
};

int main() { return live<AutoRace>({ .port = 8080, .page_bg = ar::page, .title = "Auto Race" }); }
