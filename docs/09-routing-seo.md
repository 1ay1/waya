# Routing & SEO

waya apps are multi-page and **server-render every route** — real, crawlable
HTML with correct metadata, produced before any JavaScript runs. This chapter
covers the router, wiring routes into your Elm loop, and the SEO surface.

## The router

`router()` starts a route table; chain `.at(pattern, id)` to register routes:

```cpp
enum Screen { Feed, Post, Tag, Archive, About, NotFound };

static Router routes() {
    return router()
        .at("/",            Feed)
        .at("/post/:slug",  Post)      // :slug captures one segment
        .at("/tag/:tag",    Tag)
        .at("/archive",     Archive)
        .at("/about",       About);
}
```

### Patterns

- A **literal** segment matches exactly: `/archive`.
- `:name` **captures** one segment: `/post/:slug` matches `/post/hello` with
  `slug = "hello"`.
- A trailing `*` **captures the rest** of the path: `/docs/*` matches
  `/docs/a/b/c` with `*` = `"a/b/c"`.
- Query strings (`?q=…`) and trailing slashes are ignored.
- Routes match in **insertion order** — first match wins, so register specific
  routes before wildcards.

### Matching

```cpp
Match m = routes().match("/post/hello");
m.matched;            // true
m.value;              // Post  (the id you registered)
m.param("slug");      // "hello"
```

`Match` fields: `matched` (bool), `value` (int id), `params` (the captures),
and the convenience `param(name)` (empty string if absent).

### Typed routes — pattern to view, one table (`waya::ui`)

`Router` gives you a screen id you then feed into a `screens()` switch — two
tables to keep in sync, and params re-fetched by string far from the route. The
`Routes` table in `waya::ui` collapses that: each pattern carries the builder
that renders it, and the builder receives the `Match`, so params are read right
where the route is declared — no id enum, no second switch.

```cpp
auto pages = routes()
    .at("/",           []            { return home(); })
    .at("/users",      []            { return user_list(); })
    .at("/users/:id",  [](const Match& m){ return user_detail(m.param("id")); })
    .at("/docs/*",     [](const Match& m){ return docs(m.param("*")); })
    .fallback(         []            { return not_found(); });   // 404

// in view(): render whatever the current path resolves to
static NodeRef view(const Model& m){ return pages.view(m.path); }
// in subscribe(): keep m.path synced to the URL
static Sub<Msg> subscribe(const Model&){
    return Sub<Msg>::on_route([](std::string p){ return Nav{p}; });
}
```

It reuses the core `Router` for matching (`:name`, `*`, query parsing), so
params behave identically — this is only the pattern-to-view binding on top. Use
`pages.matches(path)` to test a route without rendering, `pages.match(path)` to
read its params.

### Query strings

The `?a=1&b=2` portion is parsed for you — no manual string-splitting, ever.
Read it with `q(name)`, and use `has_q(name)` to tell a bare flag (`?debug`)
from an absent one:

```cpp
Match m = routes().match("/search?q=hello+world&page=2&debug");
m.q("q");        // "hello world"   (percent- and +-decoded)
m.q("page");     // "2"
m.has_q("debug"); // true, even though its value is empty
m.q("missing").empty();   // true
```

The query is available even when **no route matched**, so a landing path like
`/unknown?ref=email` still lets you read `m.q("ref")`. Fragments (`#section`)
are dropped.

## Wiring routes into the loop

Routing is just state + effects. The pattern:

1. Store the current screen (and any params) in your `Model`.
2. Subscribe to route changes with `Sub::on_route`, mapping the path to a `Msg`.
3. In `update`, match the path and set the screen/params.
4. Navigate with `Cmd::navigate(...)`.
5. `view` switches on the current screen.

```cpp
struct Model {
    Screen screen = Feed;
    std::string slug, tag;
    /* … */
};
struct Route { std::string path; };   // route changed
struct Nav   { std::string path; };   // request to navigate
using Msg = std::variant<Route, Nav /*, …*/>;

// (2) react to browser route changes (back/forward, deep links)
static Sub<Msg> subscribe(const Model&) {
    return Sub<Msg>::on_route([](std::string p){ return Route{ p }; });
}

// (3)+(4) match on Route; perform navigation on Nav
static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
    return std::visit(overload{
        [&](const Nav& n) -> std::pair<Model,Cmd<Msg>> {
            return { m, Cmd<Msg>::navigate(n.path) };     // pushes history, re-routes
        },
        [&](const Route& r) -> std::pair<Model,Cmd<Msg>> {
            auto match = routes().match(r.path);
            m.screen = match.matched ? (Screen)match.value : NotFound;
            m.slug   = match.param("slug");
            m.tag    = match.param("tag");
            return { m, Cmd<Msg>::none() };
        },
    }, msg);
}
```

