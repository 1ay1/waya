#pragma once
// examples/agentty/components/pages.hpp
//
// The non-home pages: the docs shell (sidebar + rendered markdown + edit link),
// the blog index + individual posts, and a simple standalone-page frame. Content
// is fetched from GitHub (docs_data.hpp) and rendered with waya's markdown().
// Registered CSS is lifted from globals.css.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "theme.hpp"
#include "docs_data.hpp"

#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

namespace pages_detail {

inline void install_docs_css() {
    static bool done = false; if (done) return; done = true;
    assets().css(
        // docs shell
        ".docs-shell{display:grid;grid-template-columns:250px minmax(0,1fr) 200px;gap:40px;max-width:1320px;margin:0 auto;padding:36px 24px 80px}"
        ".docs-side{position:sticky;top:calc(var(--nav-h) + 24px);align-self:start;max-height:calc(100vh - var(--nav-h) - 48px);overflow-y:auto}"
        ".docs-side .sec{margin-bottom:22px}"
        ".docs-side .sec-title{font-size:12px;text-transform:uppercase;letter-spacing:.07em;color:var(--text-faint);margin:0 0 8px;font-weight:700}"
        ".docs-side a{display:block;color:var(--text-dim);font-size:14px;padding:5px 11px;border-radius:7px;border-left:2px solid transparent;text-decoration:none}"
        ".docs-side a:hover{color:var(--text);background:var(--bg-elev)}"
        ".docs-side a.active{color:var(--accent);background:color-mix(in srgb,var(--accent) 10%,transparent);border-left-color:var(--accent);font-weight:600}"
        ".docs-main{min-width:0}"
        ".docs-main h1{font-size:38px;margin:0 0 8px;letter-spacing:-.03em;line-height:1.1}"
        ".docs-main .lead{color:var(--text-dim);font-size:18px;margin:0 0 32px;line-height:1.6}"
        ".docs-main h2{font-size:24px;margin:44px 0 14px;letter-spacing:-.02em}"
        ".docs-main h3{font-size:18px;margin:28px 0 10px}"
        ".docs-main p{color:var(--text);margin:0 0 16px;line-height:1.75}"
        ".docs-main ul,.docs-main ol{color:var(--text);padding-left:22px;margin:0 0 16px}"
        ".docs-main li{margin:6px 0}.docs-main li::marker{color:var(--text-faint)}"
        ".docs-main strong{color:var(--text)}.docs-main a{color:var(--accent-2)}"
        ".docs-main code{background:var(--bg-elev);border:1px solid var(--border-soft);border-radius:6px;padding:1.5px 6px;font-size:.86em;color:var(--accent-2);font-family:var(--mono)}"
        ".docs-main pre{background:var(--code-bg);border:1px solid var(--border);border-radius:12px;padding:18px 20px;overflow-x:auto;margin:0 0 20px;font-size:13.5px;line-height:1.65}"
        ".docs-main pre code{color:#e6edf3;background:none;border:0;padding:0}"
        ".docs-main h2,.docs-main h3{scroll-margin-top:calc(var(--nav-h) + 20px)}"
        ".docs-main blockquote{border-left:3px solid var(--accent);background:color-mix(in srgb,var(--accent) 5%,transparent);border-radius:0 10px 10px 0;padding:2px 18px;margin:0 0 20px;color:var(--text-dim)}"
        ".docs-main table{width:100%;border-collapse:collapse;font-size:14px;margin:0 0 20px}"
        ".docs-main th,.docs-main td{text-align:left;padding:10px 14px;border-bottom:1px solid var(--border-soft)}"
        ".docs-main thead th{background:var(--bg-soft);color:var(--text-faint);font-size:12px;text-transform:uppercase}"
        ".docs-toc{position:sticky;top:calc(var(--nav-h) + 24px);align-self:start;font-size:13px}"
        ".docs-toc .t{color:var(--text-faint);text-transform:uppercase;letter-spacing:.06em;margin-bottom:10px;font-weight:700}"
        ".docs-toc a{display:block;color:var(--text-dim);padding:4px 0;text-decoration:none}.docs-toc a:hover{color:var(--text)}"
        ".breadcrumb{display:flex;gap:8px;align-items:center;font-size:13px;color:var(--text-faint);margin-bottom:16px;flex-wrap:wrap}"
        ".breadcrumb a{color:var(--text-dim)}.breadcrumb a:hover{color:var(--text)}.breadcrumb .cur{color:var(--text)}"
        ".docnav{display:flex;justify-content:space-between;gap:16px;margin-top:48px;padding-top:24px;border-top:1px solid var(--border-soft)}"
        ".docnav a{display:flex;flex-direction:column;gap:3px;color:var(--text-dim);border:1px solid var(--border);border-radius:10px;padding:12px 16px;text-decoration:none;max-width:48%}"
        ".docnav a:hover{border-color:var(--accent);color:var(--text)}.docnav .k{font-size:12px;color:var(--text-faint)}"
        "@media (max-width:1024px){.docs-shell{grid-template-columns:1fr}.docs-side,.docs-toc{display:none}}"
        // blog
        ".blog-wrap{max-width:900px;margin:0 auto;padding:56px 24px 80px}"
        ".blog-h{font-size:clamp(30px,4vw,44px);letter-spacing:-.03em;margin:0 0 10px}"
        ".blog-sub{color:var(--text-dim);font-size:18px;margin:0 0 40px}"
        ".blog-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}"
        ".blog-card{display:flex;flex-direction:column;text-decoration:none;color:inherit;background:var(--bg-elev);"
        "border:1px solid var(--border);border-radius:14px;padding:22px 24px;transition:border-color .2s ease,transform .25s cubic-bezier(.2,.8,.2,1),box-shadow .2s ease}"
        ".blog-card:hover{border-color:var(--accent);transform:translateY(-3px);box-shadow:0 18px 40px -22px rgba(0,0,0,.6)}"
        ".blog-card-title{font-size:19px;line-height:1.3;margin:10px 0 8px;color:var(--text)}"
        ".blog-card-excerpt{color:var(--text-dim);font-size:14px;line-height:1.6;margin:0;flex:1}"
        ".blog-card-meta{display:flex;align-items:center;gap:8px;font-family:var(--mono);font-size:12.5px;color:var(--text-faint)}"
        ".blog-card-bottom{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:16px}"
        ".blog-card-more{color:var(--accent);font-size:13.5px;font-weight:600;white-space:nowrap}"
        ".blog-tags{display:flex;flex-wrap:wrap;gap:7px}"
        ".blog-tag{font-family:var(--mono);font-size:11px;color:var(--text-dim);background:var(--bg);border:1px solid var(--border-soft);border-radius:999px;padding:2px 9px}"
        "@media (max-width:720px){.blog-grid{grid-template-columns:1fr}}"
        // blog post
        ".post-wrap{max-width:760px;margin:0 auto;padding:56px 24px 80px}"
        ".post-h{font-size:clamp(30px,4.4vw,46px);letter-spacing:-.03em;line-height:1.1;margin:14px 0 14px}"
        ".post-meta{display:flex;align-items:center;gap:10px;font-family:var(--mono);font-size:13px;color:var(--text-faint);margin-bottom:32px}"
        // reuse docs-main prose styles for post body
        // standalone page
        ".page-wrap{max-width:760px;margin:0 auto;padding:64px 24px 80px}"
        ".page-h{font-size:clamp(30px,4vw,42px);letter-spacing:-.03em;margin:0 0 24px}");
}

// breadcrumb: Home / Docs / <title>
inline NodeRef breadcrumb(const std::string& title) {
    return markup("<nav class=\"breadcrumb\"><a href=\"/\">Home</a><span>/</span>"
        "<a href=\"/docs\">Docs</a><span>/</span><span class=\"cur\">" + title + "</span></nav>");
}

// the left sidebar with the current slug highlighted
inline NodeRef docs_sidebar(const std::string& active_slug) {
    std::string html = "<aside class=\"docs-side\">";
    for (auto& sec : docs::docs_nav()) {
        html += "<div class=\"sec\"><p class=\"sec-title\">" + sec.title + "</p>";
        for (auto& it : sec.items) {
            std::string href = it.slug.empty() ? "/docs" : "/docs/" + it.slug;
            std::string cls = it.slug == active_slug ? "active" : "";
            html += "<a class=\"" + cls + "\" href=\"" + href + "\">" + it.title + "</a>";
        }
        html += "</div>";
    }
    html += "</aside>";
    return markup(html);
}

} // namespace pages_detail

