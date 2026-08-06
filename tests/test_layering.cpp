// tests/test_layering.cpp — enforces the module-layering invariant: the whole
// Model -> update -> view -> diff loop compiles and runs using ONLY the UI core
// and "the ideas", WITHOUT the runtime (no surface/live.hpp, no sockets). If
// this file ever needs <waya/surface/live.hpp> to build, the layering has
// regressed and the core has grown a transport dependency.
#include <waya/surface/node.hpp>       // UI core
#include <waya/surface/dom.hpp>        // UI core: render
#include <waya/surface/diff.hpp>       // UI core: diff
#include <waya/surface/program.hpp>    // the ideas: Program hooks
#include <waya/surface/effect.hpp>     // the ideas: Cmd / Sub
// deliberately NOT included: surface/live.hpp, net/ws.hpp, net/http.hpp

#include <iostream>
#include <string>
#include <variant>

using namespace waya::surface;

// A complete app, defined with only the core + the ideas.
struct App {
    struct Model { int n = 0; };
    struct Inc {}; struct Dec {};
    using Msg = std::variant<Inc, Dec>;
    static Model init() { return {}; }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        std::visit(overload{ [&](Inc){ ++m.n; }, [&](Dec){ --m.n; } }, msg);
        return { m, Cmd<Msg>::none() };
    }
    static NodeRef view(const Model& m) {
        return col(text("count"), text(m.n) | bold) | gap(8) | pad(16);
    }
};

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }

int main() {
    // the concept + diagnostics are available WITHOUT the runtime
    static_assert(SurfaceProgram<App>, "App is a valid Program (core+ideas only)");

    // run the full loop by hand — exactly what a runtime would do, minus sockets
    auto [m0, c0] = detail::init_of<App, App::Model, App::Msg>();
    (void)c0;
    NodeRef v0 = App::view(m0);

    auto [m1, c1] = detail::dispatch<App, App::Model, App::Msg>(m0, App::Inc{}, "");
    (void)c1;
    NodeRef v1 = App::view(m1);

    // the core can render + diff, no runtime needed
    auto out = DomBackend{}.render(*v1);
    check(out.html.find("1") != std::string::npos, "view renders updated model");

    auto patch = diff(v0, v1);
    check(patch.size() == 1 && patch[0].op == Op::set_text, "update -> one set_text delta");

    // subscriptions + meta hooks resolve through the ideas layer
    auto sub = detail::subs_of<App, App::Model, App::Msg>(m1);
    check(sub.is_none(), "no-subscribe app -> Sub::none");
    auto meta = detail::meta_of<App, App::Model>(m1);
    check(meta.title.empty(), "no-meta app -> blank Meta");

    std::cout << "test_layering: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
