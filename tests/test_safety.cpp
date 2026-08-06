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

    // ── hardened rules ───────────────────────────────────────────────────────
    // duplicate sibling keys corrupt the keyed-list diff
    { auto dup = col(text("a") | key("k1"), text("b") | key("k1"));
      check(has_rule(dup, "duplicate-key"), "duplicate sibling keys flagged"); }
    { auto uniq = col(text("a") | key("k1"), text("b") | key("k2"));
      check(!has_rule(uniq, "duplicate-key"), "unique keys ok"); }

    // a select with no options is unusable
    check(has_rule(select({}), "empty-select"), "empty select flagged");
    check(!has_rule(select({option("a")}), "empty-select"), "select with an option ok");

    // an event handler wired to no real message is dead
    { auto bad = box(text("x"));
      bad->events.push_back({"keydown", -1, "Enter"});
      check(has_rule(bad, "dead-handler"), "handler with no message flagged"); }

    // a stray <option> outside a select is a mistake
    { auto n = text("opt"); n->tag = "option";
      check(has_rule(n, "orphan-option"), "orphan option flagged"); }

    // assert_valid returns a sound tree unchanged (for chaining)
    { auto ok = col(text("fine"));
      check(assert_valid(ok) == ok, "assert_valid passes a sound tree through"); }

    // the compile-time invariant path exists and holds for a fixed shape
    WAYA_STATIC_CHECK(!waya::surface::detail::is_void(Kind::button));
    WAYA_STATIC_CHECK(waya::surface::detail::is_void(Kind::image));
    static_assert(NodeFactory<decltype([]{ return text("x"); })>,
                  "a lambda returning NodeRef is a NodeFactory");
    check(true, "static invariants compiled");

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

    // the raw attr() escape hatch must ALSO sanitise URL-bearing attributes,
    // or it becomes an XSS bypass around href()/link_to().
    { auto h = html_of(text("x") | attr("href", "javascript:evil()"));
      check(!has(h, "javascript:"), "attr(href, javascript:) is neutralised"); }
    { auto h = html_of(image("/ok.png") | alt("a") | attr("src", "javascript:evil()"));
      check(!has(h, "data-ev") && !has(h, "\"javascript:evil()\""), "attr(src, javascript:) neutralised"); }
    { auto h = html_of(box(text("x")) | attr("formaction", "javascript:evil()"));
      check(!has(h, "javascript:"), "attr(formaction, javascript:) neutralised"); }
    { auto h = html_of(box(text("x")) | attr("data-safe", "javascript:ok"));
      check(has(h, "data-safe=\"javascript:ok\""), "non-URL attr keeps its value verbatim"); }

    std::cout << "test_safety: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
