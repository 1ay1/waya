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

        // ── HUD in the style of the original: big glowing readouts on the black
        // centre band (TIME + GEAR), the tach beside them, ACTIVISION footer.
        std::uint32_t etc = m.phase==Phase::Finished ? good
                          : (m.phase==Phase::Blown||m.phase==Phase::Fouled) ? warn : 0xffffff;

        // a labelled readout: tiny caption above a big glowing value.
        auto readout = [&](std::string label, std::string value, std::uint32_t c, int size){
            return col(
                text(std::move(label)) | fg(waya::rgba(0xffffff, 0.45f)) | font(9) | term
                    | weight(Weight::black) | uppercase | tracking_em(0.28f),
                text(std::move(value)) | fg(c) | font(size) | term | weight(Weight::black)
                    | tabular_nums | tracking_em(0.02f) | leading(0.9f) | glow_text(c, 12)
            ) | gap(1);
        };

        // centre band: TIME | tach | GEAR, laid out across the black divider.
        auto midband = row(
            readout("time", et, etc, 38),
            box(tachometer(m)) | grow() | pad_x(rem(1.4f)),
            readout("gear", m.gear ? std::to_string(m.gear) : "N", 0xffffff, 38)
        ) | items_center | gap(rem(1)) | w_full
          | absolute() | left(pct(0)) | right(pct(0)) | top(pct(47.5f)) | h(pct(5))
          | pad_x(rem(1.6f));

        // brand + best-time strip along the very bottom (the ACTIVISION line).
        auto footer = row(
            text("ACTIVISION") | fg(0xffffff) | font(14) | term | weight(Weight::black) | tracking_em(0.28f)
                | glow_text(0xffffff, 4),
            spacer(),
            readout("best", best, good, 15)
        ) | items_center | w_full
          | absolute() | left(pct(0)) | right(pct(0)) | bottom(pct(0)) | h(pct(7))
          | pad_x(rem(1.6f)) | bg(band)
          | detail::raw_css("box-shadow", "inset 0 1px 0 rgba(255,255,255,.06)");

        // control buttons (bottom-right, above the footer) — for touch/click.
        bool running = m.phase==Phase::Race || m.phase==Phase::Count;
        auto controls = row(
            hud_pill("␣", m.gas ? "THROTTLE" : "THROTTLE", GasTog{}, warn, m.gas),
            hud_pill("▲", "SHIFT", Shift{}, amber, false),
            hud_pill(running ? "⏎" : "⏎", running ? "RUNNING" : "START", Start{}, good, running)
        ) | gap(10)
          | absolute() | right(pct(2)) | bottom(pct(9)) | z(6);

        auto hud = box(midband, controls, footer)
            | absolute() | pin() | z(5) | safe_area() | no_pointer;

        // the whole window: full-bleed screen, HUD over it, overlay on top. dvh/
        // dvw (dynamic viewport) has no Len unit — the one place raw_css is right.
        return box(fx(), strip_screen(m), hud, ov)
            | absolute() | overflow("hidden") | bg(dr::page)
            | detail::raw_css("position", "relative")
            | detail::raw_css("height", "100dvh")
            | detail::raw_css("width", "100dvw")
            // global keyboard — Space = throttle, Up/W = shift, Enter/R = start.
            // (Shift itself isn't used as a game key: it's a modifier and holding
            // it interferes with other keydowns.)
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
