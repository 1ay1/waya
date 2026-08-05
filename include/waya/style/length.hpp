#pragma once
/// \file length.hpp
/// A typed CSS dimension — a value plus a unit — never a bare string.
/// Constructed via literals (12_px, 1.5_rem, 50_pct) so nonsense like a bare
/// number where a length is required cannot be written.

#include <cstdint>

namespace waya::style {

enum class Unit : std::uint8_t { zero, px, rem, em, pct, vw, vh, fr, ch, auto_ };

struct Len {
    float value = 0;
    Unit  unit  = Unit::zero;
    constexpr bool operator==(const Len&) const = default;
};

inline constexpr Len zero{0, Unit::zero};
inline constexpr Len autolen{0, Unit::auto_};

// Factory helpers (used by the user-facing UDLs below and directly in code).
constexpr Len px(float v)  { return {v, Unit::px};  }
constexpr Len rem(float v) { return {v, Unit::rem}; }
constexpr Len em(float v)  { return {v, Unit::em};  }
constexpr Len pct(float v) { return {v, Unit::pct}; }
constexpr Len vw(float v)  { return {v, Unit::vw};  }
constexpr Len vh(float v)  { return {v, Unit::vh};  }
constexpr Len fr(float v)  { return {v, Unit::fr};  }
constexpr Len ch(float v)  { return {v, Unit::ch};  }

namespace literals {
consteval Len operator""_px(long double v)        { return px(static_cast<float>(v)); }
consteval Len operator""_px(unsigned long long v) { return px(static_cast<float>(v)); }
consteval Len operator""_rem(long double v)        { return rem(static_cast<float>(v)); }
consteval Len operator""_rem(unsigned long long v) { return rem(static_cast<float>(v)); }
consteval Len operator""_em(long double v)         { return em(static_cast<float>(v)); }
consteval Len operator""_em(unsigned long long v)  { return em(static_cast<float>(v)); }
consteval Len operator""_pct(long double v)        { return pct(static_cast<float>(v)); }
consteval Len operator""_pct(unsigned long long v) { return pct(static_cast<float>(v)); }
consteval Len operator""_vw(long double v)         { return vw(static_cast<float>(v)); }
consteval Len operator""_vw(unsigned long long v)  { return vw(static_cast<float>(v)); }
consteval Len operator""_vh(long double v)         { return vh(static_cast<float>(v)); }
consteval Len operator""_vh(unsigned long long v)  { return vh(static_cast<float>(v)); }
consteval Len operator""_fr(unsigned long long v)  { return fr(static_cast<float>(v)); }
} // namespace literals

} // namespace waya::style