Then in `view`, render the current screen. The `screens` helper (from
`sugar.hpp`) is a flat switch:

```cpp
static NodeRef view(const Model& m) {
    return screens((int)m.screen, {
        { Feed,    [&]{ return feed(m); } },
        { Post,    [&]{ return post_view(m); } },
        { Tag,     [&]{ return tag_view(m); } },
        { Archive, [&]{ return archive(m); } },
        { About,   [&]{ return about(); } },
        { NotFound,[&]{ return not_found(); } },
    });
}
```

Every screen is a normal `view`-returning function; only the active one is
built.

### Navigating from the UI

Any interactive node can trigger navigation by sending a `Nav`:

```cpp
text("Read →") | pointer | tap(Nav{ "/post/" + p.slug })
```

`Cmd::navigate` updates the browser's address bar (real history — back/forward
work), re-routes, and re-renders. `Cmd::push_url` updates only the URL without
routing (for deep-link sync).

## SSR: every route is server-rendered

When a browser requests `/post/hello`, the runtime:

1. Runs `init()`, then feeds the route through your `update` (so `m.screen` and
   `m.slug` are set) — exactly as if the user had navigated there.
2. Calls `view(model)` and renders it to complete HTML.
3. Emits `meta(model)` into the `<head>`.

So the first byte the crawler (or the user) receives is the finished page for
that exact route. No loading spinner, no hydration gap.

## Per-route metadata: `meta`

Provide a `meta(const Model&) -> Meta` on your `Program` to set the page's SEO
tags per route:

```cpp
static Meta meta(const Model& m) {
    std::string base = "https://example.com";
    if (m.screen == Post) if (auto* p = find(m, m.slug))
        return {
            .title       = p->title + " · My Blog",
            .description = p->excerpt,
            .canonical   = base + "/post/" + p->slug,
            .image       = p->cover_url,
            .type        = "article",
            .site_name   = "My Blog",
            .author      = p->author,
            .json_ld     = jsonld("Article", {
                { "headline", p->title },
                { "author",   p->author },
                { "datePublished", p->date },
            }),
        };
    return { .title = "My Blog", .description = "…", .canonical = base + "/" };
}
```

### The `Meta` struct

| Field | Renders as | Notes |
|---|---|---|
| `title` | `<title>` + `og:title` + `twitter:title` | falls back to `LiveConfig.title` |
| `description` | `<meta description>` + `og:description` | |
| `canonical` | `<link rel=canonical>` + `og:url` | dedupe/ranking signal |
| `image` | `og:image` + `twitter:image` | social preview |
| `type` | `og:type` | `"website"` (default), `"article"`, `"product"`… |
| `site_name` | `og:site_name` | |
| `author` | `<meta author>` | |
| `keywords` | `<meta keywords>` | comma-separated |
| `robots` | `<meta robots>` | default `index,follow`; set `"noindex,nofollow"` to hide |
| `locale` | `og:locale` | default `en_US` |
| `card` | `twitter:card` | default `summary_large_image` |
| `json_ld` | `<script type=ld+json>` | schema.org structured data |
| `lang` | `<html lang>` | default `en` |

### JSON-LD helper

`jsonld(type, fields)` builds a schema.org JSON-LD string for `Meta.json_ld`:

```cpp
jsonld("Article", {
    { "headline", "Zero-cost abstractions" },
    { "author",   "Ada Lovelace" },
    { "datePublished", "2024-05-01" },
});
```

## robots.txt and sitemap.xml

The runtime serves both automatically:

- **`/robots.txt`** — allows indexing by default; if your `Program` defines
  `static const char* site_url()`, it also points crawlers at your sitemap.
- **`/sitemap.xml`** — lists the routes your `Program` returns from
  `static std::vector<std::string> sitemap()`.

```cpp
static const char* site_url() { return "https://example.com"; }

static std::vector<std::string> sitemap() {
    std::vector<std::string> urls = { "/", "/archive", "/about" };
    for (auto& p : corpus()) urls.push_back("/post/" + p.slug);
    return urls;
}
```

Both are optional. With them, your site ships a valid sitemap and robots policy
with zero extra plumbing.

## SEO checklist

- ✅ Give every route a distinct `title` and `description` in `meta`.
- ✅ Set a `canonical` URL per route.
- ✅ Use semantic elements (`as_main`, `as_article`, `as_nav`, headings) in your
  `view`.
- ✅ Add `json_ld` for rich results on key page types (Article, Product, FAQ).
- ✅ Provide `site_url()` + `sitemap()` so crawlers find everything.
- ✅ Add `image` for good social-share cards.

---

Next: [Scaling to Big Apps](10-scaling.md) — feature modules and organisation
for large codebases.
