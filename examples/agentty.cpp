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

#include <array>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;

// The agentty pixel wordmark: ">AGENTTY" in a 6x7 bitmap font, rendered as a
// grid of tiny lit/unlit cells with a magenta gradient — the site's welcome
// mark. Brand policy, so it lives in the app. Bitmaps transcribed from the
// author's own AgenttyLogo.
static NodeRef pixel_logo() {
    // 6-wide x 7-tall glyphs; '#' = a lit pixel.
    static const std::vector<std::pair<char, std::array<const char*, 7>>> GLYPHS = {
        {'>', {"      ","#  #  ","## ## "," ## ##","## ## ","#  #  ","      "}},
        {'A', {"  ##  "," #  # ","#    #","######","#    #","#    #","#    #"}},
        {'G', {" #### ","#    #","#     ","#  ###","#    #","#    #"," #### "}},
        {'E', {"######","#     ","#     ","##### ","#     ","#     ","######"}},
        {'N', {"#    #","##   #","# #  #","#  # #","#   ##","#    #","#    #"}},
        {'T', {"######","  ##  ","  ##  ","  ##  ","  ##  ","  ##  ","  ##  "}},
        {'Y', {"#    #","#    #"," #  # ","  ##  ","  ##  ","  ##  ","  ##  "}},
    };
    const std::string text = ">AGENTTY";
    const int cell = 11;      // px per pixel-cell
    // Build the letters as a row of pixel grids.
    std::vector<NodeRef> letters;
    for (char ch : text) {
        const std::array<const char*, 7>* g = nullptr;
        for (auto& [k, bmp] : GLYPHS) if (k == ch) { g = &bmp; break; }
        if (!g) continue;
        std::vector<NodeRef> cells;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 6; ++x) {
                bool lit = (*g)[(std::size_t)y][(std::size_t)x] == '#';
                cells.push_back(box() | w((float)cell) | h((float)cell)
                    | (lit ? bg_transparent() : bg_transparent()));  // color via parent gradient
                if (lit) cells.back() = cells.back() | detail::raw_css("background","currentColor") | round(2);
            }
        auto grid = box(); grid->kids = std::move(cells); grid->style.flow = Flow::grid; finalize(*grid);
        grid = grid | detail::raw_css("grid-template-columns", "repeat(6," + std::to_string(cell) + "px)")
            | detail::raw_css("gap","1px");
        letters.push_back(std::move(grid));
    }
    auto mark = box(); mark->kids = std::move(letters); mark->style.flow = Flow::row; finalize(*mark);
    // magenta gradient tint + a soft glow, applied via currentColor + text-shadow.
    return mark | items_start | detail::raw_css("gap", std::to_string(cell) + "px")
        | fg(0xd97cd9)
        | detail::raw_css("filter","drop-shadow(0 0 24px rgba(217,124,217,.45))")
        | detail::raw_css("margin-bottom","28px");
}

