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
            ? box(text("▶  PRESS ENTER / TAP TO RACE") | fg(0x000000) | font(13) | weight(Weight::black) | term)
                | pad_x(18) | pad_y(12) | round(px(6)) | pointer
                | bg(0xffffff) | animate("dr-glow", 1400)
                | tap(Start{})
            : nothing();
        return col(
            text(std::move(title)) | fg(tc) | font(44) | weight(Weight::black) | term
                | tracking_em(0.06f) | text_align(Justify::center),
            text(std::move(sub)) | fg(0xffffff) | font(14) | term | text_align(Justify::center),
            prompt
        ) | gap(18) | center
          | absolute() | pin() | z(10)
          | attr("data-modal","1") | attr("role","dialog") | attr("aria-modal","true") | tab_index(-1)
          | hotkey("Enter", Start{}) | hotkey("r", Start{}) | hotkey(" ", Start{})
          | bg(waya::rgba(0x000000, 0.78f)) | backdrop_blur(2);
    }

    // the christmas-tree staging lights during the countdown.
    static NodeRef tree(const Model& m) {
        int stage = 3 - (m.count / 20);            // 0,1,2 amber, then GO
        auto bulb = [&](int idx, std::uint32_t c){
            bool lit = stage > idx;
            return box() | size(px(34)) | round(pct(50)) | border(2, 0x000000)
                   | bg(lit ? waya::rgb(c) : waya::rgba(0xffffff, 0.15f));
        };
        bool green = m.count <= 6;
        return col(
            row(bulb(0, amber), bulb(1, amber), bulb(2, amber)) | gap(10) | justify(Justify::center),
            text(green ? "GREEN!" : "GET SET…") | fg(green ? good : amber)
                | font(22) | weight(Weight::black) | term | text_align(Justify::center)
        ) | gap(14) | center
          | absolute() | pin() | z(9)
          | attr("data-modal","1") | attr("role","dialog") | attr("aria-modal","true") | tab_index(-1)
          | bg(waya::rgba(0x000000, 0.5f));
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

        // ── a solid dashboard bar docked under the screen ──────────────────
        std::uint32_t etc = m.phase==Phase::Finished ? good
                          : (m.phase==Phase::Blown||m.phase==Phase::Fouled) ? warn : 0xffffff;
        bool running = m.phase==Phase::Race || m.phase==Phase::Count;

        // a compact stat: caption + value, vertically stacked and legible.
        auto stat = [&](std::string label, std::string value, std::uint32_t c){
            return col(
                text(std::move(label)) | fg(waya::rgba(0xffffff,0.55f)) | font(10) | term
                    | weight(Weight::bold) | uppercase | tracking_em(0.16f),
                text(std::move(value)) | fg(c) | font(22) | mono_font | weight(Weight::black) | tabular_nums | leading(1.0f)
            ) | gap(2);
        };

        // The HERO race timer — this is a best-time game, so the clock leads.
        // A clean tabular monospace so the digits don't jitter, with a caption.
        auto timer = col(
            text("elapsed") | fg(waya::rgba(0xffffff,0.55f)) | font(10) | term
                | weight(Weight::bold) | uppercase | tracking_em(0.22f),
            text(std::string(et) + "s") | fg(etc) | font(48) | mono_font | weight(Weight::black)
                | tabular_nums | leading(0.9f) | glow_text(etc, 14)
        ) | gap(2);

        // control key-hints: a key badge + label per action, grouped as one tray.
        auto controls = row(
            hud_pill("space", "GAS",   GasTog{}, warn,  m.gas),
            hud_pill("↑",     "SHIFT", Shift{},  amber, false),
            hud_pill(running ? "R" : "↵", running ? "RESET" : "START", Start{}, good, running)
        ) | gap(10);

        auto dash = col(
            row(
                timer,
                stat("gear", m.gear ? std::to_string(m.gear) : "N", 0xffffff),
                stat("best", best, good),
                box(tachometer(m)) | grow(),
                controls
            ) | items_center | gap(rem(1.8f)) | w_full,
            row(
                text("ACTIVISION") | fg(waya::rgba(0xffffff,0.85f)) | font(11) | term
                    | weight(Weight::black) | tracking_em(0.34f),
                spacer(),
                text("DRAGSTER · 1980") | fg(waya::rgba(0xffffff,0.4f)) | font(10) | term
                    | weight(Weight::bold) | tracking_em(0.24f)
            ) | items_center | w_full
        ) | gap(rem(0.8f))
          | pad_x(rem(2.2f)) | pad_y(rem(1.2f))
          | gradient(0x1a1d27, 0x0c0d13, 180)
          | detail::raw_css("box-shadow", "inset 0 1px 0 rgba(255,255,255,.09), 0 -10px 30px rgba(0,0,0,.4)")
          | detail::raw_css("border-top", "1px solid rgba(255,255,255,.08)")
          | safe_area();

        // the screen area: the two-lane race, with the phase overlay on top.
        auto screen = box(strip_screen(m), ov)
            | detail::raw_css("position", "relative")
            | grow() | w_full | overflow("hidden");

        // whole window = a flex column: screen grows, dashboard docks below.
        return col(fx(), screen, dash)
            | overflow("hidden") | bg(dr::page)
            | detail::raw_css("height", "100dvh")
            | detail::raw_css("width", "100dvw")
            // global keyboard — Space = throttle, Up/W = shift, Enter/R = start.
            | hotkey(" ",          GasTog{})
            | hotkey("ArrowUp",    Shift{})
            | hotkey("w",          Shift{})
            | hotkey("ArrowRight", Shift{})
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
