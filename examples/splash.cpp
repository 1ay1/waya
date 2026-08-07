/// examples/splash.cpp — a striking animated landing page. A living aurora
/// headline, a breathing status pill, gradient-bordered glass cards that lift on
/// hover, a glow CTA, a live counter. All one-line mods, no HTML/CSS/JS.
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

    static NodeRef feature(std::string icon, std::string title, std::string desc,
                           std::uint32_t a, std::uint32_t b, int delay_ms) {
        return col(
            text(icon) | font(28) | float_(5),
            text(title) | fg(ink) | subtitle | weight(Weight::bold),
            text(desc)  | fg(muted) | body
        ) | gap(10) | pad(24) | round(20)
          | gradient_border(a, b, 1)                       // glowing edge
          | grow(1) | min_w(rem(15))
          | hover_lift(5) | hover_glow(a, 30)
          | fade_up(600) | delay(delay_ms);
    }

    static NodeRef view(const Model& m) {
        auto pill = row(
            box() | size(6) | round(999) | bg(0x34d399) | breathe(),
            text("v1.0 \u00b7 typed, server-rendered, real-time") | fg(0x9fb3c8) | caption | semibold
        ) | gap(8) | center | pad_x(14) | pad_y(7) | round(999)
          | frost(10) | fade_in(500);

        auto hero = col(
            pill,
            text("Build live UIs in C++") | display | weight(Weight::black)
                | font_fluid(44, 84) | tracking_em(-0.03f)
                | aurora_text(0x818cf8, 0x22d3ee, 0xf472b6, 7) | fade_up(700),
            text("Describe what to render \u2014 waya owns how. Server-rendered, "
                 "keyed-diffed, themeable, and beautiful by default.")
                | fg(0x94a3b8) | font_fluid(16, 21) | max_w(rem(40))
                | leading(1.6f) | fade_up(800) | delay(80),
            row(
                text("Get started \u2192") | fg(white) | semibold | font(16)
                    | pad_x(26) | pad_y(14) | round(12) | gradient_bg(0x6366f1, 0x8b5cf6)
                    | glow(0x6366f1, 30) | hover_lift(2) | press() | pointer | tap(Star{}),
                row(
                    text(m.starred ? "\u2605" : "\u2606") | fg(m.starred ? warn : muted) | font(18),
                    text("Star  " + std::to_string(m.stars)) | fg(ink) | semibold | font(16)
                ) | gap(8) | center | pad_x(20) | pad_y(14) | round(12)
                  | frost(10) | interactive() | tap(Star{})
            ) | gap(14) | wrap | center | fade_up(900) | delay(160)
        ) | gap(26) | center | text_center;

        auto features = row(
            feature("\u26a1", "Real-time", "Only the changed nodes stream over a binary WebSocket. Instant.", 0x818cf8, 0x22d3ee, 1000),
            feature("\U0001f9ec", "Typed", "Messages are a std::variant \u2014 payloads, exhaustive matching, no strings.", 0x22d3ee, 0x34d399, 1120),
            feature("\U0001f3a8", "Beautiful", "Motion, glass, themes, aurora \u2014 every one a one-line mod.", 0xf472b6, 0x818cf8, 1240)
        ) | gap(20) | wrap | w_full;

        return page(0x070912, centered(60, col(hero, features) | gap(60) | center) | center)
             | mesh(0x6366f1, 0x22d3ee, 0x070912) | clip_x;
    }
};

int main() {
    static_assert(SurfaceProgram<Splash>);
    return live<Splash>({ .port = 8080, .page_bg = 0x070912, .title = "waya \u2014 live UIs in C++" });
}
