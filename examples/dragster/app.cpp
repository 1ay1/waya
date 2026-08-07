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
            ? box(text("▶  PRESS ENTER / TAP TO RACE") | fg(lcd) | font(13) | weight(Weight::black) | term)
                | pad_x(18) | pad_y(12) | round(px(6)) | pointer
                | bg(seg) | animate("dr-glow", 1400)
                | tap(Start{})
            : nothing();
        return col(
            text(std::move(title)) | fg(tc) | font(40) | weight(Weight::black) | term
                | tracking_em(0.06f) | text_align(Justify::center),
            text(std::move(sub)) | fg(ink) | font(14) | term | text_align(Justify::center),
            prompt
        ) | gap(18) | center
          | absolute() | pin() | z(10)
          | bg(waya::rgba(lcd, 0.86f)) | backdrop_blur(2);
    }

    // the christmas-tree staging lights during the countdown.
    static NodeRef tree(const Model& m) {
        int stage = 3 - (m.count / 20);            // 0,1,2 amber, then GO
        auto bulb = [&](int idx, std::uint32_t c){
            bool lit = stage > idx;
            return box() | size(px(34)) | round(pct(50)) | border(2, frame)
                   | bg(lit ? waya::rgb(c) : waya::rgb(ghost).alpha(0.3f));
        };
        bool green = m.count <= 6;
        return col(
            row(bulb(0, amber), bulb(1, amber), bulb(2, amber)) | gap(10) | justify(Justify::center),
            text(green ? "GREEN!" : "GET SET…") | fg(green ? good : amber)
                | font(22) | weight(Weight::black) | term | text_align(Justify::center)
        ) | gap(14) | center
          | absolute() | pin() | z(9) | bg(waya::rgba(lcd, 0.6f));
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
        else                   std::snprintf(best, sizeof best, "--.--");
        char et[16]; std::snprintf(et, sizeof et, "%.2f", secs(m.elapsed));

        // phase overlay (none while racing).
        NodeRef ov =
            m.phase == Phase::Ready
                ? overlay("DRAGSTER", "throttle · shift before the redline · best time wins", amber)
          : m.phase == Phase::Count
                ? tree(m)
          : m.phase == Phase::Finished
                ? overlay("FINISH", std::string("time ") + et + "s   ·   best " + best, good)
          : m.phase == Phase::Blown
                ? overlay("ENGINE BLOWN", "held past the redline — press enter", warn)
          : m.phase == Phase::Fouled
                ? overlay("RED-LIGHT FOUL", "shifted before the green — press enter", warn)
          : (nothing());

        // ── HUD, floating on top of the LCD panel ─────────────────────────
        // a stat chip: tiny label + big value, in LCD-dark ink on the olive.
        auto chip = [&](std::string label, std::string value, std::uint32_t c){
            return col(
                text(std::move(label)) | fg(inkSoft) | font(9) | term | uppercase | tracking_em(0.24f),
                text(std::move(value)) | fg(c) | font(30) | term | weight(Weight::black)
                    | tabular_nums | leading(1.0f)
            ) | gap(2) | items_center;
        };
        std::uint32_t etc = m.phase==Phase::Finished ? good
                          : (m.phase==Phase::Blown||m.phase==Phase::Fouled) ? warn : ink;

        // top bar: brand left, timing chips right — on a faint dark bezel strip.
        auto topbar = row(
            col(text("DRAGSTER") | fg(ink) | font(18) | term | weight(Weight::black) | tracking_em(0.16f),
                text("activision · 1980") | fg(inkSoft) | font(8) | term | uppercase | tracking_em(0.26f)) | gap(1),
            spacer(),
            chip("time", et, etc),
            chip("best", best, good),
            chip("gear", m.gear ? std::to_string(m.gear) : "N", ink)
        ) | items_center | gap(20) | w_full;

        // bottom bar: the tach across the width + the tap-controls + hint.
        auto botbar = col(
            tachometer(m),
            row(
                hud_pill(m.gas ? "THROTTLE ◉" : "THROTTLE", GasTog{}, warn, m.gas),
                hud_pill("SHIFT ▲", Shift{}, amber, false),
                spacer(),
                hud_pill(m.phase==Phase::Race||m.phase==Phase::Count ? "RACING" : "START", Start{}, good, false)
            ) | items_center | gap(10) | w_full,
            text("space throttle   ·   ↑ / shift up   ·   enter start")
                | fg(inkSoft) | font(9) | term | text_align(Justify::center) | w_full | tracking_em(0.08f)
        ) | gap(12) | w_full;

        // The HUD frame doesn't intercept clicks (no_pointer); the pills opt back
        // in to pointer events themselves.
        auto hud = col(topbar, spacer(), botbar)
            | absolute() | pin() | z(5) | pad(20) | safe_area() | no_pointer;

        // the whole window: full-bleed screen, HUD over it, overlay on top. dvh/
        // dvw (dynamic viewport) has no Len unit — the one place raw_css is right.
        return box(fx(), strip_screen(m), hud, ov)
            | absolute() | overflow("hidden") | bg(dr::page)
            | detail::raw_css("position", "relative")
            | detail::raw_css("height", "100dvh")
            | detail::raw_css("width", "100dvw")
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
