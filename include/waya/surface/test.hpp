#pragma once
/// \file test.hpp
/// A first-class unit-testing harness for waya Programs. Because a waya app is
/// pure (init/update/view with no I/O), you can drive the ENTIRE thing in a
/// plain test — no browser, no socket, no runtime. This header makes that a
/// one-liner instead of hand-plumbing `detail::dispatch`.
///
///   #include <waya/surface/test.hpp>
///   using namespace waya::surface;
///
///   auto app = test::harness<Counter>();   // runs init(), holds the Model
///   app.send(Counter::Inc{});              // dispatch a Msg (pure update)
///   assert(app.model().n == 1);
///   assert(app.text_contains("1"));        // render view() and query it
///   assert(app.count(Kind::button) == 2);  // structural queries
///
/// It also records the Cmd every update returns, so you can assert on effects
/// WITHOUT running them (Cmd is ==-comparable):
///
///   app.send(Counter::Save{});
///   assert(app.last_cmd() == Cmd<Msg>::after(300, Counter::Saved{}));
///
/// Everything here is header-only and dependency-free; drop it in any test.

#include "node.hpp"
#include "program.hpp"
#include "effect.hpp"
#include "validate.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace waya::surface::test {

/// Walk a rendered tree, invoking `fn` on every node (pre-order).
inline void walk(const NodeRef& n, const std::function<void(const Node&)>& fn) {
    if (!n) return;
    fn(*n);
    for (auto& k : n->kids) walk(k, fn);
}

/// Collect the visible text of a tree (all `text` nodes, in order, space-joined).
inline std::string text_of(const NodeRef& n) {
    std::string out;
    walk(n, [&](const Node& nd) {
        if (nd.kind == Kind::text && !nd.text.empty()) {
            if (!out.empty()) out += ' ';
            out += nd.text;
        }
    });
    return out;
}

/// Count nodes of a given kind in a tree.
inline int count_kind(const NodeRef& n, Kind k) {
    int c = 0;
    walk(n, [&](const Node& nd) { if (nd.kind == k) ++c; });
    return c;
}

/// Find the first node carrying a given key (keyed lists / addressable nodes).
inline NodeRef find_key(const NodeRef& n, std::string_view key) {
    NodeRef hit;
    std::function<void(const NodeRef&)> go = [&](const NodeRef& cur) {
        if (hit || !cur) return;
        if (cur->key == key) { hit = cur; return; }
        for (auto& k : cur->kids) go(k);
    };
    go(n);
    return hit;
}

/// The harness: a live-runtime-free driver for one Program `P`. Holds the
/// current Model, replays init(), dispatches Msgs through the real update
/// (all four update shapes supported), and re-renders view() on demand.
template <typename P>
    requires SurfaceProgram<P>
class Harness {
public:
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    Harness() {
        auto [m, cmd] = detail::init_of<P, Model, Msg>();
        model_ = std::move(m);
        last_cmd_ = std::move(cmd);
    }

    /// Dispatch a Msg (no input value) through the real update; records its Cmd.
    Harness& send(Msg msg) { return send(std::move(msg), std::string{}); }

    /// Dispatch a Msg WITH an input value (as a wired input/change would send).
    Harness& send(Msg msg, std::string value) {
        auto [m, cmd] = detail::dispatch<P, Model, Msg>(std::move(model_), std::move(msg), value);
        model_ = std::move(m);
        last_cmd_ = std::move(cmd);
        return *this;
    }

    /// Dispatch a whole sequence in order (fluent scenario setup).
    Harness& send_all(std::vector<Msg> msgs) {
        for (auto& m : msgs) send(std::move(m));
        return *this;
    }

    /// The current model (mutable ref so tests can also poke it if needed).
    const Model& model() const { return model_; }
    Model&       model()       { return model_; }

    /// The Cmd the most recent update returned (Cmd::none() after init w/o cmd).
    const Cmd<Msg>& last_cmd() const { return last_cmd_; }

    /// Render the current model to a node tree (calls the app's real view()).
    NodeRef view() const { return P::view(model_); }

    // ── convenience queries over the freshly-rendered view ──────────────────
    std::string text() const { return text_of(view()); }
    bool text_contains(std::string_view needle) const {
        return text().find(needle) != std::string::npos;
    }
    int count(Kind k) const { return count_kind(view(), k); }
    NodeRef find_key(std::string_view key) const { return test::find_key(view(), key); }

    /// Assert the rendered view is structurally sound (WHATWG + waya rules).
    /// Returns "" when valid, or the violation report — assert on emptiness.
    std::string validate() const { return explain(view()); }
    bool valid() const { return verify(view()); }

private:
    Model model_{};
    Cmd<Msg> last_cmd_ = Cmd<Msg>::none();
};

/// `harness<P>()` — construct a Harness (runs init()). The function form reads
/// nicely at a call site: `auto app = test::harness<Counter>();`
template <typename P>
Harness<P> harness() { return Harness<P>{}; }

} // namespace waya::surface::test