/// `agentty::docs_page(slug)` — the docs shell for a given slug: sidebar +
/// fetched+rendered markdown + a table of contents.
inline NodeRef docs_page(const std::string& slug) {
    using namespace pages_detail;
    install_theme(); install_docs_css();
    docs::Doc d = docs::get_doc(slug);

    // main content: breadcrumb + h1 + lead + rendered markdown body
    auto body = markdown(d.body_md) | add_class("md-body");
    auto main = box(
        breadcrumb(d.title),
        markup("<h1>" + d.title + "</h1>" +
            (d.description.empty() ? "" : "<p class=\"lead\">" + d.description + "</p>")),
        body
    ) | as("article") | add_class("docs-main");

    // simple TOC placeholder (client could populate from headings)
    auto toc = markup("<nav class=\"docs-toc\"><p class=\"t\">On this page</p>"
        "<a href=\"#\">Top</a></nav>");

    return box(docs_sidebar(slug), main, toc)
        | add_class("docs-shell") | detail::raw_css("padding-top", "calc(var(--nav-h) + 12px)");
}

/// `agentty::blog_index()` — the blog listing: a card grid of posts.
inline NodeRef blog_index() {
    using namespace pages_detail;
    install_theme(); install_docs_css();

    std::string cards;
    for (auto& slug : docs::blog_slugs()) {
        docs::Doc p = docs::get_post(slug);
        if (!p.ok) continue;
        cards += "<a class=\"blog-card\" href=\"/blog/" + slug + "\">"
            "<div class=\"blog-card-meta\"><span>" + (p.date.empty() ? "" : p.date) + "</span>"
            "<span class=\"dot\">\xc2\xb7</span><span>" + (p.author.empty() ? "agentty" : p.author) + "</span></div>"
            "<h2 class=\"blog-card-title\">" + p.title + "</h2>"
            "<p class=\"blog-card-excerpt\">" + p.excerpt + "</p>"
            "<div class=\"blog-card-bottom\"><span class=\"blog-card-more\">Read \xe2\x86\x92</span></div></a>";
    }

    return box(markup(
        "<div class=\"blog-wrap\">"
        "<h1 class=\"blog-h\">Blog</h1>"
        "<p class=\"blog-sub\">Deep dives on building a native terminal coding agent in C++26.</p>"
        "<div class=\"blog-grid\">" + cards + "</div></div>"))
        | detail::raw_css("padding-top", "calc(var(--nav-h))");
}

