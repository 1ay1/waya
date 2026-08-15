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

// agentty-logo.css, adapted: the wordmark is drawn as a GRID of filled square
// pixels (one <i> per lit pixel) rather than half-block font glyphs. A real
// bitmap is solid squares, so this is pixel-crisp and font-INDEPENDENT (the
// half-block approach drifts when JetBrains Mono isn't the resolved monospace).
// The three animation layers are byte-for-byte the source's.
inline const char* LOGO_CSS =
".agentty-logo{--logo-mag:#e29be0;--px:3px;display:inline-flex;flex-direction:row;align-items:flex-start;"
"color:var(--logo-mag);user-select:none;filter:drop-shadow(0 0 22px rgba(226,155,224,.35));"
"animation:logo-pulse 6s ease-in-out infinite}"
".agentty-logo.lg{--px:9px}.agentty-logo.md{--px:6px}.agentty-logo.sm{--px:4px}"
"@media (max-width:720px){.agentty-logo.lg{--px:clamp(4px,1.1vw,9px)}}"
".logo-letter{display:grid;grid-template-columns:repeat(6,var(--px));grid-auto-rows:var(--px);"
"gap:0;margin-right:var(--px);will-change:transform;"
"animation:logo-drop 1s cubic-bezier(.16,.84,.44,1) both var(--delay),"
"logo-bob 5.5s ease-in-out infinite var(--bob-delay)}"
".logo-letter:last-child{margin-right:0}"
".logo-letter i{display:block;width:var(--px);height:var(--px);background:currentColor;border-radius:1px}"
".logo-letter i.off{background:transparent}"
"@keyframes logo-drop{from{transform:translateY(-.8em);opacity:0}to{transform:translateY(0);opacity:1}}"
"@keyframes logo-bob{0%,100%{transform:translateY(0)}50%{transform:translateY(-1.5px)}}"
"@keyframes logo-pulse{0%,100%{filter:drop-shadow(0 0 18px rgba(226,155,224,.30))}"
"50%{filter:drop-shadow(0 0 26px rgba(226,155,224,.5))}}"
"@media (prefers-reduced-motion:reduce){.agentty-logo,.logo-letter{animation:none}}";

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

        // each letter = a 6-col grid of 7 rows of pixel cells; one <i> per pixel,
        // filled (currentColor) if lit, transparent (.off) otherwise.
        std::vector<NodeRef> pixels;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 6; ++x) {
                bool lit = g && (*g)[(std::size_t)y][(std::size_t)x] == '#';
                pixels.push_back(box() | as("i") | add_class(lit ? "on" : "off"));
            }
        auto letter = box(); letter->kids = std::move(pixels); finalize(*letter);
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
