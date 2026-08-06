/// examples/counter.cpp — the smallest waya app, with a TYPED Msg.
///
///   cmake --build build -j && ./build/counter    # http://localhost:8080
///
/// Msg is a std::variant of message structs (maya/Elm) — type-safe and
/// payload-carrying. `tap(Inc{})` wires a typed message; `update` matches with
/// std::visit. No magic ints anywhere.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <variant>

using namespace waya::surface;

struct Counter {
    struct Model { int n = 0; };

    // The Program's messages — a variant of small structs.
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Inc)   { m.n++; },
            [&](Dec)   { m.n--; },
            [&](Reset) { m.n = 0; },
        }, msg);
        return m;
    }

    static NodeRef view(const Model& m) {
        auto btn = [](std::string label, Msg msg, std::uint32_t c) {
            return text(std::move(label)) | fg(0xffffff) | font(20) | semibold
                 | pad_x(20) | pad_y(12) | bg(c) | round(12) | tap(msg)
                 | transition() | on(Hover, opacity(0.85f));
        };
        auto card = col(
            text(m.n) | fg(0x818cf8) | font_fluid(56, 88) | weight(Weight::black)
                      | css("font-variant-numeric", "tabular-nums"),
            row(
                btn("−", Dec{}, 0x334155),
                btn("reset", Reset{}, 0x1e293b),
                btn("+", Inc{}, 0x6366f1)
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
