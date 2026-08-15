// examples/agentty/main.cpp — the agentty.org homepage, ported to waya
// component-by-component.
//
// This is the composition root. It wires together:
//   • the framework's general site toolkit  (ui/site.hpp)  — nav, hero, sections,
//     features, compare tables, CTA band, footer;
//   • the framework's client-owned motion primitives (surface/reveal.hpp) —
//     scroll_progress, theme_toggle, and the reveal/typewriter/count_up that the
//     toolkit builders now use internally;
//   • agentty's OWN components (examples/agentty/components/*) — the pixel logo,
//     the canvas hero backdrop, and the TUI session replica. Brand policy lives
//     in the app, not the framework.
//
// The whole page is server-rendered ONCE (crawlable on first paint) and then
// silent: every animation is CSS or a client rAF loop, nothing ticks the server.
//
//   waya run agentty          (or: WAYA_PORT=8080 ./build/agentty)

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "components/logo.hpp"
#include "components/hero_background.hpp"
#include "components/tui.hpp"

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

    // No subscribe(): the page has no live app state. The rain, glow, reveals,
    // typewriters and count-ups are all client-owned — the server renders once.

    static NodeRef view(const Model&) {
        SiteTheme theme{};   // GitHub-dark, the toolkit default

        const std::string install = "curl -fsSL https://agentty.org/install.sh | sh";

        return site_page(theme,
            // a scroll-progress bar pinned to the very top of the viewport
            scroll_progress(theme.accent, theme.accent2),

            // ── NAV ──────────────────────────────────────────────────────────
            site_nav("agentty", "v0.2.4",
                { {"Docs", "/docs"}, {"Install", "/docs/installation"},
                  {"Blog", "/blog"}, {"Community", "/community"} },
                nav_cta("GitHub", "https://github.com/1ay1/agentty", false),
                theme_toggle(),                          // one-call light/dark switch
                nav_cta("Get started", "/docs/quick-start")),

            // ── HERO ─────────────────────────────────────────────────────────
            //   app supplies its OWN logo + backdrop; the framework stays neutral.
            site_hero_lead(
                agentty::logo(),
                agentty::hero_background(theme.accent),
                "Blazing-fast", "coding agent", "in your terminal.",
                "A drop-in alternative to claude-code, written in C++26. 13 MB binary, "
                "millisecond cold start, sandboxed by default, SSH air-gap in one command, "
                "and runs inside Zed over ACP. Signs in with your existing Claude Pro/Max "
                "\xe2\x80\x94 or point it at OpenAI, Groq, OpenRouter, Cerebras, or a local Ollama.",
                agentty::tui(),
                cta_row(
                    cta_primary("Quick start", "/docs/quick-start") | magnetic(),
                    cta_ghost("Star on GitHub", "https://github.com/1ay1/agentty") | magnetic(),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c") | magnetic())),

            // ── STATS ─────────────────────────────────────────────────────────
            stats_row(
                site_stat("13 MB", "static binary"),
                site_stat("2 ms", "cold start"),
                site_stat("0", "runtimes to install"),
                site_stat("6", "model providers")),

            // ── SPEED ─────────────────────────────────────────────────────────
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

            // ── WHY AGENTTY (features) ────────────────────────────────────────
            site_features("Why agentty",
                "Everything the official client does \xe2\x80\x94 and the things it doesn't.", "",
                { site_feature("Performance & footprint",
                      "C++26, statically linked, 13 MB. Millisecond cold start \xe2\x80\x94 it's "
                      "running before an interpreter would finish booting.", "check"),
                  site_feature("Models & auth",
                      "Claude by default via your Pro/Max subscription \xe2\x80\x94 or GPT, Groq, "
                      "OpenRouter, Together, Cerebras, and local Ollama. Switch live.", "check"),
                  site_feature("Safety & isolation",
                      "Every shell and build call runs jailed. Filesystem tools refuse "
                      "paths outside the launch directory unless you opt out.", "check"),
                  site_feature("Workflow & memory",
                      "Every conversation is a saved thread you reopen later; each turn "
                      "in a git repo pins a worktree snapshot you can restore.", "check"),
                  site_feature("Reach & extensibility",
                      "Runs inside Zed over ACP, serves its tools to any MCP client, and "
                      "one command air-gaps the agent onto a remote box over SSH.", "check"),
                  site_feature("Minimal by design",
                      "Lives at the bottom of your terminal, preserves scrollback, never "
                      "takes over the screen. Diffs and todos get purpose-built widgets.", "check") }),

            // ── BRING YOUR OWN MODEL (providers) ──────────────────────────────
            site_section("Bring your own model", "Claude by default. Any model on demand.",
                "One flag switches the backend \xe2\x80\x94 same interface, any endpoint.",
                compare_table(
                    { "flag", "what it does" },
                    { { "--provider anthropic", "Claude Pro/Max (default)" },
                      { "--provider openai",    "GPT models via the OpenAI API" },
                      { "--provider groq",      "Fast open models on Groq" },
                      { "--provider cerebras",  "Wafer-scale inference \xe2\x80\x94 very fast" },
                      { "--provider ollama",    "Local models \xe2\x80\x94 no key, no cloud" },
                      { "--provider host:port", "Any OpenAI-compatible endpoint" } })),

            // ── HOW IT COMPARES ───────────────────────────────────────────────
            site_section("How it compares", "The single-binary pick.", "",
                compare_table(
                    { "", "agentty", "claude-code", "aider" },
                    { { "Language / runtime", "C++26 static", "TS / Node", "Python" },
                      { "Footprint",          "13 MB", "npm + Node", "pip + Python" },
                      { "Platforms",          "Linux/mac/Win", "Linux/mac/Win", "Linux/mac/Win" },
                      { "Sandboxed default",  "Yes", "No", "No" },
                      { "Runs in Zed (ACP)",  "Yes", "No", "No" } })),

            // ── TOOLS ─────────────────────────────────────────────────────────
            site_features("Tools", "A purpose-built widget for everything.", "",
                { site_feature("Diffs", "File edits render as a real diff, not a wall of text.", "check"),
                  site_feature("Todos", "Multi-step plans get a live checklist you watch tick off.", "check"),
                  site_feature("Shell", "Commands run in a sandbox with their exit codes surfaced.", "check"),
                  site_feature("Search", "Grep and file finds return structured, jump-to results.", "check"),
                  site_feature("Images", "Drop a PNG/JPEG/GIF/WebP path straight into the thread.", "check"),
                  site_feature("Skills", "Agent Skills teach it your conventions from a folder.", "check") }),

            // ── OPEN SOURCE ───────────────────────────────────────────────────
            site_section("Open source", "Built in the open, MIT licensed.",
                "Fork it, ship it, build on it. Star the repo and join the Discord \xe2\x80\x94 "
                "there's an AI helper bot that answers agentty questions with the real agent.",
                cta_row(
                    cta_primary("Star on GitHub", "https://github.com/1ay1/agentty") | magnetic(),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c") | magnetic())),

            // ── CTA / INSTALL ─────────────────────────────────────────────────
            cta_band("Ready to try it?",
                "One line. No Node, no Python, no Electron.",
                copy_line(install, Copy{ install }),
                cta_row(
                    cta_primary("Quick start guide", "/docs/quick-start") | magnetic(),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c") | magnetic(),
                    cta_ghost("Star on GitHub", "https://github.com/1ay1/agentty") | magnetic())),

            // ── FOOTER ────────────────────────────────────────────────────────
            site_footer("agentty",
                "A blazing-fast coding agent in your terminal \xc2\xb7 MIT licensed.",
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
