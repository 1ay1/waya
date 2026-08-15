// examples/agentty/main.cpp — the full agentty.org site, ported to waya.
//
// A routed multi-page app: home, docs (fetched from the agentty repo's markdown
// and rendered live), blog index + posts, and standalone pages. The nav, footer,
// scroll-progress and ⌘K palette wrap every route. Client-side navigation keeps
// m.path in sync via Sub::on_route; each route resolves to its page builder.
//
//   waya run agentty          (or: WAYA_PORT=8080 ./build/agentty)

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "components/theme.hpp"
#include "components/nav.hpp"
#include "components/page_home.hpp"
#include "components/pages.hpp"

using namespace waya::surface;
using namespace waya::ui;

struct App {
    struct Model { std::string path = "/"; std::string copied; };
    struct Copy { std::string cmd; };
    struct Nav  { std::string path; };      // URL changed
    using Msg = std::variant<Copy, Nav>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Copy c) -> std::pair<Model, Cmd<Msg>> {
                m.copied = c.cmd;
                return { m, Cmd<Msg>::copy(c.cmd) };
            },
            [&](Nav n) -> std::pair<Model, Cmd<Msg>> {
                if (n.path == m.path) return { m, Cmd<Msg>::none() };  // no-op: same route
                m.path = n.path;
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    // keep m.path in sync with the browser URL (SSR + client nav both route here)
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_route([](std::string p) { return Msg{ Nav{ std::move(p) } }; });
    }

    static NodeRef view(const Model& m) {
        const std::string install = "curl -fsSL https://agentty.org/install.sh | sh";

        // resolve the current path to a page body
        auto routes = Routes()
            .at("/",              [&](const Match&)  { return agentty::home(install, Copy{ install }); })
            .at("/docs",          [](const Match&)   { return agentty::docs_page(""); })
            .at("/docs/*",        [](const Match& r) { return agentty::docs_page(r.param("*")); })
            .at("/blog",          [](const Match&)   { return agentty::blog_index(); })
            .at("/blog/:slug",    [](const Match& r) { return agentty::blog_post(r.param("slug")); })
            .at("/community",     [](const Match&)   { return agentty::simple_page("Community",
                    "<p class=\"lead\">agentty is built in the open. Join the Discord, file issues, and help shape it.</p>"
                    "<p>\xe2\x86\x92 <a href=\"https://discord.gg/qhb9AZ8f3c\">Join the Discord</a></p>"
                    "<p>\xe2\x86\x92 <a href=\"https://github.com/1ay1/agentty\">GitHub repository</a></p>"); })
            .at("/contributing",  [](const Match&)   { return agentty::simple_page("Contributing",
                    "<p class=\"lead\">Bug reports, fixes, and well-scoped features are welcome.</p>"
                    "<p>Start by cloning <a href=\"https://github.com/1ay1/agentty\">the repo</a> and reading the build docs.</p>"); })
            .at("/license",       [](const Match&)   { return agentty::simple_page("License",
                    "<p class=\"lead\">agentty is MIT licensed.</p><p>Fork it, ship it, build on it.</p>"); })
            .fallback([&]() { return agentty::simple_page("Not found",
                    "<p class=\"lead\">That page doesn't exist.</p><p><a href=\"/\">\xe2\x86\x90 Back home</a></p>"); });

        return box(
            scroll_progress(0x5b9dff, 0xc99bff),
            agentty::site_nav("v0.2.4",
                { {"Docs", "/docs"}, {"Install", "/docs/installation"},
                  {"Manual", "/docs/interface"}, {"Blog", "/blog"}, {"Community", "/community"} }),
            box(routes.view(m.path)) | as("main") | attr("id", "main") | key("route:" + m.path),
            agentty::site_footer(),
            agentty::command_palette(
                { {"Docs", "/docs"}, {"Install", "/docs/installation"},
                  {"Manual", "/docs/interface"}, {"Blog", "/blog"}, {"Community", "/community"} }));
    }

    static Meta meta(const Model& m) {
        std::string title = "agentty — a blazing-fast coding agent in your terminal";
        if (m.path.rfind("/docs", 0) == 0) title = "Docs · agentty";
        else if (m.path.rfind("/blog", 0) == 0) title = "Blog · agentty";
        return { .title = title,
                 .description = "A native C++26 terminal coding agent: one static binary, "
                                "millisecond cold start, sandboxed by default, any model.",
                 .type = "website" };
    }
};

int main() { return live<App>({ .port = 8080, .page_bg = 0x0b0e14, .title = "agentty" }); }
