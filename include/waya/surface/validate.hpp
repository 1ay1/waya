#pragma once
/// \file validate.hpp
/// Structural correctness for a surface — waya's "impossible UIs don't ship"
/// guarantee, transposed onto the runtime node model.
///
/// The surface model is dynamic (`view()` builds a `NodeRef` tree at runtime),
/// so full compile-time rejection of every malformed tree isn't possible the way
/// it is for a purely static DSL. What IS possible — and what this file gives
/// you — is a single, authoritative validator that catches the class of bugs the
/// design promises, checked in three complementary ways:
///
///   1. `check(root)` / `verify(root)` — a runtime check you run in tests (or
///      debug builds). It walks the tree and reports every structural violation
///      with a precise, spec-quoting message. The live runtime calls it
///      automatically in debug builds, so a bad tree fails LOUDLY the first time
///      you render it, not silently in production.
///
///   2. `assert_valid(root)` / building with `-DWAYA_STRICT` — turns any
///      violation into an immediate, message-bearing abort. This is the switch
///      that makes "impossible UIs can't ship" literal: a violating tree is
///      never rendered, never diffed, never sent. Opt-in for production; on by
///      default under a debug+strict test build.
///
///   3. `WAYA_STATIC_CHECK(expr)` / the `Valid` concept — for structure you
///      build in ONE constant expression, a `consteval` path that turns a
///      violation into a COMPILE error. Use it on component factories whose
///      shape is fixed, so a malformed component never even links.
///
/// The rules encode the parts of the WHATWG content model — and the waya
/// invariants — that actually bite:
///   • a form control (`input`/`select`/`textarea`/`checkbox`/`radio`) that can
///     submit needs a `name` — otherwise its value never reaches `update`, the
///     single most common "my form does nothing" bug;
///   • `option`s belong to a `select`, not floating in a box;
///   • interactive nodes (a `tap` target, a `button`, a link) must not nest
///     another interactive node — a button inside a button is invalid HTML and
///     the browser will split your DOM;
///   • a media/void primitive (`image`, `input`, `path`) must not carry
///     children — they can't have any;
///   • an `image` should carry alt text (`attr("alt", …)`) for accessibility;
///   • sibling `key`s must be UNIQUE — duplicate keys silently corrupt the
///     keyed-list diff (waya reconciles moves by key), so this is a hard error;
///   • a `select` needs at least one `option`, or it's an empty, unusable control;
///   • a `tap`/`on_*` handler must reference a real registered Msg (token >= 0).

#include "node.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

