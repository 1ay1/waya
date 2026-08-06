#pragma once
/// \file msg.hpp
/// Typed messages on the wire. The surface `Msg` is the Program's own type — a
/// `std::variant` of message structs (`Inc{}`, `SetName{std::string}`), exactly
/// like maya/Elm — NOT a bare int. But the browser can't serialise a C++ value,
/// so at render time each wired `Msg` is registered in a per-render table and an
/// opaque integer TOKEN is emitted on the element (`data-tap="7"`). When the
/// click comes back with that token, the runtime looks the real `Msg` value back
/// up and dispatches it, typed. The int never escapes the plumbing; app code is
/// fully type-safe and payload-carrying.
///
/// This mirrors the DOM `waya::app` runtime (include/waya/app/msg.hpp) — the
/// surface runtime now uses the same maya-faithful mechanism.

#include <any>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <typeinfo>
#include <vector>

namespace waya::surface::detail {

/// One registered handler: its wire token and an erased mapper. The mapper takes
/// the event's runtime value (an input's text, a dropped payload, a pressed key;
/// empty for a plain tap) and returns the app's Msg. Erased as std::any holding
/// a std::function<Msg(std::string)> so the non-templated Node can store it.
struct MsgEntry { int token; std::any mapper; };

/// Per-render, per-thread registry. `view()` runs on one owner thread; we clear
/// it before each render and fill it as handlers are wired. Tokens are a STABLE
/// hash so the same handler gets the same token across renders — a click always
/// resolves against the current render's table even after the tree changed.
struct MsgTable {
    std::vector<MsgEntry> entries;
    std::uint64_t salt = 0;   // disambiguates value-mapper handlers (see below)
};
inline thread_local MsgTable g_msg_table;

inline void begin_msg_capture() { g_msg_table.entries.clear(); g_msg_table.salt = 0; }

/// A stable token from a 64-bit seed (folded to a positive 31-bit int, never 0).
inline int token_from(std::uint64_t h){ return (int)(((h ^ (h >> 32)) & 0x7fffffff) | 1); }

/// Stable token for a plain Msg VALUE (variant-alt index + byte hash).
template <typename Msg>
int value_token(const Msg& m) {
    std::uint64_t alt = 0;
    if constexpr (requires { m.index(); }) alt = m.index();
    const auto* bytes = reinterpret_cast<const unsigned char*>(&m);
    std::uint64_t h = 1469598103934665603ull ^ (alt * 1099511628211ull);
    for (std::size_t i = 0; i < sizeof(Msg); ++i) { h ^= bytes[i]; h *= 1099511628211ull; }
    return token_from(h);
}

/// Register a plain Msg (a tap): token = hash of the value, mapper ignores the
/// runtime value and returns the fixed Msg. Dedups repeats.
template <typename Msg>
int register_msg(Msg m) {
    int tok = value_token(m);
    for (auto& e : g_msg_table.entries) if (e.token == tok) return tok;
    g_msg_table.entries.push_back({tok, std::any{ std::function<Msg(std::string)>(
        [m = std::move(m)](std::string) { return m; }) }});
    return tok;
}

/// Register a VALUE-CARRYING handler (input/change/key/drop): the mapper turns
/// the runtime value into a Msg. The token can't be content-hashed (the closure
/// has no stable identity), so we salt-hash by registration order + type — stable
/// as long as the view's handler set is stable between renders (it is: same view
/// structure → same order).
template <typename Msg>
int register_mapper(std::function<Msg(std::string)> f) {
    std::uint64_t h = 1469598103934665603ull ^ (g_msg_table.salt++ * 1099511628211ull);
    h ^= (std::uint64_t)typeid(Msg).hash_code(); h *= 1099511628211ull;
    int tok = token_from(h);
    g_msg_table.entries.push_back({tok, std::any{ std::move(f) }});
    return tok;
}

/// Given a token the client sent back and the event's value, produce the Msg.
template <typename Msg>
std::optional<Msg> resolve_msg(int token, const std::string& value) {
    for (auto& e : g_msg_table.entries)
        if (e.token == token)
            if (auto* f = std::any_cast<std::function<Msg(std::string)>>(&e.mapper))
                return (*f)(value);
    return std::nullopt;
}

} // namespace waya::surface::detail
