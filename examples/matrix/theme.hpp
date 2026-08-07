/// matrix/theme.hpp — the Matrix palette + shared style helpers. Phosphor green
/// on black, monospace everything. Pure presentation tokens; no logic.
#pragma once

#include <waya/surface/sugar.hpp>
#include <cstdint>
#include <string>

namespace mtx {

using namespace waya::surface;

// ── palette ──────────────────────────────────────────────────────────────
inline constexpr std::uint32_t black   = 0x000000;
inline constexpr std::uint32_t panel   = 0x030d06;  // near-black green
inline constexpr std::uint32_t line    = 0x0d3a1f;  // dim green hairline
inline constexpr std::uint32_t dim      = 0x1c6b3a;  // dim phosphor
inline constexpr std::uint32_t green   = 0x00ff41;  // THE matrix green
inline constexpr std::uint32_t bright  = 0xb8ffcb;  // near-white head glyph
inline constexpr std::uint32_t amber   = 0xffb000;  // alert
inline constexpr std::uint32_t red     = 0xff2d4b;  // critical

inline std::string hx(std::uint32_t c){ return detail::hexstr(c); }
inline std::string hxa(std::uint32_t c, const char* aa){ return hx(c) + aa; }

// monospace, everywhere
inline const Mod term_font = detail::raw_css("font-family",
    "'Courier New','JetBrains Mono','Fira Code',ui-monospace,Consolas,monospace");

// phosphor text glow
inline Mod phosphor(std::uint32_t c = green, int px = 6) {
    return detail::raw_css("text-shadow",
        "0 0 " + std::to_string(px) + "px " + hxa(c, "cc"));
}

// a bordered terminal pane with an inner phosphor bloom + scanlines
inline Mod pane(std::uint32_t accent = green) {
    return detail::raw_css("box-shadow",
        "inset 0 0 30px " + hxa(accent, "12") + ", 0 0 20px -6px " + hxa(accent, "3a"));
}

} // namespace mtx
