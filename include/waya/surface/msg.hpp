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

#include <any>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <variant>
#include <vector>

namespace waya::surface::detail {

/// One registered handler: its wire token and an erased mapper. The mapper takes
/// the event's runtime value (an input's text, a dropped payload, a pressed key;
/// empty for a plain tap) and returns the app's Msg as a std::any. It's erased so
/// the non-templated Node can store it; resolve_msg any_casts it back to Msg.
struct MsgEntry { int token; std::function<std::any(const std::string&)> make; };

/// Per-render, per-thread registry. `view()` runs on one owner thread; we clear
/// it before each render and fill it as handlers are wired. Tokens are derived
/// from registration ORDER (a salt), stable across renders (same view structure
/// → same order), so a click always resolves against the current render's table.
struct MsgTable {
    std::vector<MsgEntry> entries;
    std::uint64_t salt = 0;
};
inline thread_local MsgTable g_msg_table;

inline void begin_msg_capture() { g_msg_table.entries.clear(); g_msg_table.salt = 0; }

/// A stable token from a 64-bit seed (folded to a positive 31-bit int, never 0).
inline int token_from(std::uint64_t h){ return (int)(((h ^ (h >> 32)) & 0x7fffffff) | 1); }

/// Register a wired handler and return a STABLE wire token. `f` maps the event's
/// runtime value to a Msg VALUE (the Program's own type). We store a thunk that
/// wraps the result in std::any HOLDING THE PROGRAM'S Msg — so resolve_msg (which
/// knows the Program's Msg) always any_casts cleanly, even when the app wrote a
/// bare alternative like `tap(Inc{})` (Msg is deduced as the variant it lands in
/// via the Fn's declared return type).
template <typename Msg>
int register_handler(std::function<Msg(std::string)> f) {
    std::uint64_t h = 1469598103934665603ull ^ (g_msg_table.salt++ * 1099511628211ull);
    h ^= (std::uint64_t)typeid(Msg).hash_code(); h *= 1099511628211ull;
    int tok = token_from(h);
    g_msg_table.entries.push_back({tok,
        [f = std::move(f)](const std::string& v) -> std::any { return std::any{ f(v) }; }});
    return tok;
}

/// Register a plain Msg (a tap): the mapper ignores the runtime value.
template <typename Msg>
int register_msg(Msg m) {
    return register_handler<Msg>([m = std::move(m)](std::string) -> Msg { return m; });
}

/// Register a VALUE-CARRYING handler (input/change/key/drop).
template <typename Msg>
int register_mapper(std::function<Msg(std::string)> f) {
    return register_handler<Msg>(std::move(f));
}

/// Given a token the client sent back and the event's value, produce the Msg.
/// The stored thunk returns a std::any holding EITHER the Program's Msg (the app
/// wrote a whole variant) OR one of its alternatives (the app wrote a bare
/// `Inc{}`). We any_cast to Msg first; failing that, we fold over the variant's
/// alternatives and construct Msg from whichever one the any actually holds — so
/// both `tap(Inc{})` and `tap(Msg{Inc{}})` just work.
template <typename Msg, std::size_t... Is>
std::optional<Msg> build_variant(std::any& a, std::index_sequence<Is...>) {
    std::optional<Msg> out;
    ((out ? void() : (void)([&]{
        using Alt = std::variant_alternative_t<Is, Msg>;
        if (auto* p = std::any_cast<Alt>(&a)) out = Msg{ *p };
    }())), ...);
    return out;
}
/// True only for std::variant specialisations (int/enum Msgs are not variants).
template <typename> inline constexpr bool is_variant_v = false;
template <typename... Ts> inline constexpr bool is_variant_v<std::variant<Ts...>> = true;

template <typename Msg>
std::optional<Msg> resolve_msg(int token, const std::string& value) {
    for (auto& e : g_msg_table.entries)
        if (e.token == token) {
            std::any produced = e.make(value);
            if (auto* m = std::any_cast<Msg>(&produced)) return *m;   // whole-variant / plain Msg
            if constexpr (is_variant_v<Msg>)                          // bare alternative
                return build_variant<Msg>(produced, std::make_index_sequence<std::variant_size_v<Msg>>{});
        }
    return std::nullopt;
}

} // namespace waya::surface::detail
