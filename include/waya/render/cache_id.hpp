#pragma once
/// \file cache_id.hpp
/// `CacheId` — a typed 64-bit content-hash identity, ported from maya.
///
/// maya's Witness Chain uses a `CacheId` as the key into the renderer's
/// component cache: `CacheIdBuilder{}.add("row").add(id).add(status).build()`.
/// Two genuinely-different contents cannot collide unless their hashes collide.
/// waya reuses it verbatim to memoise subtrees — if a component's inputs hash
/// the same as last frame, its cached VNode is reused instead of rebuilt.
///
/// Zero allocation: the builder folds everything into one `uint64_t`. Each
/// `add(v)` mixes both a TYPE TAG and the value bytes, so `add("t").add(1).add(23)`
/// and `add("t").add(12).add(3)` differ.

#include <bit>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace waya {

/// An opaque 64-bit content identity. Trivially copyable; `empty()` means
/// "no cache key — always rebuild".
struct CacheId {
    std::uint64_t value = 0;
    constexpr bool empty() const { return value == 0; }
    constexpr bool operator==(const CacheId&) const = default;
};

/// Fluent builder. `add()` chains; `build()` finalises.
class CacheIdBuilder {
public:
    constexpr CacheIdBuilder& add(std::string_view s) {
        mix_tag('s');
        for (char c : s) mix_byte(static_cast<std::uint8_t>(c));
        return *this;
    }
    constexpr CacheIdBuilder& add(const char* s) {
        return add(std::string_view{s});
    }
    template <typename T>
        requires std::is_integral_v<T>
    constexpr CacheIdBuilder& add(T v) {
        mix_tag('i');
        auto u = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(v));
        for (int i = 0; i < 8; ++i) mix_byte((u >> (i * 8)) & 0xFF);
        return *this;
    }
    template <typename T>
        requires std::is_enum_v<T>
    constexpr CacheIdBuilder& add(T v) {
        return add(static_cast<std::underlying_type_t<T>>(v));
    }
    constexpr CacheIdBuilder& add(bool b) { mix_tag('b'); mix_byte(b ? 1 : 0); return *this; }
    constexpr CacheIdBuilder& add(double d) {
        mix_tag('d');
        std::uint64_t bits = 0;
        // constexpr bit_cast in C++26
        bits = std::bit_cast<std::uint64_t>(d);
        for (int i = 0; i < 8; ++i) mix_byte((bits >> (i * 8)) & 0xFF);
        return *this;
    }

    [[nodiscard]] constexpr CacheId build() const {
        // Fold to non-zero so a real key never looks "empty".
        return { h_ == 0 ? 1 : h_ };
    }

private:
    std::uint64_t h_ = 1469598103934665603ull;   // FNV-1a offset
    constexpr void mix_byte(std::uint8_t b) { h_ ^= b; h_ *= 1099511628211ull; }
    constexpr void mix_tag(char t) { mix_byte(static_cast<std::uint8_t>(t)); }
};

/// Convenience: `cache_id("row", id, status)` — build from a value pack.
template <typename... Ts>
[[nodiscard]] constexpr CacheId cache_id(Ts... vs) {
    CacheIdBuilder b;
    (b.add(vs), ...);
    return b.build();
}

} // namespace waya
