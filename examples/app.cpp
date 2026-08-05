/// examples/app.cpp — a waya app. This is how you build UIs with waya.
///
///   cmake --build build -j && ./build/app        # http://localhost:8080
///
/// Describe a surface with a tiny vocabulary — box / text / image / path +
/// chaining attrs + tap(msg). waya renders it (as HTML here), streams only the
/// delta on each tap, and keeps the browser in sync. Not one line mentions
/// HTML, CSS, a div, flex, onclick, or a canvas; those are waya's business.

#include <waya/surface/live.hpp>

#include <vector>

using namespace waya::surface;

struct Dashboard {
    struct Model {
        int requests = 0;
        std::vector<float> history{20, 34, 28, 45, 39, 52, 47, 60};
    };
    using Msg = int;
    enum { Add, Reset };

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        if (msg == Add) {
            m.requests++;
            m.history.push_back(20 + (float)((m.requests * 13) % 55));
            if (m.history.size() > 20) m.history.erase(m.history.begin());
        } else {
            m = Model{};
        }
        return m;
    }

    static NodeRef view(const Model& m) {
        // The chart — one primitive. Any shape is `path`; nothing is off-limits.
        std::vector<Pt> chart;
        for (std::size_t i = 0; i < m.history.size(); ++i)
            chart.push_back({(float)i * 26.f, 90.f - m.history[i]});

        auto card = [](NodeRef body) {
            return body | pad(24) | bg(0x1e293b) | round(16) | border(1, 0x334155)
                        | shadow() | grow(1)
                        | transition() | on(Hover, css("transform", "translateY(-3px)"));
        };
        auto stat = [&](std::string label, NodeRef value) {
            return card(col(
                text(std::move(label)) | fg(0x94a3b8) | font(13) | css("text-transform","uppercase") | tracking(0.6f),
                std::move(value)
            ) | gap(6));
        };

        return col(
            // hero
            col(
                text("waya") | font(48) | weight(Weight::black)
                    | css("background","linear-gradient(90deg,#818cf8,#22d3ee)")
                    | css("-webkit-background-clip","text") | css("background-clip","text")
                    | css("color","transparent"),
                text("Describe what to render. waya owns how.") | fg(0x94a3b8) | font(18)
            ) | gap(6),

            // stat row — responsive: side by side, stacks narrow
            row(
                stat("Requests", text(m.requests) | fg(0xe2e8f0) | font(44) | bold),
                stat("Traffic",  path(chart) | stroke(0x22d3ee, 2) | h(90))
            ) | gap(16) | wrap | at(Md, css("flex-wrap","nowrap")),

            // actions
            row(
                text("＋ request") | fg(0xffffff) | pad_x(20) | pad_y(12) | bg(0x6366f1)
                    | round(10) | font(15) | semibold | tap(Add)
                    | transition() | on(Hover, bg(0x4f46e5)),
                text("reset") | fg(0xe2e8f0) | pad_x(20) | pad_y(12) | bg(0x334155)
                    | round(10) | font(15) | tap(Reset)
            ) | gap(12)
        ) | gap(28) | pad(40) | bg(0x0b1020) | css("min-height","100vh");
    }
};

int main() {
    static_assert(SurfaceProgram<Dashboard>);
    return live<Dashboard>({.port = 8080});
}
