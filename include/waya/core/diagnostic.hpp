#pragma once
/// \file diagnostic.hpp
/// Compile-time message builder for C++26 `static_assert` (P2741).
///
/// waya authors its OWN error messages so a web developer reads a sentence, not
/// a template substitution dump. Discovered in the Phase 0 spike: the message
/// must be a core constant expression exposing `data()`/`size()`, which is what
/// `FixedStr` provides. See DESIGN.md §10.1.
///
/// Companion rule (also from the spike): fire the assert on a plain `bool`
/// inside a helper *type*, never on a concept inside a `requires` clause, or
/// the compiler appends its own derivation. Ship `-ftemplate-backtrace-limit=1`.

#include <array>
#include <cstddef>
#include <string_view>

namespace waya::diag {

/// A constexpr string builder whose data()/size() are usable as a
/// static_assert message. `N` is a generous upper bound on the message length.
template <std::size_t N = 512>
struct Msg {
    std::array<char, N> buf{};
    std::size_t         len = 0;

    constexpr Msg& operator+=(std::string_view s) {
        for (char c : s)
            if (len < N - 1) buf[len++] = c;
        return *this;
    }

    [[nodiscard]] constexpr const char* data() const { return buf.data(); }
    [[nodiscard]] constexpr std::size_t size() const { return len; }
};

} // namespace waya::diag
