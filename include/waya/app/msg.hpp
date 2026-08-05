#pragma once
/// \file msg.hpp
/// Wiring `Msg` values to view events. `on_msg(m)` tags an element so a click
/// (or other event) delivers `m` to `update`. The live runtime maps the tag
/// back to the stored `Msg`.
///
///   button_(text("+")) | on_msg(Inc{})
///   button_(text("−")) | on_msg(Dec{})
///
/// Implementation: during a render, each `on_msg(m)` registers `m` in a
/// thread-local table keyed by a stable index, and stamps the element with
/// `data-waya-msg="<index>"`. The client sends that index back; the runtime
/// looks up the Msg. Simple, allocation-light, and it keeps `view` pure from
/// the caller's perspective (the table is render scaffolding, not app state).

#include "../dsl/element.hpp"

#include <any>
#include <charconv>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace waya::app::detail {

/// Per-render message table. `std::any` lets one table hold any Program's Msg.
/// Keyed by a STABLE id (not registration order — C++ doesn't order function
/// argument evaluation, so `div_(a|on_msg, b|on_msg)` registers unpredictably).
/// The id is a monotonically assigned index PER DISTINCT variant alternative +
/// payload signature, derived deterministically from the Msg's serialised form.
struct MsgEntry { std::string id; std::any msg; };
struct MsgTable {
    std::vector<MsgEntry> entries;
    bool capturing = false;
};

inline MsgTable& msg_table() {
    static thread_local MsgTable t;
    return t;
}

template <typename Msg>
void begin_msg_capture() {
    auto& t = msg_table();
    t.entries.clear();
    t.capturing = true;
}

/// A stable id for a Msg: the variant alternative index plus a hash of its
/// bytes. Order-independent and collision-resistant for typical Msg types.
template <typename Msg>
std::string msg_id(const Msg& m) {
    std::size_t alt = 0;
    if constexpr (requires { m.index(); }) alt = m.index();
    // Hash the object's bytes (Msg alternatives are usually trivial/aggregate).
    const auto* bytes = reinterpret_cast<const unsigned char*>(&m);
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < sizeof(Msg); ++i) { h ^= bytes[i]; h *= 1099511628211ull; }
    return std::to_string(alt) + "_" + std::to_string(h & 0xffffff);
}

/// Register a Msg, returning its stable id (dedups repeats within a render).
template <typename Msg>
std::string register_msg(Msg m) {
    auto& t = msg_table();
    std::string id = msg_id(m);
    for (auto& e : t.entries) if (e.id == id) return id;   // already present
    t.entries.push_back({id, std::move(m)});
    return id;
}

/// Look up a Msg by the id the client sent back.
template <typename Msg>
std::optional<Msg> lookup_msg(std::string_view id) {
    auto& t = msg_table();
    for (auto& e : t.entries)
        if (e.id == id)
            if (auto* p = std::any_cast<Msg>(&e.msg)) return *p;
    return std::nullopt;
}

/// Extract `?name=value` from a request path (tiny, for the dev runtime).
inline std::string query_param(std::string_view path, std::string_view name) {
    auto q = path.find('?');
    if (q == std::string_view::npos) return {};
    std::string_view qs = path.substr(q + 1);
    std::string key = std::string(name) + "=";
    for (std::size_t p = 0; p < qs.size();) {
        auto amp = qs.find('&', p);
        std::string_view pair = qs.substr(p, amp == std::string_view::npos ? amp : amp - p);
        if (pair.rfind(key, 0) == 0) return std::string(pair.substr(key.size()));
        if (amp == std::string_view::npos) break;
        p = amp + 1;
    }
    return {};
}

} // namespace waya::app::detail

namespace waya::dsl {

/// `on_msg(m)` — deliver `m` to `update` when this element is clicked. Returns a
/// pipe token that stamps `data-waya-msg="<id>"`.
template <typename Msg>
struct OnMsg { Msg msg; };
template <typename Msg> OnMsg<Msg> on_msg(Msg m) { return {std::move(m)}; }

template <Tag T, ElemCfg Cfg, typename... Cs, typename Msg>
auto operator|(ElemNode<T, Cfg, Cs...> n, OnMsg<Msg> tag) {
    std::string id = waya::app::detail::register_msg(std::move(tag.msg));
    n.attrs.extra.emplace_back("data-waya-msg", std::move(id));
    return ElemNode<T, detail::set_attrs(Cfg), Cs...>{n.attrs, n.style, std::move(n.children)};
}

} // namespace waya::dsl
