/// examples/hello.cpp — a waya landing page.
///
///   cmake --build build -j && ./build/hello       # http://localhost:8080

#include <waya/surface/live.hpp>

using namespace waya::surface;

struct Hello {
    struct Model {};
    using Msg = int;
    static Model init() { return {}; }
    static Model update(Model m, Msg) { return m; }

    static NodeRef view(const Model&) {
        auto card = [](std::string title, std::string body) {
            return col(
                text(std::move(title)) | fg(0xf1f5f9) | font(18) | semibold,
                text(std::move(body))  | fg(0x94a3b8) | font(15) | leading(1.6f)
            ) | gap(8) | pad(24) | bg(0x1e293b) | round(16) | border(1, 0x334155)
              | grow(1) | w(px(260))
              | transition() | on(Hover, css("transform","translateY(-4px)"));
        };
        return col(
            text("waya") | font(64) | weight(Weight::black)
                | css("background","linear-gradient(90deg,#818cf8,#22d3ee)")
                | css("-webkit-background-clip","text") | css("background-clip","text")
                | css("color","transparent"),
            text("Describe what to render. waya owns how.") | fg(0x94a3b8) | font(20) | max_w(px(560)),
            row(
                card("One vocabulary", "box, text, image, path. A chart is one node."),
                card("Substrate-free", "waya renders as HTML, canvas, whatever fits. You never know."),
                card("In sync by delta", "The connection carries only what changed. Tiny, live, always.")
            ) | gap(16) | wrap
        ) | gap(28) | pad(56) | bg(0x0b1020) | css("min-height","100vh");
    }
};

int main() {
    static_assert(SurfaceProgram<Hello>);
    return live<Hello>({.port = 8080});
}
