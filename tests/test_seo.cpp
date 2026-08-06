/// tests/test_seo.cpp — SEO: per-route meta head, semantic HTML elements,
/// JSON-LD structured data. Real HTML on first byte (SSR) is tested elsewhere;
/// here we check the <head> and element output that make it crawlable.

#include <waya/surface/meta.hpp>
#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }
static std::string html_of(const NodeRef& n){ return DomBackend{}.render(*n).html; }

int main() {
    // ═══ Meta → head tags ══════════════════════════════════════════════════════
    {
        Meta m{ .title="About · Acme", .description="Who we are.",
                .canonical="https://acme.dev/about", .image="https://acme.dev/og.png",
                .type="article", .site_name="Acme", .author="Ada" };
        auto head = detail::render_head(m, "fallback");
        CHECK(has(head, "name=\"description\" content=\"Who we are.\""));
        CHECK(has(head, "name=\"author\" content=\"Ada\""));
        CHECK(has(head, "rel=\"canonical\" href=\"https://acme.dev/about\""));
        // Open Graph
        CHECK(has(head, "property=\"og:title\" content=\"About · Acme\""));
        CHECK(has(head, "property=\"og:description\" content=\"Who we are.\""));
        CHECK(has(head, "property=\"og:image\" content=\"https://acme.dev/og.png\""));
        CHECK(has(head, "property=\"og:type\" content=\"article\""));
        CHECK(has(head, "property=\"og:url\" content=\"https://acme.dev/about\""));
        CHECK(has(head, "property=\"og:site_name\" content=\"Acme\""));
        // Twitter (large image because an image was set)
        CHECK(has(head, "name=\"twitter:card\" content=\"summary_large_image\""));
        CHECK(has(head, "name=\"twitter:title\" content=\"About · Acme\""));
        // robots default = index,follow
        CHECK(has(head, "name=\"robots\" content=\"index,follow\""));
    }
    // title falls back when Meta.title empty
    {
        Meta m{ .description="d" };
        CHECK(has(detail::render_head(m, "Fallback Title"), "og:title\" content=\"Fallback Title\""));
    }
    // noindex is honoured
    {
        Meta m{ .robots="noindex" };
        CHECK(has(detail::render_head(m, "x"), "name=\"robots\" content=\"noindex\""));
    }
    // no image → twitter card downgrades to summary
    {
        Meta m{ .title="t", .description="d" };
        CHECK(has(detail::render_head(m, "x"), "twitter:card\" content=\"summary\""));
    }
    // attributes are escaped (no injection)
    {
        Meta m{ .description="a \" b < c" };
        auto h = detail::render_head(m, "x");
        CHECK(has(h, "&quot;") && has(h, "&lt;"));
    }

    // ═══ JSON-LD ═══════════════════════════════════════════════════════════════
    {
        auto j = jsonld("Person", {{"name","Ada Lovelace"},{"jobTitle","Engineer"}});
        CHECK(has(j, "\"@context\":\"https://schema.org\""));
        CHECK(has(j, "\"@type\":\"Person\""));
        CHECK(has(j, "\"name\":\"Ada Lovelace\""));
        CHECK(has(j, "\"jobTitle\":\"Engineer\""));
        // it lands in the head as a script tag
        Meta m{ .json_ld = j };
        CHECK(has(detail::render_head(m, "x"), "application/ld+json"));
    }

    // ═══ semantic HTML elements ════════════════════════════════════════════════
    CHECK(has(html_of(box() | as_main), "<main"));
    CHECK(has(html_of(box() | as_nav), "<nav"));
    CHECK(has(html_of(box() | as_header), "<header"));
    CHECK(has(html_of(box() | as_footer), "<footer"));
    CHECK(has(html_of(box() | as_article), "<article"));
    CHECK(has(html_of(box() | as_section), "<section"));
    CHECK(has(html_of(text("Title") | heading_level(1)), "<h1"));
    CHECK(has(html_of(text("Sub") | heading_level(3)), "<h3"));
    CHECK(has(html_of(text("para") | as_p), "<p"));
    // closing tag matches
    { auto h = html_of(box() | as_main); CHECK(has(h, "</main>")); }
    { auto h = html_of(text("x") | heading_level(2)); CHECK(has(h, "</h2>")); }
    // default (no as()) stays div / span
    CHECK(has(html_of(box()), "<div"));
    CHECK(has(html_of(text("x")), "<span"));
    // as() still carries class/attrs/tap
    { auto h = html_of(box() | as_article | tap(1) | pad(8)); CHECK(has(h,"<article") && has(h,"data-tap=\"") && has(h,"class=\"")); }

    std::cout << "test_seo: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
