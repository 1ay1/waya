#pragma once
/// \file component.hpp
/// Reusable components, made fast and effortless. A component in waya is already
/// just a function returning a node — but rebuilding a heavy subtree every frame
/// when its inputs didn't change is wasted work, and wiring memoisation by hand
/// is fiddly. This file makes it a one-liner.
///
///   // memoise a call site by its props: the closure runs only when a prop
///   // changes; otherwise the cached node is reused (and the diff O(1)-skips it).
///   memo(user.id, user.name, user.avatar, [&]{
///       return expensive_profile_card(user);
///   })
///
///   // define a reusable, auto-memoised component once:
///   auto Avatar = component([](std::string url, int size){
///       return image(url) | w(size) | h(size) | round(999);
///   });
///   Avatar("/a.png", 40);   // rebuilt only when (url,size) change
///
/// HOW IT'S FAST. Two layers cooperate: (1) `memo` skips *building* the subtree
/// when its props are unchanged (returns the cached NodeRef); (2) even without
/// memo, the diff already skips *rendering* an unchanged subtree in O(1) via its
/// content hash. memo is the win when BUILDING is itself costly (a big list, a
/// chart) and its inputs change rarely.
///
/// CACHE KEY. Each memo slot is keyed by (the builder's unique lambda TYPE) +
/// (the hash of its props). A lambda type is unique per source expression, so
/// two distinct call sites can never collide; the props hash separates repeated
/// calls of the SAME component (e.g. one per list row). No ids to invent.

#include "node.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

namespace waya::surface {

namespace detail {
struct MemoSlot { std::uint64_t deps = 0; NodeRef node; };
// Thread-local: view() runs on one owner thread per session.
inline thread_local std::unordered_map<std::uint64_t, MemoSlot> g_memo2;

// ── prop hashing: fold any set of props into a 64-bit deps hash ─────────────
inline void hash_bytes(std::uint64_t& h, std::string_view s){
    for (char c : s) { h ^= (std::uint8_t)c; h *= 1099511628211ull; }
}
inline void hash_one(std::uint64_t& h, std::string_view s){ hash_bytes(h, s); }
inline void hash_one(std::uint64_t& h, const std::string& s){ hash_bytes(h, s); }
inline void hash_one(std::uint64_t& h, const char* s){ hash_bytes(h, std::string_view{s}); }
inline void hash_one(std::uint64_t& h, Color c){ h ^= c.rgba; h *= 1099511628211ull; }
template <typename T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
void hash_one(std::uint64_t& h, T v){
    std::uint64_t u = 0;
    if constexpr (std::is_enum_v<T>) u = (std::uint64_t)(std::underlying_type_t<T>)v;
    else if constexpr (std::is_floating_point_v<T>) { double d = (double)v; std::memcpy(&u, &d, sizeof(u)); }
    else u = (std::uint64_t)v;
    h ^= u; h *= 1099511628211ull;
}
template <typename... Ps>
std::uint64_t hash_props(const Ps&... ps){
    std::uint64_t h = 1469598103934665603ull;
    (hash_one(h, ps), ...);
    return h;
}

// A stable per-type salt for the builder/component type, so distinct call sites
// (distinct lambda types) never share a cache slot.
template <typename T>
std::uint64_t type_salt(){
    static const std::uint64_t s = [](){
        std::uint64_t h = 1469598103934665603ull; hash_bytes(h, typeid(T).name()); return h ? h : 1;
    }();
    return s;
}
} // namespace detail

/// `memo(props..., build)` — memoise a subtree by its props. The last argument is
/// a `[&]{ return NodeRef; }` builder; the leading arguments are the props it
/// depends on. `build` runs only when the props change; otherwise the cached
/// node is returned (and the diff O(1)-skips it). No ids to invent.
///
///   memo(item.id, item.title, item.done, [&]{ return todo_row(item); })
template <typename... Args>
NodeRef memo(Args&&... args) {
    static_assert(sizeof...(Args) >= 1, "memo needs at least the build lambda");
    return [&]<std::size_t... I>(std::index_sequence<I...>) -> NodeRef {
        auto tup = std::forward_as_tuple(std::forward<Args>(args)...);
        auto&& build = std::get<sizeof...(Args) - 1>(tup);
        using Build = std::decay_t<decltype(build)>;
        std::uint64_t deps = detail::hash_props(std::get<I>(tup)...);
        std::uint64_t key = detail::type_salt<Build>() ^ deps;
        auto& slot = detail::g_memo2[key];
        if (!slot.node || slot.deps != deps) { slot.deps = deps; slot.node = build(); }
        return slot.node;
    }(std::make_index_sequence<sizeof...(Args) - 1>{});
}

/// `component(fn)` — turn a plain `fn(Props...) -> NodeRef` into a reusable,
/// AUTO-MEMOISED component: calling it rebuilds only when the props change.
/// Distinct components (distinct `fn` types) get distinct cache slots; repeated
/// calls of the same component with the same props reuse the cached node.
template <typename Fn>
struct Component {
    Fn fn;
    template <typename... Props>
    NodeRef operator()(const Props&... props) const {
        std::uint64_t deps = detail::hash_props(props...);
        std::uint64_t key = detail::type_salt<Fn>() ^ deps;
        auto& slot = detail::g_memo2[key];
        if (!slot.node || slot.deps != deps) { slot.deps = deps; slot.node = fn(props...); }
        return slot.node;
    }
};
template <typename Fn> Component<Fn> component(Fn fn) { return Component<Fn>{ std::move(fn) }; }

} // namespace waya::surface
