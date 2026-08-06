// tests/test_safety.cpp — the "impossible HTML doesn't ship" + escaping story.
// Structural validation catches WHATWG content-model bugs; the DOM backend
// escapes every attribute context and sanitises dangerous URL schemes.
#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/validate.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
static std::string html_of(NodeRef n) { return DomBackend{}.render(*n).html; }
static bool has_rule(NodeRef n, const std::string& rule) {
    for (auto& v : check(n)) if (v.rule == rule) return true; return false;
}

int main() {
    // ── structural validation ────────────────────────────────────────────────
    // a well-formed tree has no violations
    check(verify(col(text("ok"), row(text("a"), text("b")))), "clean tree verifies");

    // unnamed control inside a form → form-control-name
    { auto f = form(input("")) ;
      check(has_rule(f, "form-control-name"), "unnamed control in form flagged"); }
    // named control inside a form is fine
    { auto f = form(input("") | attr("name","email"));
      check(!has_rule(f, "form-control-name"), "named control in form ok"); }

    // interactive nested in interactive → nested-interactive
    { auto bad = box(text("outer")) | tap(1);
      bad->kids.push_back(box(text("inner")) | tap(2));
      check(has_rule(bad, "nested-interactive"), "nested tap targets flagged"); }

    // void element with children → void-element
    { auto img = image("/x.png") | alt("x");
      img->kids.push_back(text("nope"));
      check(has_rule(img, "void-element"), "image with children flagged"); }

    // image without alt → img-alt ; with alt → clean
    check(has_rule(image("/x.png"), "img-alt"), "image without alt flagged");
    check(!has_rule(image("/x.png") | alt("logo"), "img-alt"), "image with alt ok");

    // explain() produces a readable report
    check(!explain(image("/x.png")).empty(), "explain reports violations");
    check(explain(text("fine")).empty(), "explain empty for clean tree");

    // ── attribute-context escaping ───────────────────────────────────────────
    // a double-quote in an attribute value must be entity-escaped, not raw
    { auto n = text("x") | attr("title", "a\"b");
      auto h = html_of(n);
      check(has(h, "&quot;"), "quote in attr is escaped");
      check(!has(h, "title=\"a\"b\""), "quote does not break out of attr"); }

    // < > & in attribute value escaped
    { auto h = html_of(text("x") | attr("data-x", "<b>&"));
      check(has(h, "&lt;b&gt;&amp;"), "angle/amp in attr escaped"); }

    // image src with a quote can't break out
    { auto h = html_of(image("a\"onerror=alert(1)") | alt(""));
      check(has(h, "&quot;"), "img src quote escaped"); }

    // ── URL sanitisation ─────────────────────────────────────────────────────
    check(safe_url("javascript:alert(1)") == "#", "javascript: url neutralised");
    check(safe_url("  JavaScript:alert(1)") == "#", "js: url with whitespace/case neutralised");
    check(safe_url("data:text/html,<script>") == "#", "data: url neutralised");
    check(safe_url("https://ok.dev/x") == "https://ok.dev/x", "https url preserved");
    check(safe_url("/relative/path") == "/relative/path", "relative url preserved");

    { auto h = html_of(link_to("click", "javascript:evil()"));
      check(has(h, "href=\"#\""), "link_to sanitises dangerous href");
      check(has(h, "<a "), "link_to renders an anchor"); }
    { auto h = html_of(text("go") | href("https://x.dev"));
      check(has(h, "href=\"https://x.dev\"") && has(h, "<a "), "href() sets a safe anchor"); }

    std::cout << "test_safety: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
