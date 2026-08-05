/// examples/counter.cpp — a live counter. The Elm architecture, running on the
/// server: click a button → Msg → update → re-render, no page reload.
///
///   cmake --build build -j && ./build/counter    # http://localhost:8080
///
/// Model/Msg/update are pure (see tests/test_program.cpp). view() wires clicks
/// to messages with `on_msg`. The runtime holds the Model, runs update on each
/// event, and swaps the fresh HTML into the page.

#include <waya/waya.hpp>
#include <waya/app/live_ws.hpp>

#include <variant>

using namespace waya;
using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

struct Counter {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Inc)   { return std::pair{Model{m.n + 1}, Cmd<Msg>::none()}; },
            [&](Dec)   { return std::pair{Model{m.n - 1}, Cmd<Msg>::none()}; },
            [&](Reset) { return std::pair{Model{0},       Cmd<Msg>::none()}; },
        }, msg);
    }

    static auto view(const Model& m) {
        auto btn = [](std::string_view label) {
            return button_(text(label))
                | width(52_px) | pad_y(10_px) | rounded(10_px)
                | size(20_px) | weight(Weight::w600)
                | prop<"border", "1px solid #334155"> | pointer
                | bg(0x1e293b) | fg(0xe2e8f0)
                | prop<"transition", "background .12s ease">
                | on<Hover>(bg(0x334155));
        };
        return div_(
            div_(text(std::to_string(m.n)))
                | size(72_px) | weight(Weight::w800) | fg(0x818cf8)
                | prop<"font-variant-numeric", "tabular-nums">,
            div_(
                btn("−") | on_msg(Msg{Dec{}}),
                btn("↺") | on_msg(Msg{Reset{}}) | size(16_px),
                btn("+") | on_msg(Msg{Inc{}})
            ) | flex(Dir::row) | gap(10_px)
        )
        | flex(Dir::col) | items(Align::center) | gap(24_px)
        | pad(48_px) | rounded(20_px) | bg(0x0f172a)
        | prop<"border", "1px solid #1e293b">
        | prop<"box-shadow", "0 20px 60px rgba(0,0,0,.5)">;
    }
};

int main() {
    static_assert(Program<Counter>);
    // Persistent WebSocket: patches STREAM to the browser, no request per click.
    return waya::live_ws<Counter>({.port = 8080});
}
