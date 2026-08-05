/// examples/counter.cpp — the smallest waya app.
///
///   cmake --build build -j && ./build/counter    # http://localhost:8080
///
/// The Elm shape: Model + Msg + init/update/view. `view` returns a surface you
/// describe with box/text + tap(msg). waya renders it and streams only what
/// changed on each tap. No HTML, no CSS, no event wiring in your code.

#include <waya/surface/live.hpp>

using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };
    using Msg = int;
    enum { Inc, Dec, Reset };

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        if (msg == Inc)   m.n++;
        if (msg == Dec)   m.n--;
        if (msg == Reset) m.n = 0;
        return m;
    }

    static NodeRef view(const Model& m) {
        auto btn = [](std::string label, int msg, std::uint32_t color) {
            return text(std::move(label)) | fg(0xffffff) | size(22)
                 | pad(12) | bg(color) | round_(12) | tap(msg);
        };
        return col({
            text(m.n) | fg(0x818cf8) | size(72) | bold,
            row({
                btn("-", Dec, 0x334155),
                btn("reset", Reset, 0x1e293b),
                btn("+", Inc, 0x6366f1),
            }) | gap(10),
        }) | gap(24) | pad(48) | bg(0x0b1020);
    }
};

int main() {
    static_assert(SurfaceProgram<Counter>);
    return live<Counter>({.port = 8080});
}