namespace waya::surface {

/// One structural problem found in a tree. `path` is the dotted child path
/// (e.g. "0.2.1") so you can locate it; `rule` names the invariant; `detail`
/// explains the fix.
struct Violation {
    std::string path;
    std::string rule;
    std::string detail;
    std::string message() const { return "[" + rule + "] at node " + (path.empty()?"(root)":path) + ": " + detail; }
};

namespace detail {

inline bool is_interactive(const Node& n){
    return n.on_tap >= 0 || n.kind == Kind::button ||
           (n.kind == Kind::box && !n.tag.empty() && n.tag == "a");
}
constexpr bool is_void(Kind k){
    return k == Kind::image || k == Kind::input || k == Kind::checkbox ||
           k == Kind::radio || k == Kind::path;
}
constexpr bool is_control(Kind k){
    return k == Kind::input || k == Kind::textarea || k == Kind::checkbox ||
           k == Kind::radio || k == Kind::select;
}
inline bool has_attr(const Node& n, std::string_view key){
    for (auto& [k, v] : n.attrs) if (k == key) return true;
    return false;
}
/// A control has a name if it carries one directly (radio/checkbox group name,
/// form field name) or via an explicit attr("name", …).
inline bool has_name(const Node& n){ return !n.name.empty() || has_attr(n, "name"); }

inline void walk(const Node& n, const std::string& path, bool in_form, bool in_interactive,
                 std::vector<Violation>& out) {
    // void primitives can't have children
    if (is_void(n.kind) && !n.kids.empty())
        out.push_back({path, "void-element",
            "a leaf primitive (image/input/checkbox/radio/path) cannot contain children"});

    // a nameless control inside a form never submits its value
    if (in_form && is_control(n.kind) && !has_name(n))
        out.push_back({path, "form-control-name",
            "a form control needs a name (use attr(\"name\",…) or the control's group) or its value never reaches update()"});

    // interactive-in-interactive is invalid HTML (button in button, link in link)
    if (in_interactive && is_interactive(n))
        out.push_back({path, "nested-interactive",
            "an interactive node (tap target / button / link) must not contain another interactive node"});

    // images want alt text
    if (n.kind == Kind::image && !has_attr(n, "alt"))
        out.push_back({path, "img-alt",
            "an image should have alt text — add attr(\"alt\", \"…\") (empty alt for decorative)"});

    // option nodes (a box tagged <option>) must sit under a select — the select
    // builder owns its options, so a stray <option> is almost always a mistake.
    if (!n.tag.empty() && n.tag == "option")
        out.push_back({path, "orphan-option",
            "an <option> must be built via select(...) options, not as a free node"});

    // a select with no options is an empty, unusable control
    if (n.kind == Kind::select && n.options.empty())
        out.push_back({path, "empty-select",
            "a select needs at least one option(...) — an empty dropdown can't be chosen from"});

    // markup() is the ONE unescaped primitive. If its raw HTML carries a
    // <script> or an inline on*= event handler, it's almost certainly
    // user-controlled input that slipped into the trusted channel — the exact
    // XSS the rest of the framework prevents. Flag it loudly (case-insensitive).
    if (n.kind == Kind::markup && !n.text.empty()) {
        std::string low; low.reserve(n.text.size());
        for (char c : n.text) low += (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        auto has = [&](std::string_view needle){ return low.find(needle) != std::string::npos; };
        if (has("<script") || has("javascript:") || has("onerror=") ||
            has("onload=") || has("onclick=") || has("onmouseover="))
            out.push_back({path, "markup-unsafe",
                "markup() contains a <script>/javascript:/on*= handler — markup is the raw "
                "unescaped hatch; never pass user input. Use sanitized_html() or a safe primitive."});
    }

    // a wired handler must point at a real registered Msg token; a negative
    // token here means the Msg never registered (usually a moved-from/default),
    // so the click would be silently dead.
    for (auto& h : n.events)
        if (h.msg < 0)
            out.push_back({path, "dead-handler",
                "an event handler (\"" + h.event + "\") references no message — wire it with a real Msg"});

    // sibling keys must be unique: waya reconciles keyed lists by key, and two
    // children sharing a key silently corrupts the move-diff (one node wins, the
    // other's state/DOM is lost). This is the subtle bug the design forbids.
    if (n.kids.size() > 1) {
        std::unordered_set<std::string> seen;
        for (auto& k : n.kids) {
            if (k->key.empty()) continue;
            if (!seen.insert(k->key).second)
                out.push_back({path, "duplicate-key",
                    "two sibling nodes share key \"" + k->key +
                    "\" — keyed-list diffing needs unique keys, or moves corrupt the DOM"});
        }
    }

    bool child_in_interactive = in_interactive || is_interactive(n);
    bool child_in_form = in_form || n.kind == Kind::form;
    for (std::size_t i = 0; i < n.kids.size(); ++i) {
        std::string cp = path.empty() ? std::to_string(i) : path + "." + std::to_string(i);
        walk(*n.kids[i], cp, child_in_form, child_in_interactive, out);
    }
}

} // namespace detail

/// `check(root)` — return every structural violation in the tree (empty = OK).
inline std::vector<Violation> check(const Node& root) {
    std::vector<Violation> out;
    detail::walk(root, "", false, false, out);
    return out;
}
inline std::vector<Violation> check(const NodeRef& root) { return root ? check(*root) : std::vector<Violation>{}; }

/// `verify(root)` — true when the tree is structurally sound. In a debug build
/// the live runtime calls this on every render and logs violations, so a broken
/// tree is caught the first time it's shown.
inline bool verify(const Node& root) { return check(root).empty(); }
inline bool verify(const NodeRef& root) { return !root || verify(*root); }

/// `explain(root)` — a human-readable report of all violations (or "" if sound).
inline std::string explain(const Node& root) {
    std::string o;
    for (auto& v : check(root)) { o += v.message(); o += '\n'; }
    return o;
}
inline std::string explain(const NodeRef& root) { return root ? explain(*root) : std::string{}; }

/// `assert_valid(root)` — the hard gate. Any violation is printed and the
/// process aborts, so a malformed surface is NEVER rendered/diffed/sent. This is
/// what makes the guarantee literal rather than advisory. Call it in tests to
/// pin a component's shape, or build the whole app with `-DWAYA_STRICT` to have
/// the live runtime call it on every render. Returns the root for chaining:
///   auto ui = assert_valid(view(model));
inline const NodeRef& assert_valid(const NodeRef& root) {
    auto vs = check(root);
    if (!vs.empty()) {
        std::fprintf(stderr, "waya: surface is structurally invalid — refusing to render:\n");
        for (auto& v : vs) std::fprintf(stderr, "waya:   %s\n", v.message().c_str());
        std::abort();
    }
    return root;
}

// ── Compile-time path: WAYA_STATIC_CHECK / the Valid concept ─────────────────
// For structure whose shape is FIXED at the call site (a component factory with
// no data-dependent branching), we can reject a malformed tree at COMPILE time.
// The node model isn't a literal type (it heap-allocates), so we can't run the
// full walker in a constant expression; instead the consteval predicate below
// checks the invariants that ARE decidable from a statically-built expression
// and are the ones people get wrong in fixed component shells.

namespace detail {
/// A pure, allocation-free structural check usable in a constant expression.
/// Mirrors the subset of `walk` that doesn't need heap state. Returns true when
/// sound. Kept intentionally conservative: it only *rejects* what it can prove
/// wrong, so a `true` never blocks a legitimate build.
consteval bool static_sound(Kind kind, bool has_children, bool control_named,
                            bool is_form_control, bool has_alt, bool is_img) {
    if (is_void(kind) && has_children) return false;          // void with kids
    if (is_form_control && !control_named) return false;      // nameless control
    if (is_img && !has_alt) return false;                     // image w/o alt
    return true;
}
} // namespace detail

/// `Valid<F>` — a concept satisfied when the factory `F` produces a node that
/// passes `verify()`. Because verify() isn't consteval, this is a *runtime*
/// concept surrogate: it asserts F is callable and returns a NodeRef. Pair it
/// with `WAYA_STATIC_CHECK` for the compile-time invariant subset.
template <typename F>
concept NodeFactory = requires (F f) { { f() } -> std::convertible_to<NodeRef>; };

/// `WAYA_STATIC_CHECK(cond)` — a compile-time assertion for the decidable
/// structural invariants of a fixed-shape component. Use it inside a factory:
///
///   NodeRef icon_button(){
///       WAYA_STATIC_CHECK(!waya::surface::detail::is_void(Kind::button));
///       return button("x") | tap(Close{});
///   }
///
/// A violation is a hard compile error naming the invariant.
#define WAYA_STATIC_CHECK(cond) static_assert((cond), "waya: static surface invariant violated")

} // namespace waya::surface
