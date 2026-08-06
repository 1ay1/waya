// examples/counter.cpp — the smallest complete waya app, PURE CORE.
//
// No component library, no theme presets — just the surface primitives
// (box/col/row/text) and the universal mod vocabulary. This is the floor:
// everything else in waya, including the entire ui/ component library, is built
// out of exactly these pieces. If you can read this, you can read all of waya.
//
//   cmake --build build --target counter && ./build/counter   # http://localhost:8080

#include <waya/surface/live.hpp>

using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };

    // Msg is a real type — a variant of message structs, matched with std::visit.
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Inc)   { ++m.n; },
            [&](Dec)   { --m.n; },
            [&](Reset) { m.n = 0; },
        }, msg);
        return m;
    }

    // A button, from scratch: a text node + padding + colour + a tap message.
    static NodeRef btn(std::string label, Msg msg, std::uint32_t c) {
        return text(std::move(label))
             | pad_x(18) | pad_y(12) | round(10) | bg(c) | fg(0xffffff) | bold
             | pointer | on(Hover, css("filter", "brightness(1.1)")) | tap(msg);
    }

    static NodeRef view(const Model& m) {
        return col(
            text("waya") | fg(0x94a3b8) | font(14) | css("letter-spacing", ".3em"),
            text(m.n) | font(84) | bold | fg(m.n < 0 ? 0xf87171 : 0xe2e8f0),
            row(
                btn("\u2212", Dec{},   0x334155),
                btn("reset",  Reset{}, 0x1e293b),
                btn("+",      Inc{},   0x6366f1)
            ) | gap(12) | center
        ) | gap(28) | center | pad(48)
          | css("min-height", "100dvh")
          | css("background", "radial-gradient(1200px 600px at 50% -10%, #16213e, #0b1020)");
    }
};

int main() { return live<Counter>({ .port = 8080, .title = "waya · counter" }); }
