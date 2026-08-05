/// examples/hello.cpp — a waya landing page.
///
///   cmake --build build -j && ./build/hello       # http://localhost:8080
///
/// A static-ish page is the same as any waya app: a Model (here, empty) and a
/// view that describes a surface. Everything is box/text/path — waya decides
/// it renders as HTML here, and could render it any other way without changing
/// a line of this file.

#include <waya/surface/live.hpp>

using namespace waya::surface;

struct Hello {
    struct Model {};
    using Msg = int;
    static Model init() { return {}; }
    static Model update(Model m, Msg) { return m; }

    static NodeRef view(const Model&) {
        auto card = [](std::string title, std::string body) {
            return col({
                text(std::move(title)) | fg(0xf1f5f9) | size(18) | bold,
                text(std::move(body))  | fg(0x94a3b8) | size(15),
            }) | gap(8) | pad(24) | bg(0x1e293b) | round_(16) | grow(1);
        };
        return col({
            text("waya") | fg(0x818cf8) | size(56) | bold,
            text("Describe what to render. waya owns how.") | fg(0x94a3b8) | size(19),

            row({
                card("One vocabulary", "box, text, image, path. That's it. A chart is one node."),
                card("Substrate-free", "waya renders it as HTML, canvas, whatever fits. You never know."),
                card("In sync by delta", "The connection carries only what changed. Tiny, live, always."),
            }) | gap(16),
        }) | gap(28) | pad(48) | bg(0x0b1020);
    }
};

int main() {
    static_assert(SurfaceProgram<Hello>);
    return live<Hello>({.port = 8080});
}
