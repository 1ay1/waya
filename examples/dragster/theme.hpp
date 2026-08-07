/// dragster/theme.hpp — the palette of the original Activision Dragster (1980)
/// on the Atari 2600: a purple sky band, mint-green ground, a black blocky car,
/// and the signature green tachometer with a red redline. Cleaned up (no CRT
/// noise) but faithful to those bold TIA colours. Blocky mono type.
#pragma once

#include <waya/surface/node.hpp>
#include <cstdint>

namespace dr {

using namespace waya::surface;

// ── palette (Dragster / Atari 2600 TIA), 0xRRGGBB ───────────────────────────
inline constexpr std::uint32_t page    = 0x000000;  // black letterbox
inline constexpr std::uint32_t sky     = 0x6a4fc4;  // purple sky band
inline constexpr std::uint32_t skyLo   = 0x5a3fb0;
inline constexpr std::uint32_t ground  = 0x5fe0a0;  // mint-green ground
inline constexpr std::uint32_t groundLo= 0x46c78a;
inline constexpr std::uint32_t car     = 0x101014;  // the black dragster
inline constexpr std::uint32_t carSoft = 0x2a2a30;  // car detail / shade
inline constexpr std::uint32_t band    = 0x000000;  // black separator band
inline constexpr std::uint32_t tick     = 0x9ad0ff; // pale-blue distance ticks
inline constexpr std::uint32_t tachOk   = 0x46c93a; // green tach fill
inline constexpr std::uint32_t tachRed  = 0xe03024; // redline zone
inline constexpr std::uint32_t digit    = 0x101014; // big score digits (black)
inline constexpr std::uint32_t good     = 0x2f9e2f; // go / finish
inline constexpr std::uint32_t warn     = 0xe03024; // danger
inline constexpr std::uint32_t amber     = 0xf0a81a; // caution
inline constexpr std::uint32_t ink       = 0x101014;
inline constexpr std::uint32_t inkSoft   = 0x2a2a30;

// Fonts. `term` = a clean geometric UI sans for labels/values (crisp, legible);
// `mono` = a tabular monospace reserved for the big timer digits so they don't
// jitter as they count.
inline const Mod term = font_family(
    "ui-sans-serif,system-ui,'Segoe UI',Inter,Roboto,'Helvetica Neue',Arial,sans-serif");
inline const Mod mono_font = font_family(
    "ui-monospace,'SF Mono','JetBrains Mono','Roboto Mono',Menlo,Consolas,monospace");

} // namespace dr
