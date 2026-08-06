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
