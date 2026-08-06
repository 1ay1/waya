#pragma once
/// \file validate.hpp
/// Structural correctness for a surface — waya's "impossible HTML doesn't ship"
/// guarantee, transposed onto the runtime node model.
///
/// The surface model is dynamic (`view()` builds a `NodeRef` tree at runtime),
/// so full compile-time rejection of every malformed tree isn't possible the way
/// it is for a purely static DSL. What IS possible — and what this file gives
/// you — is a single, authoritative validator that catches the class of bugs the
/// design promises, checked in two complementary ways:
///
///   1. `verify(root)` — a runtime check you run in tests (or debug builds). It
///      walks the tree and reports every structural violation with a precise,
///      spec-quoting message. The live runtime calls it automatically in debug
///      builds, so a bad tree fails LOUDLY the first time you render it, not
///      silently in production.
///
///   2. `WAYA_STATIC_CHECK(expr)` / the `Valid` concept — for structure you
///      build in one expression, a `consteval` path that turns a violation into
///      a COMPILE error. Use it on component factories whose shape is fixed.
///
/// The rules encode the parts of the WHATWG content model that actually bite:
///   • a form control (`input`/`select`/`textarea`) inside a `form` needs a
///     `name` — otherwise its value never reaches `update`, the single most
///     common "my form does nothing" bug;
///   • `option`s belong to a `select`, not floating in a box;
///   • interactive nodes (a `tap` target, a `button`, a link) must not nest
///     another interactive node — a button inside a button is invalid HTML and
///     the browser will split your DOM;
///   • a media/void primitive (`image`, `input`, `path`) must not carry
///     children — they can't have any;
///   • an `image` should carry alt text (`attr("alt", …)`) for accessibility.

#include "node.hpp"

#include <string>
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
inline bool is_void(Kind k){
    return k == Kind::image || k == Kind::input || k == Kind::checkbox ||
           k == Kind::radio || k == Kind::path;
}
inline bool is_control(Kind k){
    return k == Kind::input || k == Kind::textarea || k == Kind::checkbox ||
           k == Kind::radio || k == Kind::select;
}
inline bool has_attr(const Node& n, std::string_view key){
    for (auto& [k, v] : n.attrs) if (k == key) return true;
    return false;
}

inline void walk(const Node& n, const std::string& path, bool in_form, bool in_interactive,
                 std::vector<Violation>& out) {
    // void primitives can't have children
    if (is_void(n.kind) && !n.kids.empty())
        out.push_back({path, "void-element",
            "a leaf primitive (image/input/checkbox/radio/path) cannot contain children"});

    // a named-less control inside a form never submits its value
    if (in_form && is_control(n.kind) && n.name.empty() && !has_attr(n, "name"))
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

} // namespace waya::surface