// Pure-CSS "matrix rain": columns of glyphs, each translated down forever by a
// CSS keyframe. The philosophy (same as ripple/tap_pop): decorative motion is
// CLIENT-side — the browser paints it, the server renders the markup ONCE at
// first paint and never again. Zero Model state, zero WebSocket traffic, zero
// server cost per frame. A page under load pays nothing for this animation.
static NodeRef matrix_rain(std::uint32_t accent) {
    // register the fall keyframes ONCE in the shared stylesheet (client-cached).
    assets().css(
        "@keyframes agentty-fall{to{transform:translateY(50%)}}"
        ".agentty-col{will-change:transform;animation:agentty-fall linear infinite}");
    static const char* GLYPHS = "01<>{}[]#$/\\|=+*ABCDEF89";
    const int cols = 26, rows = 42, n = (int)std::string(GLYPHS).size();
    auto hash = [](std::uint32_t x){ x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; };
    std::vector<NodeRef> columns;
    for (int c = 0; c < cols; ++c) {
        // each column is a tall strip (2x the viewport) of glyphs; translating it
        // down 50% loops seamlessly because the strip repeats its own top half.
        std::vector<NodeRef> cells;
        for (int r = 0; r < rows; ++r) {
            std::uint32_t h = hash((std::uint32_t)(c * 131 + r * 977));
            char ch = GLYPHS[h % (std::uint32_t)n];
            bool head = (h % 11) == 0;
            float op = head ? 0.9f : (0.06f + (float)(h % 40) / 130.0f);
            cells.push_back(text(std::string(1, ch)) | mono | text_size(13)
                | fg(head ? 0xbfe0ff : accent)
                | detail::raw_css("opacity", detail::numstr(op))
                | detail::raw_css("line-height", "1.25"));
        }
        auto colBox = box(); colBox->kids = std::move(cells); colBox->style.flow = Flow::col; finalize(*colBox);
        // per-column speed + start offset so they don't fall in lockstep.
        float dur = 6.0f + (float)(hash((std::uint32_t)c) % 90) / 10.0f;   // 6..15s
        float delay = -(float)(hash((std::uint32_t)c * 7u) % 100) / 10.0f; // negative = mid-cycle start
        columns.push_back(colBox | items_center | attr("class", "agentty-col")
            | detail::raw_css("animation-duration", detail::numstr(dur) + "s")
            | detail::raw_css("animation-delay", detail::numstr(delay) + "s"));
    }
    auto grid = box(); grid->kids = std::move(columns); grid->style.flow = Flow::row; finalize(*grid);
    return grid | detail::raw_css("gap", "14px") | justify_center
        | absolute() | detail::raw_css("inset", "-50% 0 0 0") | z(0) | no_pointer
        | detail::raw_css("overflow", "hidden") | detail::raw_css("opacity", "0.5")
        | detail::raw_css("mask-image", "linear-gradient(180deg,transparent,#000 20%,#000 60%,transparent 100%)")
        | detail::raw_css("-webkit-mask-image", "linear-gradient(180deg,transparent,#000 20%,#000 60%,transparent 100%)");
}

