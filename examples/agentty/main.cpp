// examples/agentty/main.cpp — the agentty.org site, ported to waya
// component-by-component, faithful to the Next.js original.
//
// Structure mirrors app/layout.tsx + app/page.tsx: a fixed SiteNav, the
// homepage <main>, and the SiteFooter. Every section is a real agentty
// component (examples/agentty/components/*) built on waya's core vocabulary +
// the client-owned motion primitives (reveal / typewriter / count_up / magnetic
// / scroll_progress / theme_toggle). Server-rendered once, then silent.
//
//   waya run agentty          (or: WAYA_PORT=8080 ./build/agentty)

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "components/theme.hpp"
#include "components/nav.hpp"
#include "components/page_home.hpp"

using namespace waya::surface;
using namespace waya::ui;

struct App {
    struct Model { std::string copied; };
    struct Copy { std::string cmd; };        // a copy button was clicked
    using Msg = std::variant<Copy>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Copy c) -> std::pair<Model, Cmd<Msg>> {
                m.copied = c.cmd;
                return { m, Cmd<Msg>::copy(c.cmd) };   // real clipboard write
            },
        }, msg);
    }

    static NodeRef view(const Model&) {
        const std::string install = "curl -fsSL https://agentty.org/install.sh | sh";

        // layout.tsx: <SiteNav/> · <main>{page}</main> · <SiteFooter/>
        return box(
            scroll_progress(0x58a6ff, 0xd2a8ff),
            agentty::site_nav("v0.2.4",
                { {"Docs", "/docs"}, {"Install", "/docs/installation"},
                  {"Manual", "/docs/interface"}, {"Blog", "/blog"}, {"Community", "/community"} }),
            agentty::home(install, Copy{ install }),
            agentty::site_footer());
    }

    static Meta meta(const Model&) {
        return { .title = "agentty — a blazing-fast coding agent in your terminal",
                 .description = "A native C++26 terminal coding agent: one static binary, "
                                "millisecond cold start, sandboxed by default, any model.",
                 .type = "website" };
    }
};

int main() { return live<App>({ .port = 8080, .page_bg = 0x0d1117, .title = "agentty" }); }
