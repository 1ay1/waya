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
struct MemoSlot {
    std::uint64_t deps = 0;
    NodeRef node;
    std::uint32_t seen = 0;
    // The msg-table entries (tap/input/key handlers) this subtree registered
    // when it was built. A cache HIT skips the builder — and therefore skips
    // re-registering these — which would (a) leave the returned node's tokens
    // unresolvable and (b) desync every later sibling's order-derived token.
    // So we RECORD them at build and REPLAY them on every hit, restoring the
    // table to exactly what a fresh build would have produced. This is what
    // makes memo() safe on INTERACTIVE subtrees, not just static ones.
    std::vector<MsgEntry> handlers;
    std::uint64_t salt_used = 0;   // how many salt ticks the build consumed
};
// Thread-local: view() runs on one owner thread per session.
inline thread_local std::unordered_map<std::uint64_t, MemoSlot> g_memo2;
// Monotonic render generation for this thread's session. Bumped once per view()
// pass (see memo_begin_frame). Every memo/component hit stamps its slot with the
// current generation; slots not touched for a while are swept. Without this the
// cache grows without bound whenever memo keys churn (e.g. a game memoising by a
// per-frame-changing coordinate) — a real leak in a long-lived session.
inline thread_local std::uint32_t g_memo_gen = 0;

/// Begin a new render generation and, periodically, sweep slots that haven't
/// been touched in the last few frames. Called once at the top of each view()
/// pass by the runtime. Sweeping is amortised (every 64th frame) so the common
/// path stays O(touched), not O(cache). A slot survives if it was seen within
/// the last `keep` generations — long enough that a component which renders on
/// alternate frames (e.g. behind a conditional) isn't evicted prematurely.
inline void memo_begin_frame() {
    ++g_memo_gen;
    constexpr std::uint32_t sweep_every = 64;
    constexpr std::uint32_t keep       = 8;
    if ((g_memo_gen % sweep_every) != 0) return;
    for (auto it = g_memo2.begin(); it != g_memo2.end();) {
        // unsigned wrap is fine: (gen - seen) is the age in generations.
        if ((std::uint32_t)(g_memo_gen - it->second.seen) > keep) it = g_memo2.erase(it);
        else ++it;
    }
}

/// Drop this thread's memo cache entirely (session teardown). Keeps a recycled
/// worker thread from carrying a previous session's cached nodes.
inline void memo_reset() { g_memo2.clear(); g_memo_gen = 0; }

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
        if (!slot.node || slot.deps != deps) {
            // BUILD: record exactly the msg-table entries + salt ticks this
            // subtree registers, so a later cache hit can reproduce them.
            std::size_t base = detail::g_msg_table.entries.size();
            std::uint64_t salt0 = detail::g_msg_table.salt;
            slot.deps = deps;
            slot.node = build();
            slot.handlers.assign(detail::g_msg_table.entries.begin() + base,
                                 detail::g_msg_table.entries.end());
            slot.salt_used = detail::g_msg_table.salt - salt0;
        } else {
            // HIT: replay the recorded handlers + salt advance so the token
            // table matches a fresh build (keeps this node's taps resolvable
            // AND every later sibling's order-derived token correct).
            detail::g_msg_table.entries.insert(detail::g_msg_table.entries.end(),
                                               slot.handlers.begin(), slot.handlers.end());
            detail::g_msg_table.salt += slot.salt_used;
        }
        slot.seen = detail::g_memo_gen;
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
        if (!slot.node || slot.deps != deps) {
            std::size_t base = detail::g_msg_table.entries.size();
            std::uint64_t salt0 = detail::g_msg_table.salt;
            slot.deps = deps;
            slot.node = fn(props...);
            slot.handlers.assign(detail::g_msg_table.entries.begin() + base,
                                 detail::g_msg_table.entries.end());
            slot.salt_used = detail::g_msg_table.salt - salt0;
        } else {
            detail::g_msg_table.entries.insert(detail::g_msg_table.entries.end(),
                                               slot.handlers.begin(), slot.handlers.end());
            detail::g_msg_table.salt += slot.salt_used;
        }
        slot.seen = detail::g_memo_gen;
        return slot.node;
    }
};
template <typename Fn> Component<Fn> component(Fn fn) { return Component<Fn>{ std::move(fn) }; }

