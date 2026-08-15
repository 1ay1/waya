#pragma once
// examples/agentty/components/logo.hpp
//
// AgenttyLogo — the ">AGENTTY" welcome-screen wordmark in a 6×7 pixel-art font.
// Faithful port of components/AgenttyLogo.tsx + agentty-logo.css.
//
// Three-layer CSS animation, exactly as the source (all composited — the main
// thread stays idle, no rAF, no per-frame state):
//   1. drop cascade  — each glyph falls into place, 180 ms stagger, runs ONCE
//   2. sine bob      — each glyph bobs forever, phase-shifted L→R (a slow wave)
//   3. heartbeat     — the whole mark flashes magenta→white every 5.76 s
//
// Brand policy lives in the app, not the framework: this is an agentty
// COMPONENT, built on waya's core vocabulary (grid of cells) + the asset
// registry (keyframes). The framework ships the mechanism; agentty ships this.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <array>
#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

// sigil colour — bright magenta (maya role_brand)
inline constexpr std::uint32_t logo_magenta = 0xe29be0;

/// `agentty::logo(cell)` — the animated pixel wordmark. `cell` is the px size of
/// one pixel-art cell (default 11, matching the hero).
inline NodeRef logo(int cell = 11) {
    // 6-wide × 7-tall glyphs; '#' = a lit pixel. Transcribed from the source.
    static const std::vector<std::pair<char, std::array<const char*, 7>>> GLYPHS = {
        {'>', {"      ","#  #  ","## ## "," ## ##","## ## ","#  #  ","      "}},
        {'A', {"  ##  "," #  # ","#    #","######","#    #","#    #","#    #"}},
        {'G', {" #### ","#    #","#     ","#  ###","#    #","#    #"," #### "}},
        {'E', {"######","#     ","#     ","##### ","#     ","#     ","######"}},
        {'N', {"#    #","##   #","# #  #","#  # #","#   ##","#    #","#    #"}},
        {'T', {"######","  ##  ","  ##  ","  ##  ","  ##  ","  ##  ","  ##  "}},
        {'Y', {"#    #","#    #"," #  # ","  ##  ","  ##  ","  ##  ","  ##  "}},
    };

    // Register the three keyframe layers once (deduped by name in the registry).
    assets().keyframes("agentty-logo-drop",
        "from{transform:translateY(-120%);opacity:0}to{transform:translateY(0);opacity:1}");
    assets().keyframes("agentty-logo-bob",
        "0%,100%{transform:translateY(0)}50%{transform:translateY(-2px)}");
    assets().keyframes("agentty-logo-pulse",
        "0%,94%,100%{color:#e29be0;filter:drop-shadow(0 0 22px rgba(226,155,224,.35))}"
        "96%,97%{color:#ffffff;filter:drop-shadow(0 0 26px rgba(255,255,255,.5))}");

    const std::string txt = ">AGENTTY";
    std::vector<NodeRef> letters;
    int li = 0;
    for (char ch : txt) {
        const std::array<const char*, 7>* g = nullptr;
        for (auto& [k, bmp] : GLYPHS) if (k == ch) { g = &bmp; break; }
        if (!g) continue;
        std::vector<NodeRef> cells;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 6; ++x) {
                bool lit = (*g)[(std::size_t)y][(std::size_t)x] == '#';
                auto c = box() | w((float)cell) | h((float)cell);
                if (lit) c = c | detail::raw_css("background", "currentColor") | round(2);
                cells.push_back(std::move(c));
            }
        auto grid = box(); grid->kids = std::move(cells); grid->style.flow = Flow::grid; finalize(*grid);
        // per-letter: drop (once, 180ms stagger) + bob (4s, phase-shifted L→R).
        // bob-delay = li*180 + 900 - li*446 keeps the wave from fighting the drop.
        grid = grid | detail::raw_css("grid-template-columns", "repeat(6," + std::to_string(cell) + "px)")
            | detail::raw_css("gap", "1px")
            | detail::raw_css("animation",
                "agentty-logo-drop .9s cubic-bezier(.215,.61,.355,1) both " + std::to_string(li * 180) + "ms,"
                "agentty-logo-bob 4s ease-in-out infinite " + std::to_string(li * 180 + 900 - li * 446) + "ms");
        letters.push_back(std::move(grid));
        ++li;
    }
    auto mark = box(); mark->kids = std::move(letters); mark->style.flow = Flow::row; finalize(*mark);
    return mark | items_start | detail::raw_css("gap", std::to_string(cell) + "px")
        | fg(logo_magenta)
        // heartbeat starts after the cascade settles (7*180 + 900 = 2160 ms)
        | detail::raw_css("animation", "agentty-logo-pulse 5.76s ease-in-out infinite 2160ms")
        | detail::raw_css("margin-bottom", "28px");
}

} // namespace agentty
