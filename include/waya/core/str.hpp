#pragma once
/// \file str.hpp
/// Compile-time string usable as a non-type template parameter (NTTP).
///
/// The foundation of waya's DSL: element names, class names, attribute values,
/// and arbitrary CSS property strings all travel as `Str<N>` template
/// parameters, so they are part of the *type* and cost nothing at runtime.
///
/// Mirrors maya's `Str<N>` (reference/maya/include/maya/dsl.hpp).

#include <cstddef>
#include <string_view>

namespace waya {

/// A fixed-size character buffer that is a *structural type*, hence usable as a
/// non-type template parameter: `t<"hello">`, `cls<"card">`, `fg<"#3b82f6">`.
template <std::size_t N>
struct Str {
    char data[N]{};

    consteval Str(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
    }

    /// The string content, without the trailing NUL.
    [[nodiscard]] constexpr std::string_view view() const { return {data, N - 1}; }

    /// Length in characters (excludes the trailing NUL).
    [[nodiscard]] static constexpr std::size_t size() { return N - 1; }

    constexpr bool operator==(const Str&) const = default;
};

} // namespace waya
