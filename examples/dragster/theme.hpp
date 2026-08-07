/// dragster/theme.hpp — the look of a handheld LCD game (Game & Watch / Mattel
/// era): an olive-green LCD panel, near-black "lit" segments, and faint "ghost"
/// segments where a pixel could light but isn't. Blocky mono type. Just the
/// palette + the couple of effects the framework has no Mod for.
#pragma once

#include <waya/surface/node.hpp>
#include <cstdint>

namespace dr {

using namespace waya::surface;

// ── LCD palette, 0xRRGGBB ───────────────────────────────────────────────────
inline constexpr std::uint32_t page    = 0x1c1e12;  // dark bezel around the panel
inline constexpr std::uint32_t lcd     = 0x9ba86b;  // the classic olive-green LCD
inline constexpr std::uint32_t lcdLo   = 0x8a9a5c;  // panel gradient shadow
inline constexpr std::uint32_t seg     = 0x1a1c10;  // a LIT segment (near-black)
inline constexpr std::uint32_t ghost   = 0x8ea05f;  // an UNLIT "ghost" segment (faint)
inline constexpr std::uint32_t ink     = 0x23260f;  // panel text / labels
inline constexpr std::uint32_t inkSoft = 0x5c6534;  // muted panel text
inline constexpr std::uint32_t hot     = 0x2a1608;  // redline segment (warmer black)
inline constexpr std::uint32_t frame   = 0x2a2c1a;  // inner frame line

// accents (used sparingly — an LCD game's printed overlay colours)
inline constexpr std::uint32_t good    = 0x2f7d2f;  // go / finish (deep green)
inline constexpr std::uint32_t warn    = 0x9a3312;  // danger / redline (burnt red)
inline constexpr std::uint32_t amber   = 0x8a6a12;  // caution (dark amber)

// blocky LCD digit font (font_family is the real Mod).
inline const Mod term = font_family(
    "'Courier New',ui-monospace,'JetBrains Mono',Consolas,monospace");

// A subtle glass sheen over the LCD (linear-gradient sheen: no single Mod).
inline Mod sheen(){
    return detail::raw_css("background-image",
        "linear-gradient(135deg, rgba(255,255,255,.10) 0%, transparent 30%, "
        "transparent 70%, rgba(0,0,0,.08) 100%)");
}

} // namespace dr