/// `list(id, range, key_fn, view_fn)` — a MEMOISED keyed container. This is the
/// primitive that makes a big, mostly-static list O(changed) per frame instead
/// of O(n). It does two things at once:
///
///   (1) builds each child through `view_fn` — wrap that in `memo(...)` (or use a
///       `component`) so an unchanged row is an O(1) cache hit, not a rebuild;
///   (2) memoises the CONTAINER itself: it hashes only the child NODE IDENTITIES
///       (their pointers) + count — O(n) cheap reads, no deep re-hash — and when
///       that identity signature is unchanged from last frame, returns the SAME
///       cached container node. So a frame where nothing in the list changed
///       skips the vector→box construction AND the bottom-up re-hash of the
///       whole subtree entirely. Combined with per-row memo, an untouched
///       1000-row list costs ~n cheap pointer reads per frame, and the diff then
///       O(1)-skips the whole container by its unchanged hash.
///
/// `id` disambiguates multiple lists in one view (like a call-site key). `flow`
/// picks the container axis (col by default). Children are keyed by `key_fn` so
/// reorders reconcile by identity (moves, not re-renders).
///
///   list(0, todos, [](auto& t){ return std::to_string(t.id); },
///                   [&](auto& t){ return memo(t.id, t.done, t.title,
///                                             [&]{ return todo_row(t); }); })
template <typename Range, typename KeyFn, typename ViewFn>
NodeRef list(std::uint64_t id, const Range& range, KeyFn key_fn, ViewFn view_fn,
             Flow flow = Flow::col) {
    // Build (or cache-hit) every child, applying its key. This is where per-row
    // memo pays off; here we only pay the key set + a finalize if the row was
    // freshly built (a cached row already carries its key + hash).
    std::vector<NodeRef> kids;
    if constexpr (requires { range.size(); }) kids.reserve(range.size());
    std::uint64_t sig = 1469598103934665603ull;
    detail::hash_one(sig, id);
    detail::hash_one(sig, (std::uint64_t)flow);
    for (const auto& item : range) {
        NodeRef c = view_fn(item);
        std::string k = key_fn(item);
        if (c->key != k) { c->key = std::move(k); finalize(*c); }
        // identity signature: the child's own content hash already captures
        // "did this row change"; fold it in so the container is cached exactly
        // when the whole visible list is unchanged.
        detail::hash_one(sig, c->hash);
        kids.push_back(std::move(c));
    }

    std::uint64_t key = detail::type_salt<KeyFn>() ^ (id * 1099511628211ull) ^ 0x115700D5ull;
    auto& slot = detail::g_memo2[key];
    slot.seen = detail::g_memo_gen;
    if (slot.node && slot.deps == sig) return slot.node;   // list unchanged: reuse container

    // Rebuild the container node from the (possibly cached) children.
    auto n = std::make_shared<Node>();
    n->kind = Kind::box; n->style.flow = flow; n->kids = std::move(kids);
    finalize(*n);
    slot.deps = sig; slot.node = n;
    return n;
}

/// `list_versioned(id, version, range, key_fn, view_fn)` — the FASTEST list: when
/// you can supply a cheap `version` value (e.g. a counter you bump in update()
/// whenever the list's data changes, or a hash you already keep), an unchanged
/// frame returns the cached container in O(1) — it never even iterates the range,
/// never does a per-row memo lookup, never allocates a key string. This is the
/// SolidJS-grade path: a list of any size costs O(1) on frames where its data
/// didn't change, and O(n) only when it did. Use it for big lists on a hot
/// animation loop where OTHER state (a clock, a cursor) changes every frame but
/// the list rarely does.
///
///   // in Model bump `todos_ver` whenever you touch `todos`
///   list_versioned(0, m.todos_ver, m.todos,
///       [](auto& t){ return std::to_string(t.id); },
///       [&](auto& t){ return todo_row(t); })
template <typename Range, typename KeyFn, typename ViewFn>
NodeRef list_versioned(std::uint64_t id, std::uint64_t version, const Range& range,
                       KeyFn key_fn, ViewFn view_fn, Flow flow = Flow::col) {
    std::uint64_t key = detail::type_salt<KeyFn>() ^ (id * 1099511628211ull) ^ 0x5EC0FFEEull;
    auto& slot = detail::g_memo2[key];
    slot.seen = detail::g_memo_gen;
    // deps holds the version. O(1) short-circuit: the range is never touched.
    // (version is offset by 1 so an initial 0 version still forces a first build
    // — an empty slot has deps==0.)
    std::uint64_t ver = version + 1;
    if (slot.node && slot.deps == ver) return slot.node;

    std::vector<NodeRef> kids;
    if constexpr (requires { range.size(); }) kids.reserve(range.size());
    for (const auto& item : range) {
        NodeRef c = view_fn(item);
        std::string k = key_fn(item);
        if (c->key != k) { c->key = std::move(k); finalize(*c); }
        kids.push_back(std::move(c));
    }
    auto n = std::make_shared<Node>();
    n->kind = Kind::box; n->style.flow = flow; n->kids = std::move(kids);
    finalize(*n);
    slot.deps = ver; slot.node = n;
    return n;
}

} // namespace waya::surface
