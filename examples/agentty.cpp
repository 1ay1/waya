// examples/agentty.cpp — a landing page built with waya's site toolkit.
//
// A full marketing homepage — nav, hero, stats, comparison table, feature grid,
// providers, install CTA, footer — assembled from ui/site.hpp. Every section is
// ONE call; there's no raw CSS at the call site. Server-rendered, so the whole
// page is crawlable on first paint. The copy is modelled on agentty.org (the
// author's own site) to show the toolkit on a real page.
//
//   waya run agentty          (or: WAYA_PORT=8080 ./build/agentty)

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

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
        // A GitHub-dark theme (the site toolkit's default), tweak-able in one struct.
        SiteTheme theme{};

        const std::string install = "curl -fsSL https://agentty.org/install.sh | sh";

        return site_page(theme,
            // ── NAV ──────────────────────────────────────────────────────────
            site_nav("agentty", "v0.2.4",
                { {"Docs", "/docs"}, {"Install", "/docs/installation"},
                  {"Blog", "/blog"}, {"Community", "/community"} },
                nav_cta("GitHub", "https://github.com/1ay1/agentty", false),
                nav_cta("Get started", "/docs/quick-start")),

            // ── HERO ─────────────────────────────────────────────────────────
            site_hero(
                "Blazing-fast coding agent",
                "in your terminal.",
                "A drop-in alternative to claude-code, written in C++26. One static "
                "binary, millisecond cold start, sandboxed by default, and it signs "
                "in with your existing Claude Pro/Max.",
                cta_row(
                    cta_primary("Quick start", "/docs/quick-start"),
                    cta_ghost("Star on GitHub", "https://github.com/1ay1/agentty"),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c"))),

            // ── STATS ────────────────────────────────────────────────────────
            stats_row(
                site_stat("13 MB", "static binary"),
                site_stat("~2 ms", "cold start"),
                site_stat("0", "runtimes to install"),
                site_stat("6+", "model providers")),

            // ── SPEED (comparison table) ─────────────────────────────────────
            site_section("Speed", "Native, not interpreted.",
                "Measured on the same box, same shell, same day. No JIT warmup, no "
                "require() graph to walk, no GC ticking while bytes stream in.",
                compare_table(
                    { "", "agentty (C++26)", "claude-code (Node)" },
                    { { "Cold-start --help", "~2 ms", "~150 ms" },
                      { "--version",         "~2 ms", "~60 ms" },
                      { "Binary on disk",    "13 MB", "222 MB + Node" },
                      { "Install",           "curl | sh", "npm i -g + Node" },
                      { "GC pauses mid-stream", "None", "V8 GC" } })),

            // ── FEATURES ─────────────────────────────────────────────────────
            site_features("Features", "Everything you'd expect, nothing you wouldn't.", "",
                { site_feature("Sandboxed by default",
                      "Every tool call runs jailed — file writes and commands are "
                      "confined to the project unless you opt out.", "check"),
                  site_feature("Any model",
                      "Claude Pro/Max out of the box, or point it at OpenAI, Groq, "
                      "OpenRouter, Together, Cerebras, or a local Ollama.", "check"),
                  site_feature("SSH air-gap",
                      "One command runs the agent on a remote box with your local "
                      "keys — nothing leaves the tunnel.", "check"),
                  site_feature("Runs inside Zed",
                      "Speaks ACP, so it drops into Zed's agent panel as a native "
                      "pair-programmer.", "check"),
                  site_feature("No runtime",
                      "No Node, no Python, no Electron. One binary you curl and chmod "
                      "— it starts before an interpreter would finish booting.", "check"),
                  site_feature("MIT licensed",
                      "Free and open source. Fork it, ship it, build on it.", "check") }),

            // ── PROVIDERS (another comparison) ───────────────────────────────
            site_section("Providers", "Bring your own model.",
                "One flag switches the backend — same interface, any endpoint.",
                compare_table(
                    { "flag", "what it does" },
                    { { "--provider anthropic", "Claude Pro/Max (default)" },
                      { "--provider openai",    "GPT models via the OpenAI API" },
                      { "--provider groq",      "Fast open models on Groq" },
                      { "--provider cerebras",  "Wafer-scale inference — very fast" },
                      { "--provider ollama",    "Local models — no key, no cloud" },
                      { "--provider host:port", "Any OpenAI-compatible endpoint" } })),

            // ── CTA / INSTALL ────────────────────────────────────────────────
            cta_band("Ready to try it?",
                "One line. No Node, no Python, no Electron.",
                copy_line(install, Copy{ install }),
                cta_row(
                    cta_primary("Read the docs", "/docs"),
                    cta_ghost("Releases", "https://github.com/1ay1/agentty/releases"))),

            // ── FOOTER ───────────────────────────────────────────────────────
            site_footer("agentty",
                "A blazing-fast coding agent in your terminal · MIT licensed.",
                { {"GitHub", "https://github.com/1ay1/agentty"},
                  {"Discord", "https://discord.gg/qhb9AZ8f3c"},
                  {"Docs", "/docs"},
                  {"Blog", "/blog"} }));
    }

    static Meta meta(const Model&) {
        return { .title = "agentty — a blazing-fast coding agent in your terminal",
                 .description = "A native C++26 terminal coding agent: one static binary, "
                                "millisecond cold start, sandboxed by default, any model.",
                 .type = "website" };
    }
};

int main() { return live<App>({ .port = 8080, .page_bg = 0x0d1117, .title = "agentty" }); }