/// `agentty::blog_post(slug)` — a single blog post.
inline NodeRef blog_post(const std::string& slug) {
    using namespace pages_detail;
    install_theme(); install_docs_css();
    docs::Doc p = docs::get_post(slug);

    auto header = markup(
        "<nav class=\"breadcrumb\"><a href=\"/\">Home</a><span>/</span><a href=\"/blog\">Blog</a>"
        "<span>/</span><span class=\"cur\">" + p.title + "</span></nav>"
        "<div class=\"post-meta\"><span>" + p.date + "</span><span class=\"dot\">\xc2\xb7</span>"
        "<span>" + (p.author.empty() ? "agentty" : p.author) + "</span></div>");
    // the post's own H1 is usually the first line of body_md; render body as-is.
    auto body = markdown(p.body_md) | add_class("docs-main");

    return box(box(header, body) | add_class("docs-main"))
        | add_class("post-wrap") | detail::raw_css("padding-top", "calc(var(--nav-h) + 12px)");
}

/// `agentty::simple_page(title, body_html)` — a centered standalone page frame
/// (community, contributing, license, etc.).
inline NodeRef simple_page(std::string title, std::string body_html) {
    using namespace pages_detail;
    install_theme(); install_docs_css();
    return box(box(
        markup("<h1 class=\"page-h\">" + title + "</h1>"),
        markup(body_html) | add_class("docs-main")
    ) | add_class("page-wrap") | detail::raw_css("padding-top", "calc(var(--nav-h))"))
        ;
}

} // namespace agentty
