#pragma once
/// \file color.hpp
/// A typed colour value \u2014 waya's answer to maya's `Fg<r,g,b>`. Colours are
/// values, not magic hex ints: constructed by name, `rgb`, `rgba`, or `hsl`,
/// composable, and comparable. The DOM backend still receives a plain 32-bit
/// RGBA under the hood, so nothing about the renderer changes; this is the
/// authoring-facing type that makes `fg(brand)` read better than `fg(0x6366f1)`
/// and lets alpha ride along without a second channel.
///
///   using namespace waya::color;
///   text("hi") | fg(indigo) | bg(rgba(0,0,0,0.4))
///   box() | bg(hsl(210, 0.9f, 0.5f))
///
/// A Color is a 32-bit 0xRRGGBBAA. `opaque()` drops alpha to the legacy 0xRRGGBB
/// path used by the existing `fg`/`bg` (which take a std::uint32_t), and `css()`
/// renders `#rrggbb` or `rgba(...)` when alpha < 255.

#include <cstdint>
#include <string>

namespace waya {

struct Color {
    std::uint32_t rgba = 0;   // 0xRRGGBBAA

    constexpr Color() = default;
    constexpr explicit Color(std::uint32_t v) : rgba(v) {}

    constexpr std::uint8_t r() const { return (rgba >> 24) & 0xFF; }
    constexpr std::uint8_t g() const { return (rgba >> 16) & 0xFF; }
    constexpr std::uint8_t b() const { return (rgba >> 8)  & 0xFF; }
    constexpr std::uint8_t a() const { return rgba & 0xFF; }
    constexpr bool has_alpha() const { return a() != 0xFF; }

    /// The legacy 0xRRGGBB form the existing fg()/bg() take.
    constexpr std::uint32_t opaque() const { return (rgba >> 8) & 0xFFFFFF; }

    /// A new Color with a different alpha (0..1).
    constexpr Color alpha(float f) const {
        std::uint8_t na = (std::uint8_t)(f <= 0 ? 0 : f >= 1 ? 255 : f * 255 + 0.5f);
        return Color((rgba & 0xFFFFFF00u) | na);
    }
    /// Mix toward `other` by t (0..1) \u2014 linear per channel. For tints/shades.
    constexpr Color mix(Color other, float t) const {
        auto lerp = [t](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
            return (std::uint8_t)(x + (int)((y - x) * t + 0.5f)); };
        return rgb8(lerp(r(),other.r()), lerp(g(),other.g()), lerp(b(),other.b()));
    }
    constexpr Color lighten(float t) const { return mix(Color(0xFFFFFFFFu), t); }
    constexpr Color darken(float t)  const { return mix(Color(0x000000FFu), t); }

    /// CSS serialisation: #rrggbb when opaque, rgba(...) when translucent.
    std::string css() const {
        static const char* H = "0123456789abcdef";
        if (!has_alpha()) {
            std::string s = "#";
            std::uint32_t c = opaque();
            for (int sh = 20; sh >= 0; sh -= 4) s += H[(c >> sh) & 0xF];
            return s;
        }
        char buf[48];
        std::snprintf(buf, sizeof(buf), "rgba(%u,%u,%u,%.3g)", r(), g(), b(), a() / 255.0);
        return buf;
    }

    // helper ctors
    static constexpr Color rgb8(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
        return Color(((std::uint32_t)r << 24) | ((std::uint32_t)g << 16) | ((std::uint32_t)b << 8) | 0xFF);
    }
    bool operator==(const Color&) const = default;
};

/// `rgb(0x6366f1)` / `rgb(99,102,241)` \u2014 an opaque colour.
constexpr Color rgb(std::uint32_t hex) { return Color((hex << 8) | 0xFF); }
constexpr Color rgb(int r, int g, int b) { return Color::rgb8((std::uint8_t)r, (std::uint8_t)g, (std::uint8_t)b); }
/// `rgba(0,0,0, .4)` \u2014 a translucent colour (alpha 0..1).
constexpr Color rgba(int r, int g, int b, float a) { return Color::rgb8((std::uint8_t)r,(std::uint8_t)g,(std::uint8_t)b).alpha(a); }
constexpr Color rgba(std::uint32_t hex, float a) { return rgb(hex).alpha(a); }

/// `hsl(210, .9, .5)` — hue in degrees, saturation + lightness 0..1.
constexpr Color hsl(float h, float s, float l) {
    // normalise hue into [0,360)
    h -= 360.0f * (float)(int)(h / 360.0f); if (h < 0) h += 360.0f;
    auto fabsf_ = [](float x){ return x < 0 ? -x : x; };
    float C = (1.0f - fabsf_(2.0f * l - 1.0f)) * s;   // chroma
    float hp = h / 60.0f;
    float X = C * (1.0f - fabsf_(hp - 2.0f * (float)(int)(hp / 2.0f) - 1.0f));
    float m = l - C / 2.0f;
    float rr = 0, gg = 0, bb = 0;
    if      (hp < 1) { rr = C; gg = X; }
    else if (hp < 2) { rr = X; gg = C; }
    else if (hp < 3) { gg = C; bb = X; }
    else if (hp < 4) { gg = X; bb = C; }
    else if (hp < 5) { rr = X; bb = C; }
    else             { rr = C; bb = X; }
    auto to8 = [](float v) -> std::uint8_t {
        float x = v * 255.0f + 0.5f; return (std::uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x); };
    return Color::rgb8(to8(rr + m), to8(gg + m), to8(bb + m));
}

// ── A named palette (slate / indigo family) ─────────────────────────────────
// Values, not magic numbers. `fg(indigo)` instead of `fg(0x6366f1)`. The ctor
// helpers (rgb/rgba/hsl) live here too, so `using namespace waya::color` brings
// the whole colour vocabulary in one import.
namespace color {
using waya::rgb;
using waya::rgba;
using waya::hsl;
inline constexpr Color transparent = Color(0x00000000u);
inline constexpr Color black   = rgb(0x000000);
inline constexpr Color white   = rgb(0xffffff);
inline constexpr Color slate50 = rgb(0xf8fafc);
inline constexpr Color slate100= rgb(0xf1f5f9);
inline constexpr Color slate300= rgb(0xcbd5e1);
inline constexpr Color slate400= rgb(0x94a3b8);
inline constexpr Color slate600= rgb(0x475569);
inline constexpr Color slate800= rgb(0x1e293b);
inline constexpr Color slate900= rgb(0x0f172a);
inline constexpr Color ink     = rgb(0xe2e8f0);
inline constexpr Color muted   = rgb(0x94a3b8);
inline constexpr Color indigo  = rgb(0x6366f1);
inline constexpr Color violet  = rgb(0x8b5cf6);
inline constexpr Color cyan    = rgb(0x22d3ee);
inline constexpr Color sky     = rgb(0x38bdf8);
inline constexpr Color emerald = rgb(0x34d399);
inline constexpr Color green   = rgb(0x22c55e);
inline constexpr Color amber   = rgb(0xf59e0b);
inline constexpr Color rose    = rgb(0xf43f5e);
inline constexpr Color red     = rgb(0xef4444);
} // namespace color

} // namespace waya
