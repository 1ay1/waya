#pragma once
// examples/agentty/components/logo.hpp
//
// AgenttyLogo — the ">AGENTTY" welcome-screen wordmark, rendered as half-block
// (▀ ▄ █) cells in a monospace grid. 1:1 port of components/AgenttyLogo.tsx +
// agentty-logo.css (which is itself a port of maya/widget/welcome_screen.hpp).
//
// Three CSS-driven animation layers, IDENTICAL to the source (all composited —
// no rAF, no per-frame state, main thread idle):
//   1. cascade drop  — each glyph falls in from -1em, 180ms stagger, once,
//                      logo-drop .9s ease-out cubic (var(--delay)).
//   2. sine bob      — each glyph bobs ±0.16em forever, logo-bob 4s, phase-
//                      shifted L→R by var(--bob-delay) = li*180+900-li*446.
//   3. heartbeat     — the whole mark flashes magenta→white ~150ms every
//                      5.76s, logo-pulse, starting after the cascade (2.16s).
//
// The exact agentty-logo.css is registered verbatim; the glyph grid and the
// per-letter CSS vars are built to match buildLetter()/the JSX exactly, so the
// rendered DOM + animation are the same as the React component.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <array>
#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

namespace logo_detail {

// agentty-logo.css, verbatim (with --font-mono fallbacks inlined). Registered once.
inline const char* LOGO_CSS =
".agentty-logo{--logo-mag:#e29be0;font-family:var(--font-mono,'JetBrains Mono',ui-monospace,monospace);"
"line-height:1;letter-spacing:0;display:inline-flex;flex-direction:row;align-items:flex-start;"
"color:var(--logo-mag);user-select:none;text-shadow:0 0 22px rgba(226,155,224,.35);"
"animation:logo-pulse 5.76s linear 2.16s infinite}"
".logo-letter{display:inline-flex;flex-direction:column;margin-right:1ch;will-change:transform;"
"animation:logo-drop .9s cubic-bezier(.215,.61,.355,1) both var(--delay),"
"logo-bob 4s ease-in-out infinite var(--bob-delay)}"
".logo-letter:last-child{margin-right:0}"
".logo-row{display:flex;white-space:pre;height:1em}"
".agentty-logo .px{display:inline-block;width:1ch;text-align:center;font-size:inherit;color:inherit}"
".agentty-logo .px.off{visibility:hidden}.agentty-logo .px.on{color:inherit}"
".agentty-logo.lg{font-size:22px}.agentty-logo.md{font-size:15px}.agentty-logo.sm{font-size:11px}"
"@media (max-width:720px){.agentty-logo.lg{font-size:clamp(9px,2.7vw,22px)}}"
"@keyframes logo-drop{from{transform:translateY(-1em);opacity:0}to{transform:translateY(0);opacity:1}}"
"@keyframes logo-bob{0%{transform:translateY(0)}25%{transform:translateY(-0.16em)}50%{transform:translateY(0)}"
"75%{transform:translateY(0.16em)}100%{transform:translateY(0)}}"
"@keyframes logo-pulse{0%,94%,100%{color:var(--logo-mag);text-shadow:0 0 22px rgba(226,155,224,.35)}"
"96%,97%{color:#fff;text-shadow:0 0 26px rgba(255,255,255,.5)}}"
"@media (prefers-reduced-motion:reduce){.agentty-logo,.logo-letter{animation:none}}";

// FONT_W=6, FONT_H=7, PAD_TOP=1, PAD_BOTTOM=2 → 10 pixel rows → 5 cell rows.
constexpr int FONT_W = 6, FONT_H = 7, PAD_TOP = 1, PAD_BOTTOM = 2;
constexpr int PH = FONT_H + PAD_TOP + PAD_BOTTOM;   // 10 pixel rows
constexpr int CH = (PH + 1) / 2;                    // 5 cell rows (2 px per cell)

// buildLetter(): 6×7 '#' bitmap → CH×FONT_W half-block cells.
// Each cell = top pixel + bottom pixel → '█'(both) '▀'(top) '▄'(bottom) ' '(none).
inline std::vector<std::vector<const char*>> build_letter(const std::array<const char*, 7>* g) {
    std::array<int, PH * FONT_W> lit{};
    if (g) for (int row = 0; row < FONT_H; ++row)
        for (int col = 0; col < FONT_W; ++col)
            if ((*g)[(std::size_t)row][(std::size_t)col] == '#')
                lit[(std::size_t)((PAD_TOP + row) * FONT_W + col)] = 1;
    std::vector<std::vector<const char*>> cells;
    for (int cy = 0; cy < CH; ++cy) {
        std::vector<const char*> line;
        for (int x = 0; x < FONT_W; ++x) {
            bool top = lit[(std::size_t)(cy * 2 * FONT_W + x)] == 1;
            bool bot = (cy * 2 + 1 < PH) && lit[(std::size_t)((cy * 2 + 1) * FONT_W + x)] == 1;
            line.push_back(top && bot ? "\xe2\x96\x88"     // █
                         : top        ? "\xe2\x96\x80"     // ▀
                         : bot        ? "\xe2\x96\x84"     // ▄
                                      : " ");
        }
        cells.push_back(std::move(line));
    }
    return cells;
}

} // namespace logo_detail

