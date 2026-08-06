// tests/test_assets.cpp — the document-level asset registry and first-class grid.
// These are the two additions that make the core CLOSED under "build anything":
// userland can inject @keyframes / fonts / :root tokens / global CSS / <head>
// markup, and express real 2-D layouts without dropping to raw css().
#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/layout.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) {
    if (c) ++pass;
    else { ++fail; std::cerr << "FAIL: " << msg << "\n"; }
}
static bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }

int main() {
    // ── Assets: keyframes ──────────────────────────────────────────────────
    assets().clear();
    assets().keyframes("t-wobble", "0%{transform:none}100%{transform:rotate(6deg)}");
    check(has(assets().style_css(), "@keyframes t-wobble{0%{transform:none}100%{transform:rotate(6deg)}}"),
          "keyframes emitted");

    // dedup by name — second registration ignored
    assets().keyframes("t-wobble", "IGNORED");
    check(!has(assets().style_css(), "IGNORED"), "keyframes deduped by name");

    // ── Assets: :root tokens (last write wins) ─────────────────────────────
    assets().root_var("--brand", "#111111");
    assets().root_var("--brand", "#6366f1");
    check(has(assets().style_css(), ":root{"), "root block emitted");
    check(has(assets().style_css(), "--brand:#6366f1"), "root_var last-write-wins");
    check(!has(assets().style_css(), "#111111"), "root_var override drops old value");

    // ── Assets: font_face infers format from extension ─────────────────────
    assets().clear();
    assets().font_face("Inter", "/fonts/Inter.woff2");
    check(has(assets().style_css(), "@font-face"), "font_face emitted");
    check(has(assets().style_css(), "font-family:'Inter'"), "font_face family");
    check(has(assets().style_css(), "format('woff2')"), "font_face infers woff2");
    check(has(assets().style_css(), "font-display:swap"), "font_face uses swap");

    // ── Assets: global css + head, deduped by exact text ───────────────────
    assets().clear();
    assets().css("::selection{background:#6366f1}");
    assets().css("::selection{background:#6366f1}");  // dup
    size_t first = assets().style_css().find("::selection");
    check(first != std::string::npos, "global css emitted");
    check(assets().style_css().find("::selection", first + 1) == std::string::npos, "global css deduped");

    assets().head("<link rel=\"icon\" href=\"/f.svg\">");
    assets().head("<link rel=\"icon\" href=\"/f.svg\">");  // dup
    check(has(assets().head_html(), "<link rel=\"icon\""), "head markup emitted");
    check(assets().head_html().find("<link", assets().head_html().find("<link") + 1) == std::string::npos,
          "head deduped");

    // ── custom_animation registers AND applies in one call ─────────────────
    assets().clear();
    auto n = box() | custom_animation("t-spin2", "to{transform:rotate(360deg)}", 500);
    check(has(assets().style_css(), "@keyframes t-spin2"), "custom_animation registers keyframe");
    check(has(css_of(n), "animation:t-spin2 500ms"), "custom_animation applies animation");

    // ── First-class grid ───────────────────────────────────────────────────
    check(has(css_of(grid()), "display:grid"), "grid() sets display:grid");
    check(has(css_of(box() | grid_cols(3)), "grid-template-columns:repeat(3,minmax(0,1fr))"), "grid_cols(n)");
    check(has(css_of(box() | grid_cols("1fr 2fr")), "grid-template-columns:1fr 2fr"), "grid_cols(str)");
    check(has(css_of(box() | grid_rows("auto 1fr")), "grid-template-rows:auto 1fr"), "grid_rows(str)");
    check(has(css_of(box() | grid_cols(2)), "display:grid"), "grid_cols implies display:grid");
    check(has(css_of(box() | grid_areas("'a b' 'a c'")), "grid-template-areas:'a b' 'a c'"), "grid_areas");
    check(has(css_of(box() | auto_grid(200)), "auto-fit"), "auto_grid uses auto-fit");
    check(has(css_of(box() | col_span(2)), "grid-column:span 2"), "col_span");
    check(has(css_of(box() | row_span(3)), "grid-row:span 3"), "row_span");
    check(has(css_of(box() | area("nav")), "grid-area:nav"), "area");

    std::cout << "test_assets: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
