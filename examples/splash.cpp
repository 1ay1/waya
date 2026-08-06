/// examples/splash.cpp — a stunning animated landing page. The "wait, C++ made
/// this?" first impression: fluid gradient headline, staggered entrance
/// animations, glassmorphism, glow, a live counter — no HTML, no CSS, no JS.
///
///   cmake --build build -j && ./build/splash     # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

struct Splash {
    struct Model { int stars = 2847; bool starred = false; };
    struct Star {};
    using Msg = std::variant<Star>;

    static Model init() { return {}; }
    static Model update(Model m, Msg) { m.starred = !m.starred; m.stars += m.starred ? 1 : -1; return m; }

    // a feature card — glass panel, staggered fade-up entrance
    static NodeRef feature(std::string icon, std::string title, std::string desc, int delay_ms) {
        return col(
            text(icon) | font(30),
            text(title) | fg(ink) | subtitle | weight(Weight::bold),
            text(desc)  | fg(muted) | body
        ) | gap(10) | pad(24) | round(20)
          | glass(16, 0xffffff, 0.05f)
          | grow(1) | css("min-width", "15rem")
          | fade_up(600) | delay(delay_ms)
          | transition() | on(Hover, css("transform", "translateY(-4px)"));
    }

    static NodeRef view(const Model& m) {
        auto pill = text("v1.0 \u00b7 now with typed messages") | fg(brand2) | caption | semibold
                  | pad_x(14) | pad_y(6) | round(999)
                  | css("background", "rgba(34,211,238,.10)") | css("border", "1px solid rgba(34,211,238,.25)")
                  | fade_in(500);

        auto hero_ = col(
            pill,
            text("Build live UIs in C++") | display | weight(Weight::black)
                | font_fluid(40, 76) | css("letter-spacing", "-.02em")
                | gradient_text(0x818cf8, 0x22d3ee) | fade_up(700),
            text("Describe what to render \u2014 waya owns how. Server-rendered, "
                 "real-time, keyed-diffed, and beautiful by default.")
                | fg(muted) | font_fluid(16, 20) | css("max-width", "40rem")
                | css("line-height", "1.6") | fade_up(800) | delay(80),
            row(
                text("Get started \u2192") | fg_on_primary | semibold | font(16)
                    | pad_x(24) | pad_y(14) | round(12) | bg(brand)
                    | glow(brand, 28) | tap(Star{})
                    | transition() | on(Hover, css("transform", "translateY(-2px)")),
                row(
                    text(m.starred ? "\u2605" : "\u2606") | fg(m.starred ? warn : muted) | font(18),
                    text("Star  " + std::to_string(m.stars)) | fg(ink) | semibold | font(16)
                ) | gap(8) | center | pad_x(20) | pad_y(14) | round(12)
                  | css("background", "rgba(255,255,255,.04)") | css("border", "1px solid rgba(255,255,255,.10)")
                  | tap(Star{}) | css("cursor", "pointer") | transition() | on(Hover, css("background", "rgba(255,255,255,.08)"))
            ) | gap(14) | wrap | center | fade_up(900) | delay(160)
        ) | gap(24) | center | css("text-align", "center");

        auto features = row(
            feature("\u26a1", "Real-time", "Only the changed nodes stream over a binary WebSocket. Tiny, instant.", 1000),
            feature("\U0001f9ec", "Typed", "Messages are a std::variant. Payloads, exhaustive matching, no strings.", 1120),
            feature("\U0001f3a8", "Beautiful", "Motion, glass, themes, fluid type \u2014 all one-line mods.", 1240)
        ) | gap(20) | wrap | css("width", "100%");

        // ambient background glow behind the hero
        return page(0x090b14,
            centered(60,
                col(hero_, features) | gap(56) | center
            ) | center
        ) | css("background",
            "radial-gradient(60rem 40rem at 50% -10%, rgba(99,102,241,.18), transparent 60%),"
            "radial-gradient(50rem 40rem at 90% 10%, rgba(34,211,238,.10), transparent 55%), #090b14")
          | css("overflow-x", "hidden");
    }
};

int main() {
    static_assert(SurfaceProgram<Splash>);
    return live<Splash>({ .port = 8080, .page_bg = 0x090b14, .title = "waya \u2014 live UIs in C++" });
}
