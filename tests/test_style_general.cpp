/// tests/test_style_general.cpp — the "general enough like maya" guarantee:
/// ANY CSS is one clean pipe (prop / var_ / on / at), still interned & diffable.

#include <waya/waya.hpp>

#include <iostream>
#include <string>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)
static bool has(const std::string& h, std::string_view n) { return h.find(n) != std::string::npos; }
static int count(const std::string& h, std::string_view n) {
    int c = 0; for (std::size_t p = 0; (p = h.find(n, p)) != std::string::npos; ++p) ++c; return c;
}

int main() {
    // ── prop<>: arbitrary CSS property, emitted verbatim ────────────────────
    {
        auto r = waya::render::render(
            div_(text("x")) | prop<"backdrop-filter", "blur(8px)">
                            | prop<"clip-path", "circle(40%)">);
        CHECK(has(r.css, "backdrop-filter:blur(8px)"));
        CHECK(has(r.css, "clip-path:circle(40%)"));
    }

    // ── CSS custom properties + var() usage ─────────────────────────────────
    {
        auto r = waya::render::render(
            div_(text("x")) | var_<"--brand", "#3b82f6"> | prop<"color", "var(--brand)">);
        CHECK(has(r.css, "--brand:#3b82f6"));
        CHECK(has(r.css, "color:var(--brand)"));
    }

    // ── on<Hover>: pseudo-class as a value → .wa-x:hover{...} ────────────────
    {
        auto r = waya::render::render(
            button_(text("Go")) | bg(0x3b82f6) | on<Hover>(bg(0x2563eb)));
        CHECK(has(r.css, ":hover{"));
        CHECK(has(r.css, "background:#2563eb"));
    }

    // ── on<Focus> can carry ANY property, not just the named ones ───────────
    {
        auto r = waya::render::render(
            button_(text("Go")) | on<Focus>(prop<"outline", "2px solid #93c5fd">));
        CHECK(has(r.css, ":focus{outline:2px solid #93c5fd;}"));
    }

    // ── at<Md>: media query as a value → @media (...){.wa-x{...}} ────────────
    {
        auto r = waya::render::render(
            div_(text("x")) | pad(8_px) | at<Md>(pad(16_px)));
        CHECK(has(r.css, "@media (min-width:768px){.wa-"));
        CHECK(has(r.css, "padding:16px"));
    }

    // ── grid via prop: a layout the named tokens don't cover ────────────────
    {
        auto r = waya::render::render(
            div_(text("x")) | gridbox | prop<"grid-template-columns", "repeat(3, 1fr)">);
        CHECK(has(r.css, "display:grid"));
        CHECK(has(r.css, "grid-template-columns:repeat(3, 1fr)"));
    }

    // ── interning STILL holds with the general channel: two identical buttons
    //    (same base + same hover) share ONE class and ONE rule set ───────────
    {
        auto mkbtn = [] {
            return button_(text("x")) | bg(0x3b82f6) | rounded(6_px) | on<Hover>(bg(0x2563eb));
        };
        auto r = waya::render::render(div_(mkbtn(), mkbtn()));
        CHECK(count(r.html, "<button") == 2);
        // one base rule + one :hover rule for the shared class = ".wa-" twice,
        // but only ONE distinct class name.
        // distinct class check: both buttons carry the same class token.
        auto first = r.html.find("class=\"");
        auto cls = r.html.substr(first + 7, r.html.find('"', first + 7) - (first + 7));
        CHECK(count(r.html, cls) == 2);                 // same class on both
    }

    // ── prop_dyn: runtime value when it isn't known at compile time ─────────
    {
        int w = 42;
        auto r = waya::render::render(
            div_(text("x")) | prop_dyn("width", std::to_string(w) + "px"));
        CHECK(has(r.css, "width:42px"));
    }

    std::cout << "test_style_general: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
