// examples/typed.cpp — the maya-faithful dialect: type-state layout gates,
// typed colours, length literals. Building web UI feels like building a TUI —
// and a layout mistake (gap on a non-container) is a COMPILE error, not a
// silent no-op.
//
//   cmake --build build --target typed && ./build/typed   # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/typed.hpp>

using namespace waya::tui;                // the typed dialect: Row/Col/Box/Text + gated mods
using namespace waya::color;              // typed colours: indigo, ink, rgba(...)
using namespace waya::surface::literals;  // length literals: 16_px, 1.5_rem

struct App {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {};
    using Msg = std::variant<Inc, Dec>;

    static Model init() { return {}; }
    static Model update(Model m, Msg msg) {
        std::visit(overload{ [&](Inc){ ++m.n; }, [&](Dec){ --m.n; } }, msg);
        return m;
    }

    // A component is a function returning a typed node. The context (Flex) is in
    // the type, so gap/justify/align here are checked at COMPILE time.
    static waya::surface::NodeRef btn(std::string label, Msg m, waya::Color c) {
        return Box(Text(std::move(label)))
             | pad_x(18) | pad_y(12) | round(10_px) | bg(c) | fg(white)
             | bold | no_select | pointer | hover_lift(2) | tap(m);
    }

    static waya::surface::NodeRef view(const Model& m) {
        return Col(
            Text("waya") | fg(muted) | tracking(6.f) | uppercase | font(13_px),
            Text(m.n) | font(84_px) | bold | tabular_nums
                      | fg(m.n < 0 ? rose : ink),
            Row(
                btn("-", Dec{}, slate800),
                btn("+", Inc{}, indigo)
            ) | gap(12) | justify_center           // <- gap/justify GATED to Row
        )
        | gap(28) | align_center | justify_center  // <- gated to Col
        | pad(48_px) | h(100_vh)
        | bg(rgba(11, 16, 32, 1.0f));
        // Try it: put `| gap(8)` on the inner `Text(m.n)` above and it WON'T
        // compile — gap needs a flex/grid container. That's the guarantee.
    }
};

int main() { return waya::surface::live<App>({ .port = 8080, .title = "waya \u00b7 typed" }); }
