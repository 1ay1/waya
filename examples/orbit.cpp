/// examples/orbit.cpp — live generative art. A field of orbiting nodes drawn as
/// SVG paths, ticked ~30×/s by a subscription. Pure math → a NodeRef; waya
/// streams only what moved. Proof that "anything renders" and that a timer
/// subscription drives smooth real-time motion.
///
///   cmake --build build -j && ./build/orbit     # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <cmath>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

struct Orbit {
    struct Model { double t = 0; bool running = true; int rings = 5; };
    struct Tick {}; struct Toggle {}; struct More {}; struct Less {};
    using Msg = std::variant<Tick, Toggle, More, Less>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        std::visit(overload{
            [&](Tick)   { m.t += 0.05; },
            [&](Toggle) { m.running = !m.running; },
            [&](More)   { if (m.rings < 9) m.rings++; },
            [&](Less)   { if (m.rings > 1) m.rings--; },
        }, msg);
        return { m, Cmd<Msg>::none() };
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.running ? Sub<Msg>::every(33, Tick{}) : Sub<Msg>::none();
    }

    // one orbiting ring: a polyline traced around a centre, phase-shifted by t.
    static NodeRef ring(double t, int idx, int /*total*/) {
        std::vector<Pt> pts;
        double r = 40 + idx * 26;                         // radius grows per ring
        double speed = 1.0 + idx * 0.18;
        int n = 60;
        for (int i = 0; i <= n; ++i) {
            double a = (double)i / n * 2 * M_PI;
            double wob = 8 * std::sin(a * 3 + t * speed); // wobble for organic feel
            double rr = r + wob;
            pts.push_back({ (float)(300 + rr * std::cos(a + t * speed * 0.4)),
                            (float)(300 + rr * std::sin(a + t * speed * 0.4)) });
        }
        // hue shifts across rings and time
        std::uint32_t palette[] = {0x818cf8, 0x22d3ee, 0x34d399, 0xf472b6, 0xfbbf24, 0xa78bfa, 0x38bdf8, 0x4ade80, 0xfb7185};
        return path(pts) | stroke(palette[idx % 9], 2.0f) | css("opacity", "0.85");
    }

    // a glowing dot chasing each ring
    static NodeRef dot(double t, int idx) {
        double r = 40 + idx * 26, speed = 1.0 + idx * 0.18;
        double a = t * speed;
        float x = (float)(300 + r * std::cos(a + t * speed * 0.4));
        float y = (float)(300 + r * std::sin(a + t * speed * 0.4));
        return path({{x-3,y},{x+3,y}}, false) | stroke(0xffffff, 6.0f)
             | css("stroke-linecap", "round");
    }

    static NodeRef view(const Model& m) {
        std::vector<NodeRef> layers;
        for (int i = 0; i < m.rings; ++i) layers.push_back(ring(m.t, i, m.rings));
        for (int i = 0; i < m.rings; ++i) layers.push_back(dot(m.t, i));
        auto canvas = box_(std::move(layers))
            | css("position", "relative") | aspect(1.0f)
            | css("width", "min(80vw, 600px)") | css("max-width", "600px")
            | css("filter", "drop-shadow(0 0 24px rgba(129,140,248,.25))");
        // stack the layers on top of each other
        canvas->style.flow = Flow::stack; finalize(*canvas);

        auto btn = [](std::string label, Msg msg) {
            return text(std::move(label)) | fg(ink) | semibold | font(15)
                 | pad_x(18) | pad_y(10) | round(10) | tint() | hairline()
                 | hover_bg() | interactive() | tap(msg);
        };

        return page(0x07090f,
            centered(48, col(
                text("orbit") | display | mono | css("letter-spacing", ".3em")
                    | aurora_text(0x818cf8, 0x22d3ee, 0xf472b6, 6) | glow_text(0x818cf8, 16),
                text("a generative field, ticked 30×/s by a subscription") | fg(muted) | caption,
                canvas | center,
                row(
                    btn(m.running ? "❘❘ pause" : "▶ play", Toggle{}),
                    btn("− ring", Less{}),
                    btn("+ ring", More{})
                ) | gap(12) | wrap | center
            ) | gap(24) | center) | center
        ) | radial(0x6366f1, 50, 50, 0x07090f, 50);
    }
};

int main() {
    static_assert(SurfaceProgram<Orbit>);
    return live<Orbit>({ .port = 8080, .page_bg = 0x07090f, .title = "waya \u2014 orbit" });
}
