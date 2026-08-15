/// matrix/app.cpp — THE MATRIX: a hacker terminal over falling digital rain.
///
/// Highly component-based: each piece is its own module
///   theme.hpp        palette + term_font/glow style helpers
///   store.hpp/.cpp   Model + Msg + reducer (tick drives everything)
///   rain.hpp/.cpp    the falling-glyph SVG canvas
///   terminal.hpp/.cpp the typed hacker log + blinking cursor
///   hud.hpp/.cpp     breach meter, node grid, telemetry, controls
///   app.cpp          composes them + the tick + keyframes
///
///   waya run matrix
///
/// The whole animation is server state advanced ~15x/sec and streamed as DOM
/// deltas: rain falls, the terminal types, the breach meter climbs.

#include "store.hpp"
#include "theme.hpp"
#include "rain.hpp"
#include "terminal.hpp"
#include "hud.hpp"

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <chrono>

using namespace waya::surface;
using namespace mtx;

struct Matrix {
    using Model = mtx::Model;
    using Msg   = mtx::Msg;

    static Model init() { return Model::boot(); }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) { return mtx::update(std::move(m), std::move(msg)); }

    static Sub<Msg> subscribe(const Model& m) {
        return m.running ? Sub<Msg>::every(std::chrono::milliseconds(66), Tick{})   // ~15 fps
                         : Sub<Msg>::none();
    }

    // CSS keyframes for the blink/pulse animations, injected once.
    static NodeRef fx() {
        return markup(
            "<style>"
            "@keyframes mtx-blink{0%,50%{opacity:1}50.01%,100%{opacity:0}}"
            "@keyframes mtx-pulse{0%,100%{opacity:1}50%{opacity:.35}}"
            "@keyframes mtx-flicker{0%,100%{opacity:.97}50%{opacity:1}}"
            // full-screen scanline + vignette overlay above the rain, below the UI
            "body::after{content:'';position:fixed;inset:0;pointer-events:none;z-index:2;"
              "background:repeating-linear-gradient(0deg,rgba(0,0,0,0) 0 2px,rgba(0,0,0,.14) 2px 4px);"
              "animation:mtx-flicker 5s infinite}"
            "body::before{content:'';position:fixed;inset:0;pointer-events:none;z-index:2;"
              "background:radial-gradient(120% 100% at 50% 50%,transparent 45%,rgba(0,0,0,.7) 100%)}"
            "</style>");
    }

    static NodeRef view(const Model& m) {
        // the foreground overlay: title, terminal (fills), hud (side rail)
        auto title = row(
            text("THE MATRIX") | fg(green) | font(22) | term_font | weight(Weight::black)
                | phosphor(green, 10) | tracking_em(0.22f),
            box() | grow(),
            text("// wake up, neo") | fg(dim) | font(12) | term_font
        ) | items_center | w_full | wrap | gap(10);

        // terminal + hud side by side; stacks on narrow screens via switcher.
        auto stage = switcher(rem(46),
            terminal_pane(m) | grows,
            hud_panel(m)
        ) | gap(18) | items_start | w_full;

        auto overlay = col(title, stage)
            | gap(20)
            | relative
            | z(3)                                // above rain + scanlines
            | max_w(1200) | mx_auto
            | pad_fluid(20, 44)
            | h_screen;

        // stack the rain (z0) under the overlay (z3), with fx() supplying scanlines (z2).
        return box(fx(), rain_canvas(m), overlay)
            | relative
            | h_screen
            | bg(black)
            | clip
            | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "THE MATRIX \u00b7 hacker terminal";
        mt.description = "A Matrix-style digital-rain hacker terminal rendered server-side in C++ with waya.";
        return mt;
    }
};

int main() { return live<Matrix>({ .port = 8080, .page_bg = 0x000000, .title = "Matrix" }); }
