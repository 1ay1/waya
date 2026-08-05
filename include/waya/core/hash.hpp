#pragma once
/// \file hash.hpp
/// FNV-1a hashing, constexpr. Used for interned atomic-class names and, later,
/// the render cache's content identity (the CacheId ported from maya).

#include <cstdint>
#include <string>
#include <string_view>

namespace waya {

/// 32-bit FNV-1a. Deterministic across runs — the interned class names it
/// produces are stable, so a page's generated stylesheet is reproducible.
[[nodiscard]] constexpr std::uint32_t fnv1a(std::string_view s) noexcept {
    std::uint32_t h = 2166136261u;
    for (char c : s) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}

/// Render a 32-bit value as 8 lowercase hex digits into `out` (appends).
inline void hex8(std::string& out, std::uint32_t v) {
    static constexpr char H[] = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4)
        out += H[(v >> shift) & 0xF];
}

} // namespace waya
