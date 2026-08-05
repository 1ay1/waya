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
template <typename P>
[[nodiscard]] bool verify_roundtrip(const std::vector<typename P::Msg>& msgs) {
    auto [model, _] = program_init<P>();
    vdom::VNode prev = view_vnode<P>(model);

    for (const auto& msg : msgs) {
        auto [m2, cmd] = P::update(std::move(model), msg);
        model = std::move(m2); (void)cmd;

        vdom::VNode next = view_vnode<P>(model);
        vdom::Patch patch = vdom::diff(prev, next);

        // Apply the patch to the client's view of the world (== prev) and check
        // it becomes exactly `next`.
        vdom::VNode client = prev;
        vdom::apply(client, patch);
        if (vdom::vnode_to_html(client) != vdom::vnode_to_html(next))
            return false;

        prev = std::move(next);
    }
    return true;
}

} // namespace waya
