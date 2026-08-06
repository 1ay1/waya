/// examples/splash.cpp — a stunning animated landing page. Fluid gradient
/// headline, staggered entrances, glass cards, a glow CTA, a live counter — no
/// HTML, no CSS, no JS. Built from one-line mods: mesh(), frost(), hover_lift(),
/// gradient_text(), fade_up().
///
///   cmake --build build -j && ./build/splash     # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::color;

struct Splash {
    struct Model { int stars = 2847; bool starred = false; };
    struct Star {};
    using Msg = std::variant<Star>;

    static Model init() { return {}; }
    static Model update(Model m, Msg) { m.starred = !m.starred; m.stars += m.starred ? 1 : -1; return m; }

    static NodeRef feature(std::string icon, std::string title, std::string desc, int delay_ms) {
        return col(
            text(icon) | font(30),
            text(title) | fg(ink) | subtitle | weight(Weight::bold),
            text(desc)  | fg(muted) | body
        ) | gap(10) | pad(24) | round(20) | frost(16)
          | grow(1) | css("min-width", "15rem")
          | hover_lift(4) | fade_up(600) | delay(delay_ms);
    }

    static NodeRef view(const Model& m) {
        auto pill = text("v1.0 \u00b7 typed messages, server-rendered") | fg(brand2) | caption | semibold
                  | pad_x(14) | pad_y(6) | round(999) | tint(brand2, 0.10f) | hairline(brand2, 0.25f)
                  | fade_in(500);

        auto hero = col(
            pill,
            text("Build live UIs in C++") | display | weight(Weight::black)
                | font_fluid(40, 78) | css("letter-spacing", "-.03em")
                | gradient_text(0x818cf8, 0x22d3ee) | fade_up(700),
            text("Describe what to render \u2014 waya owns how. Real-time, keyed-diffed, "
                 "themeable, and beautiful by default.")
                | fg(muted) | font_fluid(16, 21) | css("max-width", "40rem")
                | leading(1.6f) | fade_up(800) | delay(80),
            row(
                text("Get started \u2192") | fg(white) | semibold | font(16)
                    | pad_x(24) | pad_y(14) | round(12) | bg(brand)
                    | hover_glow(brand, 30) | hover_lift(2) | press() | pointer | tap(Star{}),
                row(
                    text(m.starred ? "\u2605" : "\u2606") | fg(m.starred ? warn : muted) | font(18),
                    text("Star  " + std::to_string(m.stars)) | fg(ink) | semibold | font(16)
                ) | gap(8) | center | pad_x(20) | pad_y(14) | round(12)
                  | tint() | hairline() | hover_bg() | interactive() | tap(Star{})
            ) | gap(14) | wrap | center | fade_up(900) | delay(160)
        ) | gap(24) | center | css("text-align", "center");

        auto features = row(
            feature("\u26a1", "Real-time", "Only the changed nodes stream over a binary WebSocket. Instant.", 1000),
            feature("\U0001f9ec", "Typed", "Messages are a std::variant \u2014 payloads, exhaustive matching, no strings.", 1120),
            feature("\U0001f3a8", "Beautiful", "Motion, glass, themes, fluid type \u2014 every one a one-line mod.", 1240)
        ) | gap(20) | wrap | css("width", "100%");

        return page(0x090b14, centered(60, col(hero, features) | gap(56) | center) | center)
             | mesh(0x6366f1, 0x22d3ee, 0x090b14) | css("overflow-x", "hidden");
    }
};

int main() {
    static_assert(SurfaceProgram<Splash>);
    return live<Splash>({ .port = 8080, .page_bg = 0x090b14, .title = "waya \u2014 live UIs in C++" });
}
