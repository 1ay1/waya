/// examples/counter.cpp — the smallest waya app.
///
///   cmake --build build -j && ./build/counter    # http://localhost:8080
///
/// Model + Msg + init/update/view. `view` describes a surface with box/text +
/// chaining attrs + tap(msg). waya renders it and streams only what changed.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

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
        auto btn = [](std::string label, int msg, std::uint32_t c) {
            return text(std::move(label)) | fg(0xffffff) | font(20) | semibold
                 | pad_x(20) | pad_y(12) | bg(c) | round(12) | tap(msg)
                 | transition() | on(Hover, opacity(0.85f));
        };
        auto card = col(
            text(m.n) | fg(0x818cf8) | font_fluid(56, 88) | weight(Weight::black)
                      | css("font-variant-numeric", "tabular-nums"),
            row(
                btn("−", Dec, 0x334155),
                btn("reset", Reset, 0x1e293b),
                btn("+", Inc, 0x6366f1)
            ) | gap(12) | wrap | center
        ) | center | gap(28) | pad_fluid(28, 48) | round(24) | bg(0x111827)
          | shadow() | border(1, 0x1f2937);
        return page(color::bg0, centered(24, card) | center);
    }
};

int main() {
    static_assert(SurfaceProgram<Counter>);
    return live<Counter>({.port = 8080});
}