// The agentty-specific hero backdrop — a blueprint grid + falling digital-rain
// streaks + a drifting brand glow + CRT scanlines. This is the APP's brand
// aesthetic (policy), NOT the framework's — so it lives here in the example and
// is passed to site_hero_split_bg(), keeping ui/site.hpp unopinionated.
static NodeRef agentty_backdrop(std::uint32_t accent) {
    assets().keyframes("agentty-glow",
        "from{transform:translate(0,0) scale(1);opacity:.75}"
        "to{transform:translate(140px,70px) scale(1.18);opacity:1}");

    auto grid = box() | absolute() | detail::raw_css("inset", "-2px")
        | detail::raw_css("background-image",
            "linear-gradient(" + detail::rgba_hex(accent, 0.05f) + " 1px, transparent 1px),"
            "linear-gradient(90deg," + detail::rgba_hex(accent, 0.05f) + " 1px, transparent 1px)")
        | detail::raw_css("background-size", "44px 44px")
        | detail::raw_css("mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)")
        | detail::raw_css("-webkit-mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)");
    auto rain = matrix_rain(accent);   // pure-CSS falling code — client-painted, zero server cost
    auto glow = box() | absolute()
        | detail::raw_css("width", "760px") | detail::raw_css("height", "760px")
        | detail::raw_css("top", "-220px") | detail::raw_css("left", "20%")
        | detail::raw_css("background", "radial-gradient(circle," + detail::rgba_hex(accent, 0.20f) + ", transparent 60%)")
        | detail::raw_css("filter", "blur(10px)")
        | detail::raw_css("animation", "agentty-glow 12s ease-in-out infinite alternate");
    auto scan = box() | absolute() | detail::raw_css("inset", "0")
        | detail::raw_css("background",
            "repeating-linear-gradient(180deg, rgba(255,255,255,.02) 0px, rgba(255,255,255,.02) 1px, transparent 1px, transparent 3px)")
        | detail::raw_css("mix-blend-mode", "overlay");
    return box(grid, rain, glow, scan)
        | absolute() | detail::raw_css("inset", "0") | z(0)
        | detail::raw_css("overflow", "hidden") | no_pointer
        | detail::raw_css("mask-image", "linear-gradient(180deg,#000 0%,#000 62%,transparent 100%)")
        | detail::raw_css("-webkit-mask-image", "linear-gradient(180deg,#000 0%,#000 62%,transparent 100%)");
}

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

    // No subscribe() — the page has no live app state. The matrix rain and glow
    // are PURE CSS (client-painted), so nothing ticks the server. A marketing
    // page should cost the server one render, then stay silent.

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

            // ── HERO ──────────────────────────────────────────────────
            //   app supplies its OWN logo + backdrop (the framework stays neutral).
            site_hero_lead(
                pixel_logo(),
                agentty_backdrop(0x58a6ff),
                "Blazing-fast", "coding agent", "in your terminal.",
                "A drop-in alternative to claude-code, written in C++26. 13 MB binary, "
                "millisecond cold start, sandboxed by default, SSH air-gap in one command, "
                "and runs inside Zed over ACP. Signs in with your existing Claude Pro/Max "
                "\xe2\x80\x94 or point it at OpenAI, Groq, OpenRouter, Cerebras, or a local Ollama.",
                // the demo box on the right — a faithful replica agentty session
                tui_window("agentty \xe2\x80\x94 ~/projects/app",
                    // ACTIONS panel header
                    tui_line({ {0xd97cd9, "\xe2\x94\x8e "}, {0xaab1bd, "A C T I O N S"}, {0x656d76, "  \xc2\xb7  4/4"}, {0x656d76, "                          4.2s \xe2\x94\x92"} }),
                    tui_line({ {0xd97cd9, "\xe2\x94\x8e "}, {0x56b6c2, "I N S P E C T"}, {0xaab1bd, " 2"}, {0x656d76, "  \xc2\xb7  "}, {0xc586c0, "M U T A T E"}, {0xaab1bd, " 1"}, {0x656d76, "  \xc2\xb7  "}, {0x56b6c2, "E X E C U T E"}, {0xaab1bd, " 1"} }),
                    tui_line({ {0x656d76, ""} }),
                    // tool rows: tree branch + status + name + detail + timing
                    tui_line({ {0x656d76, " \xe2\x94\x9c\xe2\x94\x80 "}, {0x7ee787, "\xe2\x9c\x93 "}, {0x56d4e0, "Read"}, {0x656d76, "   src/auth/handler.cpp"}, {0xe5c07b, "  \xc2\xb7 214 lines"}, {0x656d76, "       142ms"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x9c\xe2\x94\x80 "}, {0x7ee787, "\xe2\x9c\x93 "}, {0x56d4e0, "Grep"}, {0x656d76, "   TokenCache"}, {0xe5c07b, "  \xc2\xb7 3 matches"}, {0x656d76, "            89ms"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x9c\xe2\x94\x80 "}, {0x7ee787, "\xe2\x9c\x93 "}, {0xc586c0, "Edit"}, {0x656d76, "   src/auth/handler.cpp"}, {0xe5c07b, "  (+18 \xe2\x88\x92 9)"}, {0x656d76, "          6ms"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82   "}, {0x656d76, "@@ resolve(id) @@"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82   "}, {0xe06c75, "- return fetch_remote(id);"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82   "}, {0x98c379, "+ if (auto v = cache.lookup(id)) return *v;"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x82"} }),
                    tui_line({ {0x656d76, " \xe2\x94\x94\xe2\x94\x80 "}, {0x7ee787, "\xe2\x9c\x93 "}, {0x56b6c2, "Bash"}, {0x656d76, "   cmake --build build -j"}, {0x656d76, "                3.6s"} }),
                    tui_line({ {0x656d76, "     "}, {0x656d76, "[100%] Built target agentty"} }),
                    tui_line({ {0x656d76, ""} }),
                    tui_line({ {0x7ee787, " \xe2\x9c\x93 D O N E"}, {0x656d76, "   4 actions"}, {0x656d76, "   4.2s"} }),
                    tui_line({ {0x656d76, ""} }),
                    // assistant prose
                    tui_line({ {0xe6edf3, "Auth handler now resolves through "}, {0x56d4e0, "TokenCache::lookup"}, {0xe6edf3, ", falling"} }),
                    tui_line({ {0xe6edf3, "back to a network refresh only on a miss. Build is green."} }),
                    tui_line({ {0x656d76, ""} }),
                    // input line + status bar
                    tui_line({ {0xd97cd9, " \xe2\x9d\xaf "}, {0x656d76, "type a message\xe2\x80\xa6"} }),
                    tui_line({ {0x656d76, ""} }),
                    tui_line({ {0xd97cd9, " \xe2\x96\x8e "}, {0xaab1bd, "refactor auth"}, {0x656d76, "   \xc2\xb7   "}, {0x7ee787, "\xe2\x97\x8f Ready"}, {0xe5c07b, " \xe2\x9a\xa1"}, {0x656d76, "   0.0 t/s  \xe2\x96\x81\xe2\x96\x82\xe2\x96\x83\xe2\x96\x82\xe2\x96\x85\xe2\x96\x83\xe2\x96\x82\xe2\x96\x81"} })),
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

            // ── SPEED ────────────────────────────────────────────────────────
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

            // ── WHY AGENTTY (features) ───────────────────────────────────────
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

            // ── BRING YOUR OWN MODEL (providers) ─────────────────────────────
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

            // ── HOW IT COMPARES ──────────────────────────────────────────────
            site_section("How it compares", "The single-binary pick.", "",
                compare_table(
                    { "", "agentty", "claude-code", "aider" },
                    { { "Language / runtime", "C++26 static", "TS / Node", "Python" },
                      { "Footprint",          "13 MB", "npm + Node", "pip + Python" },
                      { "Platforms",          "Linux/mac/Win", "Linux/mac/Win", "Linux/mac/Win" },
                      { "Sandboxed default",  "Yes", "No", "No" },
                      { "Runs in Zed (ACP)",  "Yes", "No", "No" } })),

            // ── TOOLS ────────────────────────────────────────────────────────
            site_features("Tools", "A purpose-built widget for everything.", "",
                { site_feature("Diffs", "File edits render as a real diff, not a wall of text.", "check"),
                  site_feature("Todos", "Multi-step plans get a live checklist you watch tick off.", "check"),
                  site_feature("Shell", "Commands run in a sandbox with their exit codes surfaced.", "check"),
                  site_feature("Search", "Grep and file finds return structured, jump-to results.", "check"),
                  site_feature("Images", "Drop a PNG/JPEG/GIF/WebP path straight into the thread.", "check"),
                  site_feature("Skills", "Agent Skills teach it your conventions from a folder.", "check") }),

            // ── OPEN SOURCE ──────────────────────────────────────────────────
            site_section("Open source", "Built in the open, MIT licensed.",
                "Fork it, ship it, build on it. Star the repo and join the Discord \xe2\x80\x94 "
                "there's an AI helper bot that answers agentty questions with the real agent.",
                cta_row(
                    cta_primary("Star on GitHub", "https://github.com/1ay1/agentty"),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c"))),

            // ── CTA / INSTALL ────────────────────────────────────────────────
            cta_band("Ready to try it?",
                "One line. No Node, no Python, no Electron.",
                copy_line(install, Copy{ install }),
                cta_row(
                    cta_primary("Quick start guide", "/docs/quick-start"),
                    cta_ghost("Join the Discord", "https://discord.gg/qhb9AZ8f3c"),
                    cta_ghost("Star on GitHub", "https://github.com/1ay1/agentty"))),

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
