#pragma once
/// \file diff.hpp
/// The surface delta engine. Keeps the previous surface, diffs the next against
/// it, and emits only the changed nodes — the payload that travels to the
/// browser. Uses the subtree hash for O(1) skip of unchanged branches.
///
/// The op set is small and substrate-neutral: the client (DOM or canvas) knows
/// how to apply each one to its own representation.
///
/// Children reconcile one of two ways:
///   • positional (default): lockstep, cheap, correct when the list is stable;
///   • keyed: when children carry `key(...)`, the diff matches by key and emits
///     `move` ops for reorders instead of re-rendering every shifted row — so a
///     list that only reorders touches O(moved) nodes, not O(n), and each row's
///     own DOM (and its input focus / scroll) survives the reorder.

#include "node.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace waya::surface {

/// The op set is small, ORTHOGONAL, and TOTAL. Each op transports exactly one
/// of a node's four content channels (see docs/internals/wire-protocol-design.md)
/// and the client's action for it touches ONLY that channel:
///
///   replace    struct/identity: swap the whole element for a fresh subtree
///   set_shell  shell channel: morph attrs+class in place, children untouched
///   set_text   text channel:  set the element's text content (textContent)
///   set_inner  inner channel: set element.innerHTML (markup nodes)
///   set_prop   one reflected DOM property (value/checked/src) by name
///   remove/insert/move  structural child edits
///
/// A node that changed two channels (e.g. a button's style AND its label) emits
/// two orthogonal ops (set_shell + set_text) that cannot overlap or clobber each
/// other. There is deliberately no "set_paint" — its old, ambiguous double duty
/// ("attrs, and maybe the body, depending on kind") is what let markup()/button
/// bodies silently drop.
enum class Op : std::uint8_t {
    replace, set_shell, set_text, set_inner, set_prop, remove, insert, move
};

struct PatchOp {
    Op          op;
    std::string path;   ///< dotted child index from root, "0.2.1"
    std::string s;      ///< set_text/set_inner: body; set_prop: value; move: dest idx
    std::string prop;   ///< set_prop: the DOM property name ("value"/"checked"/"src")
    NodeRef     node;   ///< replace/insert: new subtree; set_shell: the node (shell)
    int         from = -1;  ///< move: current child index of the node being moved
    int         to   = -1;  ///< move: target child index
};
using Patch = std::vector<PatchOp>;

