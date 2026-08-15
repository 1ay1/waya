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
          | dialog()
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
          | dialog()
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
                ? overlay(m.won ? "YOU WIN!" : "YOU LOSE",
                          std::string("time ") + et + "s   ·   best " + best,
                          m.won ? good : warn)
          : m.phase == Phase::Blown
                ? overlay("ENGINE BLOWN", "held past the redline — press enter", warn)
          : m.phase == Phase::Fouled
                ? overlay("RED-LIGHT FOUL", "shifted before the green — press enter", warn)
          : (nothing());

        // ── the data strip: one instrument row of evenly-spaced segments, each
        // divided by a hairline, so it reads as a single cohesive panel ────────
        std::uint32_t etc = m.phase==Phase::Finished ? good
                          : (m.phase==Phase::Blown||m.phase==Phase::Fouled) ? warn : ink;
        bool running = m.phase==Phase::Race || m.phase==Phase::Count;

        // a labelled field: caption over value; all fields share a baseline so
        // the values line up cleanly regardless of caption/value size.
        auto field = [&](std::string label, NodeRef value){
            return col(
                text(std::move(label)) | fg(inkFaint) | font(9) | term
                    | weight(Weight::bold) | uppercase | tracking_em(0.18f),
                value
            ) | gap(3);
        };
        auto val = [&](std::string v, std::uint32_t c, int size){
            return text(std::move(v)) | fg(c) | font(size) | mono_font
                 | weight(Weight::black) | tabular_nums | leading(1.0f);
        };

        auto controls = row(
            hud_pill("space", "GAS",   GasTog{}, warn,  m.gas),
            hud_pill("↑",     "SHIFT", Shift{},  amber, false),
            hud_pill(running ? "R" : "↵", running ? "RESET" : "START", Start{}, good, running)
        ) | gap(8) | wrap | row_gap(8);

        // readouts — a tidy group, values bottom-aligned so 30px TIME and 22px
        // GEAR/BEST share one baseline.
        auto readouts = row(
            field("time", val(std::string(et) + "s", etc, 28)),
            field("gear", val(m.gear ? std::to_string(m.gear) : "N", cyan, 22)),
            field("best", val(best, good, 22))
        ) | align(Align::end) | gap(rem(1.2f));

        // Top line: readouts left, controls right; wraps (controls drop below)
        // on narrow screens instead of overflowing.
        auto topline = row(readouts, spacer(), controls)
            | items_center | gap(rem(1.0f)) | w_full | wrap
            | row_gap(10);

        // the tach spans the FULL width on its own line — always readable.
        auto tachline = tachometer_bar(m) | w_full;

        auto dash = col(topline, tachline)
          | gap(rem(0.7f))
          | gradient(panel, panelLo, 180)
          | inset_light(.06f)
          | border_top(1, line_c)
          | detail::raw_css("padding",
              "0.85rem max(1.2rem, env(safe-area-inset-right)) "
              "max(0.85rem, env(safe-area-inset-bottom)) max(1.2rem, env(safe-area-inset-left))");

        // the screen area: the two-lane race, with the phase overlay on top.
        auto screen = box(strip_screen(m), ov)
            | relative
            | grow() | w_full | overflow("hidden");

        // the game "cabinet": screen grows on top, the data strip docks below.
        // Rounded + bordered so it reads as one framed unit.
        auto cabinet = col(screen, dash)
            | overflow("hidden") | round(rem(0.9f))
            | grow() | w_full
            | border(1, line_c)
            | shadow("0 24px 70px -24px rgba(0,0,0,.9)")
            | ring(rgba(0xffffff, .012f), 1);

        // whole window: the cabinet inset from the edges by an even outer margin
        // (safe-area aware), on the dark backdrop.
        return box(fx(), cabinet)
            | overflow("hidden") | bg(dr::page)
            | horizontal /* flex frame so the cabinet stretches */
            | h(dvh(100)) | w(dvw(100))
            | pad_safe(14)
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
