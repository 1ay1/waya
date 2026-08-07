/// dragster/theme.hpp — the look of a 1980 Atari 2600 game on a CRT: a chunky
/// plastic console frame around a scanlined TV screen, bold flat "TIA" colors
/// (the 2600's signature blue sky, green track, primary-red car), and a blocky
/// score font. Warm, low-res, unmistakably Activision-era.
#pragma once

#include <waya/surface/sugar.hpp>
#include <cstdint>
#include <string>

namespace dr {

using namespace waya::surface;

// ── palette (Atari 2600 TIA-ish) ─────────────────────────────────────────
inline constexpr std::uint32_t page    = 0x0a0a0f;  // dark room behind the TV
inline constexpr std::uint32_t woodHi  = 0x8a5a2b;  // faux-woodgrain console top
inline constexpr std::uint32_t woodLo  = 0x4d2f14;  // woodgrain shadow
inline constexpr std::uint32_t bezel   = 0x1a1a1a;  // black TV bezel
inline constexpr std::uint32_t sky     = 0x3b6ea5;  // 2600 blue sky
inline constexpr std::uint32_t skyLo   = 0x25507f;
inline constexpr std::uint32_t track   = 0x4a7a2a;  // green drag strip
inline constexpr std::uint32_t trackLo = 0x2f5019;
inline constexpr std::uint32_t lane    = 0xcfcfcf;  // lane / distance stripes
inline constexpr std::uint32_t car     = 0xd83a2a;  // primary-red dragster
inline constexpr std::uint32_t carHi   = 0xff6a4d;
inline constexpr std::uint32_t chrome  = 0xe8e4d8;  // bodywork highlight
inline constexpr std::uint32_t amber   = 0xf4b41a;  // score / labels
inline constexpr std::uint32_t hot     = 0xff3b1a;  // redline / danger
inline constexpr std::uint32_t good    = 0x46c93a;  // green light / go
inline constexpr std::uint32_t ink     = 0xf3e7d8;  // console text
inline constexpr std::uint32_t muted   = 0xb2a48f;

inline std::string hx(std::uint32_t c){ return detail::hexstr(c); }
inline std::string hxa(std::uint32_t c, const char* aa){ return hx(c) + aa; }

// blocky, retro-console mono
inline const Mod term = detail::raw_css("font-family",
    "'Courier New',ui-monospace,'JetBrains Mono',Consolas,monospace");

inline Mod dglow(std::uint32_t c, int px = 8){
    return detail::raw_css("box-shadow", "0 0 " + std::to_string(px) + "px " + hxa(c, "cc")
                           + ", 0 0 " + std::to_string(px*2) + "px " + hxa(c, "55"));
}
inline Mod text_glow(std::uint32_t c, int px = 6){
    return detail::raw_css("text-shadow", "0 0 " + std::to_string(px) + "px " + hxa(c, "aa"));
}

// CRT scanlines overlaid on the screen for that TV feel.
inline Mod scanlines(){
    return detail::raw_css("background-image",
        "repeating-linear-gradient(0deg, rgba(0,0,0,.16) 0 1px, transparent 1px 3px)");
}

} // namespace dr
