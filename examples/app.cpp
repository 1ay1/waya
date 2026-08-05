/// examples/app.cpp — a waya app. This is how you build UIs with waya.
///
///   cmake --build build -j && ./build/app        # http://localhost:8080
///
/// Everything is a node; everything you do to a node is a `Mod` you pipe with
/// `|`. Style, layout, state, interactivity — one uniform, composable API. Not
/// a line of HTML, CSS, div, flex, onclick, or canvas. A delight to write.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

struct App {
    struct Model {
        std::string name;
        int likes = 0;
        std::vector<float> traffic{20, 34, 28, 45, 39, 52, 47, 60};
    };
    using Msg = int;
    enum { Like, Reset, NameChanged };

    static Model init() { return {}; }

    static Model update(Model m, Msg msg, std::string value) {
        switch (msg) {
            case Like:  m.likes++;
                        m.traffic.push_back(20 + (float)((m.likes * 13) % 55));
                        if (m.traffic.size() > 20) m.traffic.erase(m.traffic.begin());
                        break;
            case Reset: m = Model{}; break;
            case NameChanged: m.name = value; break;
        }
        return m;
    }

    static NodeRef view(const Model& m) {
        // the chart — one primitive; any shape is `path`
        std::vector<Pt> chart;
        for (std::size_t i = 0; i < m.traffic.size(); ++i)
            chart.push_back({(float)i * 26.f, 90.f - m.traffic[i]});

        auto card = [](NodeRef body){
            return body | pad(sp(6)) | bg(bg2) | round(16) | border(1, line) | shadow()
                        | transition()
                        | on(Hover, css("transform","translateY(-3px)"), css("border-color","#475569"));
        };

        std::string greeting = m.name.empty() ? "there" : m.name;

        return center_col(
            // hero
            col(
                text("waya") | fluid_font(40, 64) | weight(Weight::black)
                    | gradient_text(0x818cf8, brand2),
                text("Everything is a node. Everything composes.") | fg(muted) | font(18)
            ) | gap(sp(1)),

            // a real input, in the same vocabulary
            card(col(
                text("What's your name?") | fg(muted) | font(13) | tracking(0.5f),
                input(m.name) | placeholder("type here…") | on_input(NameChanged)
                    | pad(sp(3)) | round(10) | bg(bg0) | fg(ink) | font(16)
                    | border(1, line)
                    | on(Focus, css("border-color","#6366f1")),
                text("Hi, " + greeting + " 👋") | fg(ink) | font(20) | semibold
            ) | gap(sp(2))),

            // stats — a responsive grid: as many columns as fit, no breakpoints
            grid(px(220),
                card(col(
                    text("Likes") | fg(muted) | font(13),
                    text(m.likes) | fg(ink) | font(44) | bold
                ) | gap(sp(1))),
                card(col(
                    text("Traffic") | fg(muted) | font(13),
                    path(chart) | stroke(brand2, 2) | h(90)
                ) | gap(sp(2)))
            ),

            // actions — a cluster wraps by itself when narrow
            cluster(
                text("♥ like") | fg(white) | pad_x(sp(5)) | pad_y(sp(3)) | bg(brand)
                    | round(10) | font(15) | semibold | tap(Like)
                    | transition() | on(Hover, bg(0x4f46e5)) | on(Active, scale(0.97f)),
                text("reset") | fg(ink) | pad_x(sp(5)) | pad_y(sp(3)) | bg(bg2)
                    | round(10) | font(15) | tap(Reset)
            )
        ) | gap(sp(7)) | pad(sp(8)) | bg(bg0);
    }
};

int main() {
    static_assert(SurfaceProgram<App>);
    return live<App>({.port = 8080});
}