namespace detail {
inline std::string child(const std::string& p, std::size_t i) {
    return p.empty() ? std::to_string(i) : p + "." + std::to_string(i);
}

// A compile-time string, so a channel can name its DOM property ("value",
// "src", "checked") as a template argument. This keeps the property name in the
// type of the channel — the emitter can't be paired with the wrong name.
namespace detail_str {
template <std::size_t N> struct Name {
    char value[N]{};
    constexpr Name(const char (&s)[N]) { for (std::size_t i=0;i<N;++i) value[i]=s[i]; }
};
}

inline void diff_node(const Node& a, const Node& b, const std::string& path,
                      const NodeRef& bref, Patch& out);

// ═════════════════════════════════════════════════════════════════
//  CHANNELS  —  a node's mutable content, partitioned into typed channels.
//
//  The diff is NOT a per-kind switch of ad-hoc `if`s (that is exactly what let
//  the button label and markup body silently drop). Instead each channel is a
//  first-class value: a PROJECTION (what part of the Node it reads) paired with
//  an EMITTER (the single op that transports it). A Kind's `Schema` is the fixed
//  list of channels it owns; the diff is the generic FOLD "for every channel,
//  if the projection changed, emit its op". The knowledge "which op carries
//  which field" lives in exactly one place per channel and cannot be duplicated
//  or forgotten.
// ═════════════════════════════════════════════════════════════════

/// A channel = (does it differ between a,b?) + (emit the op that carries it).
/// Both close over nothing but the two nodes and the path, so a channel is a
/// pure description the fold can run uniformly.
struct Channel {
    bool (*differs)(const Node&, const Node&);
    void (*emit)(const Node& b, const std::string& path, const NodeRef& bref, Patch&);
};

// The universal SHELL channel: every element carries its style/wiring/attrs as
// DOM attributes+class. One op (set_shell) morphs them in place, body untouched.
inline bool shell_differs(const Node& a, const Node& b){
    return a.style != b.style || a.on_tap != b.on_tap || a.events != b.events
        || a.draggable != b.draggable || a.attrs != b.attrs || a.name != b.name
        || a.disabled != b.disabled || a.placeholder != b.placeholder
        || a.input_type != b.input_type || a.tag != b.tag
        || a.on_input != b.on_input || a.on_change != b.on_change;
}
inline constexpr Channel shell_channel{
    shell_differs,
    [](const Node&, const std::string& p, const NodeRef& b, Patch& o){
        o.push_back({Op::set_shell, p, {}, {}, b}); }
};

// Body-channel constructors — one per DOM carrier. Each names the field it reads
// and the op that transports it; that pairing is the whole spec for the channel.

/// textContent (a <span> label, a <button> label).
template <auto Field>
inline constexpr Channel text_channel{
    [](const Node& a, const Node& b){ return a.*Field != b.*Field; },
    [](const Node& b, const std::string& p, const NodeRef&, Patch& o){
        o.push_back({Op::set_text, p, b.*Field}); }
};

/// innerHTML (markup / raw SVG body).
template <auto Field>
inline constexpr Channel inner_channel{
    [](const Node& a, const Node& b){ return a.*Field != b.*Field; },
    [](const Node& b, const std::string& p, const NodeRef&, Patch& o){
        o.push_back({Op::set_inner, p, b.*Field}); }
};

/// a reflected string DOM property (value / src) named at compile time.
template <auto Field, detail_str::Name Prop>
inline constexpr Channel prop_channel{
    [](const Node& a, const Node& b){ return a.*Field != b.*Field; },
    [](const Node& b, const std::string& p, const NodeRef&, Patch& o){
        o.push_back({Op::set_prop, p, b.*Field, Prop.value}); }
};

/// a reflected boolean DOM property (checked). Serialised as "1"/"".
template <auto Field, detail_str::Name Prop>
inline constexpr Channel bool_prop_channel{
    [](const Node& a, const Node& b){ return a.*Field != b.*Field; },
    [](const Node& b, const std::string& p, const NodeRef&, Patch& o){
        o.push_back({Op::set_prop, p, (b.*Field) ? "1" : "", Prop.value}); }
};

/// a channel whose change can only be realised by swapping the whole element
/// (path geometry, a select's option set). Emits replace.
inline constexpr Channel replace_channel(bool (*differs)(const Node&, const Node&)){
    return { differs,
        [](const Node&, const std::string& p, const NodeRef& b, Patch& o){
            o.push_back({Op::replace, p, {}, {}, b}); } };
}

/// True when EVERY child on both sides carries a non-empty key AND all keys are
/// UNIQUE within their list. Only then can we reconcile by identity; a mixed or
/// duplicate-keyed list falls back to positional diffing. Uniqueness is
/// essential: the keyed planner matches each key to a single old index (via an
/// old-key→index map), so a repeated key would collapse two rows onto one slot
/// and corrupt the client DOM. Falling back to positional is always safe.
inline bool fully_keyed(const std::vector<NodeRef>& a, const std::vector<NodeRef>& b) {
    if (a.empty() && b.empty()) return false;
    // Fast bail (the common case is UNKEYED lists): if either side's first child
    // has no key, it can't be fully keyed — return before allocating any set.
    if ((!a.empty() && (!a.front() || a.front()->key.empty())) ||
        (!b.empty() && (!b.front() || b.front()->key.empty())))
        return false;
    std::unordered_set<std::string> seen;
    seen.reserve((a.size() + b.size()));
    for (auto& c : a) {
        if (!c || c->key.empty()) return false;
        if (!seen.insert(c->key).second) return false;   // duplicate key in old
    }
    seen.clear();
    for (auto& c : b) {
        if (!c || c->key.empty()) return false;
        if (!seen.insert(c->key).second) return false;   // duplicate key in new
    }
    return true;
}

/// Positional child diff — lockstep by index. The historical default.
inline void diff_children_positional(const Node& a, const Node& b,
                                     const std::string& path, Patch& out) {
    const std::size_t na = a.kids.size(), nb = b.kids.size();
    const std::size_t common = na < nb ? na : nb;
    for (std::size_t i = 0; i < common; ++i)
        diff_node(*a.kids[i], *b.kids[i], child(path, i), b.kids[i], out);
    // Remove surplus tail from the back so earlier indices stay valid as we go.
    for (std::size_t i = nb; i < na; ++i)
        out.push_back({Op::remove, child(path, na - 1 - (i - nb))});
    for (std::size_t i = na; i < nb; ++i)
        out.push_back({Op::insert, path, {}, {}, b.kids[i]});
}

/// Keyed child diff — match by `key`, emit move/insert/remove so a reordered
/// list preserves each row's DOM. O(n log n): a longest-increasing-subsequence
/// over the surviving nodes' old positions identifies the maximal set that is
/// ALREADY in the right relative order (they never move); every other node is
/// moved/inserted into place exactly once. This both minimises the number of
/// move ops on the wire AND kills the old O(n²) linear-scan reconcile.
///
/// The op semantics match the client's applier (surface/client.hpp): `move
/// [from,to]` detaches the child at `from` and reinserts before the child now
/// at `to`; `insert_at [to,html]` inserts before the child at `to`. We process
/// targets RIGHT-TO-LEFT so a just-placed successor is a stable reference point,
/// and drive every index off one running model array (`cur`) that mirrors the
/// client exactly — so `from`/`to` are always valid at apply time.
inline void diff_children_keyed(const Node& a, const Node& b,
                                const std::string& path, Patch& out) {
    const std::size_t na = a.kids.size(), nb = b.kids.size();

    // old key -> its index in a.kids (+ the node, for in-place diff).
    std::unordered_map<std::string, std::size_t> old_pos;
    old_pos.reserve(na * 2);
    for (std::size_t i = 0; i < na; ++i) old_pos[a.kids[i]->key] = i;
    std::unordered_set<std::string> want;
    want.reserve(nb * 2);
    for (auto& c : b.kids) want.insert(c->key);

    // `cur` models the client's child order as we mutate it. Start = old order.
    // 1) Remove old keys absent from the new list (back-to-front so indices stay
    //    valid), and diff-in-place the survivors against their new selves.
    std::vector<std::string> cur;
    cur.reserve(na);
    for (std::size_t i = na; i-- > 0; ) {
        if (!want.count(a.kids[i]->key))
            out.push_back({Op::remove, child(path, i)});
    }
    for (std::size_t i = 0; i < na; ++i)
        if (want.count(a.kids[i]->key)) cur.push_back(a.kids[i]->key);

    // 2) For each target position, the OLD index of that key (or -1 if new).
    //    Also diff each surviving node in place now (content patches are order-
    //    independent of the structural moves below).
    std::vector<long> new_to_old(nb);
    for (std::size_t t = 0; t < nb; ++t) {
        const NodeRef& bc = b.kids[t];
        auto it = old_pos.find(bc->key);
        if (it == old_pos.end()) new_to_old[t] = -1;
        else { new_to_old[t] = (long)it->second;
               diff_node(*a.kids[it->second], *bc, child(path, t), bc, out); }
    }

    // 3) LIS over the surviving old-indices: the target positions in the LIS are
    //    already in correct relative order and must NOT move. `in_lis[t]` marks
    //    them. (New nodes, new_to_old==-1, are never in the LIS.)
    std::vector<char> in_lis(nb, 0);
    {
        // patience sort tracking predecessors; `tails[k]` = target-index whose
        // old-index ends the best length-(k+1) increasing run so far.
        std::vector<std::size_t> tails;         // indices into new_to_old
        std::vector<long> prev(nb, -1);
        tails.reserve(nb);
        for (std::size_t t = 0; t < nb; ++t) {
            if (new_to_old[t] < 0) continue;    // skip fresh nodes
            long v = new_to_old[t];
            // binary search: first tail whose old-index >= v (strict LIS).
            std::size_t lo = 0, hi = tails.size();
            while (lo < hi) { std::size_t mid = (lo + hi) / 2;
                if (new_to_old[tails[mid]] < v) lo = mid + 1; else hi = mid; }
            prev[t] = lo > 0 ? (long)tails[lo - 1] : -1;
            if (lo == tails.size()) tails.push_back(t); else tails[lo] = t;
        }
        for (long t = tails.empty() ? -1 : (long)tails.back(); t >= 0; t = prev[t])
            in_lis[(std::size_t)t] = 1;
    }

    // 4) Walk targets RIGHT-TO-LEFT, placing each displaced node immediately
    //    BEFORE its successor (target ti+1, already finalised this pass), or at
    //    the end for the last target. Anchoring by the successor — not an
    //    absolute index — keeps the op sequence correct regardless of how many
    //    elements to the left are still unplaced. LIS nodes are already in the
    //    right relative order and are skipped, so ONLY genuinely-displaced nodes
    //    (and fresh inserts) touch the model `cur` — which is why the common
    //    few-row reorder is cheap even though `cur` is a plain vector.
    for (std::size_t ti = nb; ti-- > 0; ) {
        const std::string& k = b.kids[ti]->key;
        bool fresh = new_to_old[ti] < 0;
        if (!fresh && in_lis[ti]) continue;            // already correctly placed

        // The anchor is the successor's CURRENT index in `cur`, or end-of-list.
        std::size_t anchor = cur.size();
        if (ti + 1 < nb) {
            const std::string& sk = b.kids[ti + 1]->key;
            for (std::size_t i = 0; i < cur.size(); ++i) if (cur[i] == sk) { anchor = i; break; }
        }

        if (fresh) {
            out.push_back({Op::insert, path, std::to_string(anchor), {}, b.kids[ti], -1, (int)anchor});
            cur.insert(cur.begin() + anchor, k);
        } else {
            std::size_t from = 0;
            for (std::size_t i = 0; i < cur.size(); ++i) if (cur[i] == k) { from = i; break; }
            // Erasing `from` first shifts the anchor left by one when the anchor
            // sat to its right.
            std::size_t to = (from < anchor) ? anchor - 1 : anchor;
            if (from != to) {
                out.push_back({Op::move, path, {}, {}, {}, (int)from, (int)to});
                cur.erase(cur.begin() + from);
                cur.insert(cur.begin() + to, k);
            }
        }
    }
}

// ── the per-Kind SCHEMA: the fixed list of BODY channels a Kind owns ────────
// (the shell channel is universal and handled separately). `body_schema` is a
// TOTAL map over Kind: the switch has no default, so the compiler rejects the
// file if a new Kind is added without giving it a schema. `box`/`form` return
// an empty body schema — their body is child nodes, reconciled structurally.
struct Schema { const Channel* chans; std::size_t n;
    template <std::size_t N> constexpr Schema(const Channel (&a)[N]) : chans(a), n(N) {}
    constexpr Schema() : chans(nullptr), n(0) {} };

inline bool points_differ(const Node& a, const Node& b){ return a.points!=b.points || a.closed!=b.closed; }
inline bool options_differ(const Node& a, const Node& b){ return a.options!=b.options; }

inline Schema body_schema(Kind k){
    // one static array per Kind's body; addresses are stable (function-local
    // statics), so returning a view is safe.
    switch (k) {
        case Kind::text: {
            static constexpr Channel c[]{ text_channel<&Node::text> }; return c; }
        case Kind::button: {
            static constexpr Channel c[]{ text_channel<&Node::text> }; return c; }
        case Kind::markup: {
            static constexpr Channel c[]{ inner_channel<&Node::text> }; return c; }
        case Kind::image: {
            static constexpr Channel c[]{ prop_channel<&Node::src, "src"> }; return c; }
        case Kind::video: case Kind::audio: {
            static constexpr Channel c[]{ prop_channel<&Node::src, "src"> }; return c; }
        case Kind::input: case Kind::textarea: {
            static constexpr Channel c[]{ prop_channel<&Node::text, "value"> }; return c; }
        case Kind::checkbox: case Kind::radio: {
            static constexpr Channel c[]{ bool_prop_channel<&Node::checked, "checked"> }; return c; }
        case Kind::select: {
            // option-set change -> replace (structural body); else value prop.
            static const Channel c[]{ replace_channel(options_differ),
                                      prop_channel<&Node::selected, "value"> }; return c; }
        case Kind::path: {
            static const Channel c[]{ replace_channel(points_differ) }; return c; }
        case Kind::box: case Kind::form:
            return {};   // body is child nodes; reconciled below
    }
    return {};   // unreachable: switch is total over Kind
}

inline void diff_node(const Node& a, const Node& b, const std::string& path,
                      const NodeRef& bref, Patch& out) {
    if (a.hash == b.hash) return;                       // fast path: unchanged subtree

    // Identity/kind change -> the two nodes are different elements; swapping the
    // whole subtree is the only sound move (no cross-identity DOM reuse). This
    // matches React/Elm keyed reconciliation.
    if (a.kind != b.kind) { out.push_back({Op::replace, path, {}, {}, bref}); return; }
    if (!a.key.empty() && !b.key.empty() && a.key != b.key) {
        out.push_back({Op::replace, path, {}, {}, bref}); return;
    }

    // THE FOLD. Shell channel first (universal), then the Kind's body channels.
    // Each channel emits at most one op, and the channels are orthogonal (shell
    // = attrs, body = text/inner/prop/replace), so a node that changed two
    // channels emits two non-overlapping ops — no duplication, no lost content.
    if (shell_channel.differs(a, b)) shell_channel.emit(b, path, bref, out);

    Schema s = body_schema(a.kind);
    if (s.chans) {
        for (std::size_t i = 0; i < s.n; ++i) {
            if (!s.chans[i].differs(a, b)) continue;
            std::size_t before = out.size();
            s.chans[i].emit(b, path, bref, out);
            // A replace reships the whole element, subsuming every later body
            // channel of this node — stop once one fires. (Only the select
            // schema has >1 channel where this matters.)
            if (!out.empty() && out[before].op == Op::replace) break;
        }
    } else if (a.kind == Kind::box || a.kind == Kind::form) {
        if (fully_keyed(a.kids, b.kids)) diff_children_keyed(a, b, path, out);
        else                            diff_children_positional(a, b, path, out);
    }
}
} // namespace detail

/// Produce the minimal delta turning `prev` into `next`.
inline Patch diff(const NodeRef& prev, const NodeRef& next) {
    Patch out; detail::diff_node(*prev, *next, "", next, out); return out;
}

} // namespace waya::surface
