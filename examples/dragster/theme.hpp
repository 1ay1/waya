/// dragster/theme.hpp — a modern, clean take on Dragster. Deep-slate UI with a
/// vivid neon cyan system (electric cyan primary, lime "go", amber caution,
/// hot-magenta danger) and rich gradient lanes — the 1980 layout, a 2020s look.
#pragma once

#include <waya/surface/node.hpp>
#include <cstdint>

namespace dr {

using namespace waya::surface;

// ── palette, 0xRRGGBB ───────────────────────────────────────────────────────
// backdrop / surfaces
inline constexpr std::uint32_t page    = 0x0a0b12;  // near-black slate backdrop
inline constexpr std::uint32_t panel   = 0x12141d;  // dashboard surface
inline constexpr std::uint32_t panelLo = 0x0c0d15;  // dashboard surface (deep)
inline constexpr std::uint32_t line_c  = 0x272b3a;  // hairlines / borders

// the track scene — a modern dusk-race look
inline constexpr std::uint32_t sky     = 0x3d2f8f;  // deep indigo dusk (top)
inline constexpr std::uint32_t skyLo   = 0x6d4bd6;  // violet horizon
inline constexpr std::uint32_t ground  = 0x14b8a6;  // teal-green track (top)
inline constexpr std::uint32_t groundLo= 0x0e7c72;  // teal-green track (deep)
inline constexpr std::uint32_t car     = 0x0b0c12;  // near-black dragster body
inline constexpr std::uint32_t carSoft = 0x232838;
inline constexpr std::uint32_t band    = 0x0a0b12;  // scene separator
inline constexpr std::uint32_t tick     = 0x8be9ff; // bright cyan distance ticks

// accents — vivid, modern, high-contrast
inline constexpr std::uint32_t cyan   = 0x38bdf8; // electric cyan (primary)
inline constexpr std::uint32_t good      = 0x4ade80; // lime green (go / finish)
inline constexpr std::uint32_t amber     = 0xfbbf24; // amber (caution / shift)
inline constexpr std::uint32_t warn      = 0xfb5a7d; // hot magenta-red (danger)
inline constexpr std::uint32_t tachOk    = 0x4ade80; // tach green
inline constexpr std::uint32_t tachRed   = 0xfb5a7d; // tach redline

// text
inline constexpr std::uint32_t ink       = 0xf4f6fb; // primary text (near-white)
inline constexpr std::uint32_t inkSoft   = 0x9aa3b8; // muted labels
inline constexpr std::uint32_t inkFaint  = 0x5a627a; // faintest captions

// Fonts. `term` = a clean geometric UI sans; `mono_font` = tabular monospace
// for the big timer digits so they don't jitter as they count.
inline const Mod term = font_family(
    "ui-sans-serif,system-ui,'Inter','Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif");
inline const Mod mono_font = font_family(
    "ui-monospace,'SF Mono','JetBrains Mono','Roboto Mono',Menlo,Consolas,monospace");

} // namespace dr
