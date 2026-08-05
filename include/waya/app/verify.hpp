#pragma once
/// \file verify.hpp
/// Live-runtime soundness helpers. The whole promise of the live loop is:
///
///   the patch the server sends ALWAYS turns the client's tree into exactly the
///   tree the server just rendered.
///
/// This file makes that checkable for ANY Program with no browser: drive a Msg
/// sequence, and after each step assert `apply(prev, diff(prev,next)) == next`.
/// The live runtime uses the same `apply` to maintain its own `prev`, so server
/// and client stay in lockstep by construction. If they could ever drift, the
/// check here (and the runtime's debug self-check) fires.

#include "program.hpp"
#include "../render/vwalk.hpp"
#include "../render/diff.hpp"

#include <string>
#include <vector>

namespace waya {

/// Render a Program's current view to a VNode (no HTML, no server).
template <typename P>
[[nodiscard]] vdom::VNode view_vnode(const typename P::Model& m) {
    style::StyleSheet sheet;
    return render::to_vnode(P::view(m), sheet);
}

/// Drive `P` from init through `msgs`, checking after every step that applying
/// the emitted patch to the previous tree reproduces the new tree EXACTLY —
/// i.e. the client (which runs the same apply) can never drift from the server.
/// Returns true iff every step round-trips. This is the executable form of
/// "if it compiles, the live UI is consistent."
///
/// IMPORTANT: this models the ACTUAL runtime loop — same memo cache reused
/// across frames, `prev` replaced by the fresh render each step — so a bug that
/// only shows up across a sequence (e.g. a stale-hash regression) is caught
/// here, not only in a live browser.
template <typename P>
[[nodiscard]] bool verify_roundtrip(const std::vector<typename P::Msg>& msgs) {
    auto [model, _] = program_init<P>();

    render::MemoCache memo;
    auto build = [&](const typename P::Model& m) {
        render::active_memo = &memo; render::memo_builds = 0;
        style::StyleSheet sheet;
        auto v = render::to_vnode(P::view(m), sheet);
        memo.rotate(); render::active_memo = nullptr;
        return v;
    };

    vdom::VNode prev = build(model);

    for (const auto& msg : msgs) {
        auto [m2, cmd] = P::update(std::move(model), msg);
        model = std::move(m2); (void)cmd;

        vdom::VNode next = build(model);
        vdom::Patch patch = vdom::diff(prev, next);

        // The client (starting from prev == its current DOM) applies the patch;
        // the result must be exactly `next`.
        vdom::VNode client = prev;
        vdom::apply(client, patch);
        if (vdom::vnode_to_html(client) != vdom::vnode_to_html(next))
            return false;

        prev = std::move(next);   // same baseline strategy as the live runtime
    }
    return true;
}

} // namespace waya
