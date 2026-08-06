# SEO in waya

waya apps are SEO-friendly by default and easy to make excellent. The hard part —
real HTML on the first byte — is already handled by SSR (see the runtime); this
adds the `<head>`, semantic markup, and crawler files search engines want.

## 1. SSR — real content, first byte (automatic)

The initial HTTP response is the fully-rendered page for the requested route, not
a blank shell. Crawlers, link unfurlers, and no-JS clients all see content
immediately, and every route renders its own screen. Nothing to configure.

## 2. Per-route `<head>` — one method

Add `static Meta meta(const Model&)`; the runtime computes it *after routing* and
injects a complete head — title, description, canonical, Open Graph, Twitter
card, robots, JSON-LD:

```cpp
static Meta meta(const Model& m) {
    switch (m.screen) {
        case UserView:
            return { .title = who + " · SaaS",
                     .description = who + " — team profile.",
                     .canonical = "https://saas.example/users/" + m.user_id,
                     .type = "profile",
                     .json_ld = jsonld("Person", {{"name",who},{"jobTitle",role}}) };
        case Settings:
            return { .title = "Settings", .robots = "noindex" };   // private page
        default:
            return { .title = "SaaS — ships software",
                     .description = "A tiny SaaS built with waya.",
                     .canonical = "https://saas.example/" };
    }
}
```

Every field is optional. From one `Meta` the runtime emits `<title>`, `<meta
description/author/keywords/robots>`, `<link canonical>`, the full Open Graph set
(`og:title/description/type/url/image/site_name/locale`), the Twitter card, and a
JSON-LD `<script>`. All values are escaped.

## 3. Structured data (rich results)

`jsonld(type, fields)` builds a schema.org object; put it in `Meta.json_ld`:

```cpp
.json_ld = jsonld("Organization", {{"name","Acme"},{"url","https://acme.dev"}})
.json_ld = jsonld("Person", {{"name","Ada"},{"jobTitle","Engineer"}})
```

## 4. Semantic HTML — landmarks + headings

Crawlers weight semantic elements. Make a box say what it *is*, with zero layout
change:

```cpp
nav_bar(...)   | as_nav
sidebar(...)   | as_aside
content(...)   | as_main
article(...)   | as_article
text("Title")  | heading_level(1)     // real <h1>
text("Body")   | as_p
```

`as_main / as_nav / as_header / as_footer / as_article / as_section / as_aside`,
`heading_level(1..6)`, `as_p`, and the escape hatch `as("figure")`.

## 5. robots.txt + sitemap.xml — automatic

Served for you. Declare where the site lives and which routes to list:

```cpp
static const char* site_url() { return "https://saas.example"; }
static std::vector<std::string> sitemap() { return {"/", "/users", "/settings"}; }
```

`GET /robots.txt` → allows all + points to the sitemap. `GET /sitemap.xml` → a
valid urlset of your routes. Omit them and robots.txt still allows everything.

---

See `examples/bigapp.cpp` for the full picture: per-route titles/descriptions,
`Organization`/`Person` JSON-LD, `noindex` on a private page, semantic `<nav>`/
`<main>`/`<h1>`, plus robots.txt and sitemap.xml. Every route SSRs its screen and
passes the W3C HTML5 validator with zero errors.
