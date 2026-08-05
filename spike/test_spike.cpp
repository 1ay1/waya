// waya Phase 0 spike — validation of the type-state HTML DSL.
//
// Build:  g++ -std=c++26 -O2 spike/test_spike.cpp -o /tmp/waya_spike && /tmp/waya_spike
//
// The positive cases below compile AND are checked at compile time via
// static_assert on constexpr-rendered output. The negative cases are compiled
// separately by spike/run_spike.sh, which asserts that each one FAILS.

#include "waya_dsl.hpp"

#include <cassert>
#include <iostream>

using namespace waya;
using namespace waya::dsl;

// ── 1. Compile-time rendering ───────────────────────────────────────────────
// A whole document rendered during constant evaluation. Nothing at runtime.

constexpr auto page = html_(
    head_(title_(text("Hi"))),
    body_(p_(text("hello")))
);

static_assert(render_document(page) ==
    "<!DOCTYPE html><html><head><title>Hi</title></head>"
    "<body><p>hello</p></body></html>");

// ── 2. Attributes via the pipe operator ─────────────────────────────────────

constexpr auto styled = div_(
    h1_(text("Dashboard")) | cls<"title">,
    p_(text("body copy")) | cls<"lead">
) | cls<"page"> | id_<"root">;

static_assert(render(styled) ==
    "<div id=\"root\" class=\"page\">"
    "<h1 class=\"title\">Dashboard</h1>"
    "<p class=\"lead\">body copy</p>"
    "</div>");

// ── 3. href is type-gated to <a> ────────────────────────────────────────────

constexpr auto link = a_(text("click")) | href<"/next">;
static_assert(render(link) == "<a href=\"/next\">click</a>");

// ── 4. Tables: strict parent/child structure ────────────────────────────────

constexpr auto tbl = table_(
    thead_(tr_(th_(text("Name")), th_(text("Status")))),
    tbody_(tr_(td_(text("api")), td_(text("up"))))
) | cls<"grid">;

static_assert(render(tbl) ==
    "<table class=\"grid\">"
    "<thead><tr><th>Name</th><th>Status</th></tr></thead>"
    "<tbody><tr><td>api</td><td>up</td></tr></tbody>"
    "</table>");

// ── 5. Void elements self-close and take no children ────────────────────────

constexpr auto voids = div_(br_(), hr_(), img_());
static_assert(render(voids) == "<div><br><hr><img></div>");

// ── 6. Escaping is automatic and unskippable ────────────────────────────────

static_assert(render(p_(text("<script>alert('xss')</script>"))) ==
    "<p>&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;</p>");

// ── 7. Lists ────────────────────────────────────────────────────────────────

constexpr auto list = ul_(li_(text("one")), li_(text("two")));
static_assert(render(list) == "<ul><li>one</li><li>two</li></ul>");

// ── 8. Deep nesting still folds at compile time ─────────────────────────────

constexpr auto deep = div_(div_(div_(div_(div_(p_(text("deep")))))));
static_assert(render(deep) ==
    "<div><div><div><div><div><p>deep</p></div></div></div></div></div>");

// ── 9. The content model is genuinely consulted ─────────────────────────────
// These concept checks are the machine-readable form of the guarantee.

static_assert( PermittedChild<Tag::p,     TextNode>);           // phrasing in <p>
static_assert(!PermittedChild<Tag::p,     decltype(div_())>);   // <div> in <p>  ✗
static_assert( PermittedChild<Tag::div,   decltype(p_())>);     // <p> in <div>
static_assert( PermittedChild<Tag::tr,    decltype(td_())>);    // <td> in <tr>
static_assert(!PermittedChild<Tag::div,   decltype(td_())>);    // <td> in <div> ✗
static_assert(!PermittedChild<Tag::ul,    decltype(div_())>);   // <div> in <ul> ✗
static_assert( PermittedChild<Tag::ul,    decltype(li_())>);    // <li> in <ul>
static_assert(!PermittedChild<Tag::head,  decltype(p_())>);     // <p> in <head> ✗
static_assert( PermittedChild<Tag::span,  TextNode>);
static_assert(!PermittedChild<Tag::title, decltype(div_())>);   // <div> in <title> ✗

// The href gate.
static_assert( AcceptsHref<Tag::a>);
static_assert(!AcceptsHref<Tag::span>);
static_assert(!AcceptsHref<Tag::div>);

// ── 10. Runtime rendering path works too ────────────────────────────────────

int main() {
    // Runtime data flows through the same DSL.
    std::string user = "Ada <admin>";
    auto greeting = div_(
        h1_(text("Welcome")),
        p_(text(user))
    ) | cls<"greet">;

    const std::string out = render(greeting);
    std::cout << out << "\n";
    assert(out ==
        "<div class=\"greet\"><h1>Welcome</h1><p>Ada &lt;admin&gt;</p></div>");

    std::cout << render_document(page) << "\n";
    std::cout << render(tbl) << "\n";

    std::cout << "\nAll spike assertions passed.\n";
    std::cout << "  - documents render at COMPILE TIME (static_assert)\n";
    std::cout << "  - the HTML5 content model is enforced by concepts\n";
    std::cout << "  - escaping cannot be skipped\n";
    return 0;
}
