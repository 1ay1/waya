/// examples/orbit.cpp — ORBIT: live generative art. A constellation of nodes
/// swings around three moving attractors; every node is linked to its nearest
/// neighbours, so the whole web breathes and re-weaves ~30x a second. Pure math
/// becomes an SVG string, handed to waya as one node — and only the delta ships
/// each frame. A speed control and a node-count stepper are real state.
///
///   waya run orbit            # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <cmath>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Orbit {
    struct Model {
        long  t      = 0;
        int   n      = 48;      // node count
        int   speed  = 5;       // 1..10
        bool  running = true;
    };

    struct Tick {}; struct Faster {}; struct Slower {}; struct More {}; struct Fewer {}; struct Toggle {};
    using Msg = std::variant<Tick, Faster, Slower, More, Fewer, Toggle>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        std::visit(overload{
            [&](Tick)   { m.t += m.speed; },
            [&](Faster) { if (m.speed < 10) m.speed++; },
            [&](Slower) { if (m.speed > 1)  m.speed--; },
            [&](More)   { if (m.n < 90) m.n += 6; },
            [&](Fewer)  { if (m.n > 12) m.n -= 6; },
            [&](Toggle) { m.running = !m.running; },
        }, msg);
        return { m, Cmd<Msg>::none() };
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.running ? Sub<Msg>::every(std::chrono::milliseconds(33), Tick{})
                         : Sub<Msg>::none();
    }

    // Build the whole scene as one SVG string. This is where "anything renders"
    // pays off: it's just math -> a string -> a node.
    static std::string scene(const Model& m) {
        const float W = 900, H = 560;
        const float tt = m.t * 0.012f;

        struct P { float x, y; std::uint32_t c; };
        std::vector<P> pts;
        pts.reserve(m.n);

        // three drifting attractors
        auto attractor = [&](int k) {
            float a = tt * (0.6f + 0.2f * k) + k * 2.1f;
            return P{ W * (0.5f + 0.32f * std::cos(a)),
                      H * (0.5f + 0.30f * std::sin(a * 1.3f)), 0 };
        };
        P A0 = attractor(0), A1 = attractor(1), A2 = attractor(2);

        // palette lerp across three hues
        auto lerp_hue = [](std::uint32_t a, std::uint32_t b, float f) {
            auto ch = [&](int s){ int x=(a>>s)&0xff, y=(b>>s)&0xff; return (std::uint32_t)(x+(y-x)*f); };
            return (ch(16)<<16)|(ch(8)<<8)|ch(0);
        };

        for (int i = 0; i < m.n; ++i) {
            float u  = (float)i / m.n;
            float ang = u * 6.2831853f * 3.0f + tt;
            float rad = 60 + 200 * u;
            // each node orbits a blend of the three attractors
            float bx = A0.x * (1-u) + A1.x * u * 0.6f + A2.x * (1-u) * 0.4f;
            float by = A0.y * (1-u) + A2.y * u * 0.7f + A1.y * (1-u) * 0.3f;
            float x = bx + rad * std::cos(ang) * (0.5f + 0.5f * std::sin(tt + u * 4));
            float y = by + rad * std::sin(ang) * (0.5f + 0.5f * std::cos(tt * 0.8f + u * 3));
            std::uint32_t c = u < 0.5f
                ? lerp_hue(0x22d3ee, 0xa78bfa, u * 2)
                : lerp_hue(0xa78bfa, 0xf472b6, (u - 0.5f) * 2);
            pts.push_back({ x, y, c });
        }

        auto hexstr = [](std::uint32_t c){
            static const char* Hx = "0123456789abcdef"; std::string o = "#";
            for (int s = 20; s >= 0; s -= 4) o += Hx[(c >> s) & 0xF]; return o;
        };
        auto num = [](float f){ char b[32]; std::snprintf(b, sizeof b, "%.1f", f); return std::string(b); };

        std::string svg;
        svg.reserve(m.n * 220);
        svg += "<svg viewBox='0 0 " + num(W) + " " + num(H) +
               "' preserveAspectRatio='xMidYMid meet' "
               "style='width:100%;height:100%;display:block'>";
        svg += "<defs><filter id='g'><feGaussianBlur stdDeviation='2.4'/></filter></defs>";

        // links: connect near neighbours
        svg += "<g stroke-linecap='round'>";
        for (int i = 0; i < m.n; ++i) {
            for (int j = i + 1; j < m.n; ++j) {
                float dx = pts[i].x - pts[j].x, dy = pts[i].y - pts[j].y;
                float d2 = dx*dx + dy*dy;
                if (d2 < 9000) {
                    float a = 0.5f * (1 - d2 / 9000);
                    svg += "<line x1='" + num(pts[i].x) + "' y1='" + num(pts[i].y) +
                           "' x2='" + num(pts[j].x) + "' y2='" + num(pts[j].y) +
                           "' stroke='" + hexstr(pts[i].c) + "' stroke-width='1' opacity='" +
                           num(a) + "'/>";
                }
            }
        }
        svg += "</g>";

        // glowing nodes
        svg += "<g filter='url(#g)'>";
        for (auto& p : pts)
            svg += "<circle cx='" + num(p.x) + "' cy='" + num(p.y) + "' r='3.2' fill='" +
                   hexstr(p.c) + "'/>";
        svg += "</g><g>";
        for (auto& p : pts)
            svg += "<circle cx='" + num(p.x) + "' cy='" + num(p.y) + "' r='1.6' fill='#fff'/>";
        svg += "</g>";

        svg += "</svg>";
        return svg;
    }

    static NodeRef ctrl(std::string label, Msg msg) {
        return text(std::move(label)) | font(15) | semibold | fg(ink)
             | square(38) | center | round(10)
             | frost(12) | hairline(0xffffff, 0.14f)
             | interactive() | hover_bg(0xffffff, 0.08f) | tap(msg);
    }

    static NodeRef stat_(std::string k, std::string v) {
        return col(text(k) | fg(faint) | font(11) | uppercase | tracking_em(0.10f),
                   text(v) | fg(ink) | font(15) | semibold | tabular_nums) | gap(1);
    }

    static NodeRef view(const Model& m) {
        auto canvas = markup(scene(m))
            | w_full | h_full
            | detail::raw_css("line-height", "0");

        auto stage = box(canvas)
            | w_full | round(22) | clip_content
            | detail::raw_css("aspect-ratio", "900/560")
            | radial(0x101a33, 60, 40, 0x070a16)
            | hairline(0xffffff, 0.08f) | elevation(4);

        auto bar = row(
            row(box() | circle(9) | bg(m.running ? 0x34d399 : 0x64748b)
                    | (m.running ? breathe() : noop),
                text("Orbit") | fg(ink) | font(18) | weight(Weight::black),
                text("generative \u00b7 ~30fps") | fg(faint) | font(12)) | gap(10) | items_center,
            box() | grow(),
            row(stat_("nodes", std::to_string(m.n)),
                stat_("speed", std::to_string(m.speed) + "x"),
                stat_("frame", std::to_string(m.t))) | gap(22),
            box() | grow(),
            row(ctrl("\u2212", Fewer{}), ctrl("+", More{}),
                box() | w(1) | h(24) | tint(0xffffff, 0.12f),
                ctrl("\u25C2", Slower{}), ctrl("\u25B8", Faster{}),
                box() | w(1) | h(24) | tint(0xffffff, 0.12f),
                text(m.running ? "\u2758\u2758" : "\u25B6") | font(13) | fg(ink)
                    | square(38) | center | round(10)
                    | gradient(0x8b5cf6, 0x6366f1, 135) | glow(0x8b5cf6, 16)
                    | interactive() | tap(Toggle{}))
                | gap(8) | items_center
        ) | items_center | gap(18) | wrap | w_full
          | pad(16) | round(16) | frost(14) | hairline(0xffffff, 0.08f);

        return col(bar, stage)
             | gap(18) | pad(24) | max_w(1000) | center_x | min_h(100_vh) | justify_center
             | aurora(0x11072a, 0x070a16, 0x0a1f2e, 30)
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Orbit";
        mt.description = "Live generative SVG art, ticked server-side and streamed as deltas.";
        return mt;
    }
};

int main() {
    return live<Orbit>({ .port = 8080, .title = "waya \u00b7 orbit" });
}
