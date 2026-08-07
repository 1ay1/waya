#pragma once
/// \file meta.hpp
/// SEO metadata for the SSR'd page. Because waya server-renders real HTML on the
/// first byte, search engines and social crawlers see fully-formed content — but
/// they also need the right <head>. An app declares `static Meta meta(const
/// Model&)`; the runtime computes it PER ROUTE (after routing the SSR model) and
/// injects a complete set of tags: title, description, canonical, Open Graph,
/// Twitter card, robots, keywords, author, and optional JSON-LD structured data.
///
///   static Meta meta(const Model& m) {
///       if (m.route == "/about")
///           return { .title = "About · Acme",
///                    .description = "Who we are and what we build.",
///                    .canonical = "https://acme.dev/about" };
///       return { .title = "Acme — ships software", .description = "…" };
///   }
///
/// Every field is optional; sensible defaults fill the rest. This is all it takes
/// to make a waya app first-class for Google, Twitter, Slack, and friends.

#include <string>
#include <utility>
#include <vector>

namespace waya::surface {

struct Meta {
    std::string title;                 // <title> + og:title (falls back to LiveConfig.title)
    std::string description;           // <meta description> + og:description
    std::string canonical;            // <link canonical> + og:url (dedupe/ranking)
    std::string image;                // og:image / twitter image (social preview)
    std::string type = "website";     // og:type (website/article/product…)
    std::string site_name;            // og:site_name
    std::string author;               // <meta author>
    std::string keywords;             // <meta keywords> (comma-separated)
    std::string robots;               // e.g. "noindex,nofollow" (default: index,follow)
    std::string locale = "en_US";     // og:locale
    std::string card = "summary_large_image";  // twitter:card
    std::string json_ld;             // raw JSON-LD (schema.org structured data)
    std::string lang = "en";          // <html lang>
    bool has() const {                // did the app set anything meaningful?
        return !title.empty() || !description.empty() || !canonical.empty()
            || !image.empty() || !json_ld.empty();
    }
};

/// The HTTP outcome of an SSR request — the response status, an optional
/// redirect, per-route cache policy, and cookies. An app declares
/// `static HttpResult http_status(const Model&)` to make a route return the
/// RIGHT status: a real 404 for an unknown page (so search engines don't index
/// it), a 301/302 redirect, a 410 Gone, custom caching, or a Set-Cookie. Without
/// it, every route is a cacheless 200 (fine for a simple app, wrong for SEO on a
/// content site). Every field is optional.
///
///   static HttpResult http_status(const Model& m) {
///       if (m.screen == NotFound) return HttpResult::not_found();
///       if (m.screen == Moved)    return HttpResult::redirect("/new-home");   // 301
///       return HttpResult::ok().cache(3600);   // 200, cache 1h at the CDN
///   }
struct HttpResult {
    int status = 200;                       ///< HTTP status code (200/301/302/404/410/…)
    std::string location;                   ///< redirect target (for 301/302/307/308)
    long cache_seconds = -1;                ///< >=0 => Cache-Control: public,max-age=N; <0 => no-store
    std::vector<std::string> cookies;       ///< raw Set-Cookie header values
    std::vector<std::pair<std::string,std::string>> headers;  ///< extra response headers

    // ── factories ──────────────────────────────────────────────────────────
    static HttpResult ok()               { return {}; }
    static HttpResult not_found()        { HttpResult r; r.status = 404; return r; }
    static HttpResult gone()             { HttpResult r; r.status = 410; return r; }
    static HttpResult status_(int code)  { HttpResult r; r.status = code; return r; }
    /// A permanent (301) redirect — use for moved pages (passes SEO ranking on).
    static HttpResult redirect(std::string to){ HttpResult r; r.status = 301; r.location = std::move(to); return r; }
    /// A temporary (302) redirect — use for auth gates, A/B, transient moves.
    static HttpResult temporary_redirect(std::string to){ HttpResult r; r.status = 302; r.location = std::move(to); return r; }

    // ── chainable modifiers ──────────────────────────────────────────────
    HttpResult& cache(long seconds){ cache_seconds = seconds; return *this; }
    HttpResult& no_cache(){ cache_seconds = -1; return *this; }
    HttpResult& cookie(std::string set_cookie_value){ cookies.push_back(std::move(set_cookie_value)); return *this; }
    HttpResult& header(std::string name, std::string value){ headers.emplace_back(std::move(name), std::move(value)); return *this; }

    bool is_redirect() const { return status/100 == 3 && !location.empty(); }

    /// The status line text for the wire ("200 OK", "404 Not Found", …).
    const char* status_line() const {
        switch(status){
            case 200: return "200 OK";
            case 201: return "201 Created";
            case 204: return "204 No Content";
            case 301: return "301 Moved Permanently";
            case 302: return "302 Found";
            case 303: return "303 See Other";
            case 307: return "307 Temporary Redirect";
            case 308: return "308 Permanent Redirect";
            case 400: return "400 Bad Request";
            case 401: return "401 Unauthorized";
            case 403: return "403 Forbidden";
            case 404: return "404 Not Found";
            case 410: return "410 Gone";
            case 429: return "429 Too Many Requests";
            case 500: return "500 Internal Server Error";
            case 503: return "503 Service Unavailable";
            default:  return "200 OK";
        }
    }
};

namespace detail {

inline void meta_esc(std::string& o, std::string_view s){
    for(char c : s){ if(c=='<')o+="&lt;"; else if(c=='>')o+="&gt;"; else if(c=='&')o+="&amp;";
        else if(c=='"')o+="&quot;"; else o+=c; }
}
inline void meta_tag(std::string& o, const char* attr, const char* key, const std::string& v){
    if(v.empty()) return;
    o+="<meta "; o+=attr; o+="=\""; o+=key; o+="\" content=\""; meta_esc(o,v); o+="\">";
}

/// Build the SEO <head> tags from a Meta (title is emitted separately by the
/// shell so there's always exactly one). Returns a string of <meta>/<link>/
/// <script> tags to splice into <head>.
inline std::string render_head(const Meta& mt, const std::string& fallback_title){
    std::string t = mt.title.empty() ? fallback_title : mt.title;
    std::string o;
    meta_tag(o, "name", "description", mt.description);
    meta_tag(o, "name", "author", mt.author);
    meta_tag(o, "name", "keywords", mt.keywords);
    meta_tag(o, "name", "robots", mt.robots.empty() ? std::string("index,follow") : mt.robots);
    if(!mt.canonical.empty()){ o+="<link rel=\"canonical\" href=\""; meta_esc(o,mt.canonical); o+="\">"; }
    // Open Graph (Facebook, Slack, LinkedIn, iMessage…)
    meta_tag(o, "property", "og:title", t);
    meta_tag(o, "property", "og:description", mt.description);
    meta_tag(o, "property", "og:type", mt.type);
    meta_tag(o, "property", "og:url", mt.canonical);
    meta_tag(o, "property", "og:image", mt.image);
    meta_tag(o, "property", "og:site_name", mt.site_name);
    meta_tag(o, "property", "og:locale", mt.locale);
    // Twitter card
    meta_tag(o, "name", "twitter:card", mt.image.empty() ? std::string("summary") : mt.card);
    meta_tag(o, "name", "twitter:title", t);
    meta_tag(o, "name", "twitter:description", mt.description);
    meta_tag(o, "name", "twitter:image", mt.image);
    // JSON-LD structured data (schema.org) — rich results.
    if(!mt.json_ld.empty()){ o+="<script type=\"application/ld+json\">"; o+=mt.json_ld; o+="</script>"; }
    return o;
}

} // namespace detail
} // namespace waya::surface