/// `agentty::logo(size)` — the animated half-block pixel wordmark.
/// `size` ∈ {"lg","md","sm"} maps to the source's 22/15/11px presets.
inline NodeRef logo(const char* size = "lg") {
    using namespace logo_detail;
    assets().css(LOGO_CSS);

    static const std::vector<std::pair<char, std::array<const char*, 7>>> GLYPHS = {
        {'>', {"      ","#  #  ","## ## "," ## ##","## ## ","#  #  ","      "}},
        {'A', {"  ##  "," #  # ","#    #","######","#    #","#    #","#    #"}},
        {'G', {" #### ","#    #","#     ","#  ###","#    #","#    #"," #### "}},
        {'E', {"######","#     ","#     ","##### ","#     ","#     ","######"}},
        {'N', {"#    #","##   #","# #  #","#  # #","#   ##","#    #","#    #"}},
        {'T', {"######","  ##  ","  ##  ","  ##  ","  ##  ","  ##  ","  ##  "}},
        {'Y', {"#    #","#    #"," #  # ","  ##  ","  ##  ","  ##  ","  ##  "}},
    };

    const std::string txt = ">AGENTTY";
    std::vector<NodeRef> letters;
    int li = 0;
    for (char ch : txt) {
        const std::array<const char*, 7>* g = nullptr;
        for (auto& [k, bmp] : GLYPHS) if (k == ch) { g = &bmp; break; }
        auto cells = build_letter(g);

        // each letter = a column of .logo-row's, each row a run of .px cells
        std::vector<NodeRef> rows;
        for (auto& line : cells) {
            std::vector<NodeRef> pxs;
            for (const char* c : line) {
                bool on = c[0] != ' ';
                // space cells render \u00a0 (nbsp) and are visibility:hidden via .off
                pxs.push_back(text(on ? std::string(c) : "\xc2\xa0")
                              | add_class(on ? "px on" : "px off"));
            }
            auto r = row(); r->kids = std::move(pxs); finalize(*r);
            rows.push_back(r | add_class("logo-row"));
        }
        auto letter = col(); letter->kids = std::move(rows); finalize(*letter);
        letter = letter | add_class("logo-letter");
        // exact source CSS vars: --delay = li*180ms, --bob-delay = li*180+900-li*446ms
        letter->style.extra.emplace_back("--delay", std::to_string(li * 180) + "ms");
        letter->style.extra.emplace_back("--bob-delay", std::to_string(li * 180 + 900 - li * 446) + "ms");
        finalize(*letter);
        letters.push_back(std::move(letter));
        ++li;
    }

    auto mark = row(); mark->kids = std::move(letters); finalize(*mark);
    mark = mark | add_class(std::string("agentty-logo ") + size);
    mark->attrs.emplace_back("role", "img");
    mark->attrs.emplace_back("aria-label", "agentty");
    mark->style.extra.emplace_back("margin-bottom", "28px");
    finalize(*mark);
    return mark;
}

} // namespace agentty
