/// tests/test_render.cpp — SSR + styling correctness (Phase 1).
/// A tiny header-free harness: each CHECK is counted; non-zero exit on failure.

#include <waya/waya.hpp>

#include <iostream>
#include <string>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do {                                                     \
    if (cond) { ++g_pass; }                                                  \
    else { ++g_fail; std::cerr << "FAIL " << __FILE__ << ':' << __LINE__     \
                               << "  " #cond "\n"; }                         \
} while (0)
#define CHECK_EQ(a, b) do {                                                  \
    auto _a = (a); auto _b = (b);                                            \
    if (_a == _b) { ++g_pass; }                                              \
    else { ++g_fail; std::cerr << "FAIL " << __FILE__ << ':' << __LINE__     \
        << "\n  expected: " << _b << "\n  actual:   " << _a << "\n"; }       \
} while (0)

static bool contains(const std::string& h, std::string_view n) {
    return h.find(n) != std::string::npos;
}

int main() {
    // ── constexpr document folds to the right bytes (no styling) ────────────
    {
        auto page = html_(head_(title_(text("Hi"))), body_(p_(text("hello"))));
        auto doc  = waya::render::render_document(page);
        CHECK_EQ(doc,
            std::string("<!DOCTYPE html><html><head><title>Hi</title></head>"
                        "<body><p>hello</p></body></html>"));
    }

    // ── escaping is automatic ───────────────────────────────────────────────
    {
        auto r = waya::render::render(p_(text("<script>alert('x')</script>")));
        CHECK_EQ(r.html, std::string("<p>&lt;script&gt;alert('x')&lt;/script&gt;</p>"));
    }

    // ── attributes ──────────────────────────────────────────────────────────
    {
        auto r = waya::render::render(a_(text("go")) | href<"/next"> | cls<"link">);
        CHECK(contains(r.html, "href=\"/next\""));
        CHECK(contains(r.html, "class=\"link\""));
    }

    // ── styling: style becomes a class, not an inline style ─────────────────
    {
        auto r = waya::render::render(div_(text("x")) | pad(16_px) | bg(0x0f172a));
        CHECK(!contains(r.html, "style="));            // no inline styles
        CHECK(contains(r.html, "class=\"wa-"));         // an interned class
        CHECK(contains(r.css, "padding:16px"));
        CHECK(contains(r.css, "background:#0f172a"));
    }

    // ── interning: identical styles collapse to ONE rule ────────────────────
    {
        auto r = waya::render::render(
            div_(
                span_(text("a")) | pad(8_px) | bg(0x111111),
                span_(text("b")) | pad(8_px) | bg(0x111111)   // identical
            )
        );
        // count occurrences of ".wa-" in css == number of unique rules
        int rules = 0;
        for (std::size_t p = 0; (p = r.css.find(".wa-", p)) != std::string::npos; ++p) ++rules;
        CHECK_EQ(rules, 1);   // the two spans share one rule
    }

    // ── user class + style class combine ────────────────────────────────────
    {
        auto r = waya::render::render(div_(text("x")) | cls<"card"> | pad(8_px));
        CHECK(contains(r.html, "class=\"card wa-"));
    }

    // ── full-page style injection into <head> ───────────────────────────────
    {
        auto page = html_(head_(title_(text("T"))),
                          body_(div_(text("x")) | pad(4_px)));
        auto doc = waya::render::render_document(page);
        CHECK(contains(doc, "<head><style>.wa-"));
        CHECK(contains(doc, "</style><title>"));
    }

    std::cout << "test_render: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
