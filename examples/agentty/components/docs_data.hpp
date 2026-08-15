#pragma once
// examples/agentty/components/docs_data.hpp
//
// The docs/blog content layer. The source of truth is markdown in the agentty
// repo (github.com/1ay1/agentty/docs/website/*.md and docs/website/blog/*.md,
// with YAML frontmatter). Rather than vendor the content, we FETCH it from
// GitHub raw once, parse the frontmatter, cache it in memory, and let the pages
// render it with waya's markdown(). Adding a doc upstream = it appears here, no
// code change.
//
// Fetch uses curl via popen (waya has no outbound HTTP client); results are
// cached process-wide behind a mutex so each URL is pulled at most once.

#include <array>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace agentty::docs {

// ── raw fetch + cache ───────────────────────────────────────────────────────
inline std::string http_get(const std::string& url) {
    static std::mutex m;
    static std::map<std::string, std::string> cache;
    {
        std::lock_guard<std::mutex> l(m);
        auto it = cache.find(url);
        if (it != cache.end()) return it->second;
    }
    std::string cmd = "curl -fsSL --max-time 8 '" + url + "' 2>/dev/null";
    std::string out;
    if (FILE* p = popen(cmd.c_str(), "r")) {
        char buf[8192]; std::size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
        pclose(p);
    }
    std::lock_guard<std::mutex> l(m);
    cache[url] = out;
    return out;
}

// ── a parsed document ───────────────────────────────────────────────────────
struct Doc {
    std::string slug;         // "" for index
    std::string title;
    std::string description;
    std::string nav_section;
    int         nav_order = 999;
    std::string date;         // blog: YYYY-MM-DD
    std::string author;       // blog
    std::string excerpt;      // blog
    std::string body_md;      // markdown after the frontmatter
    bool        ok = false;
};

// minimal YAML frontmatter parse: a leading `---\n … \n---\n` block of key: value.
inline Doc parse_doc(const std::string& raw, const std::string& slug) {
    Doc d; d.slug = slug;
    if (raw.empty()) return d;
    std::size_t pos = 0;
    if (raw.rfind("---", 0) == 0) {
        std::size_t end = raw.find("\n---", 3);
        if (end != std::string::npos) {
            std::string fm = raw.substr(3, end - 3);
            std::size_t body_start = raw.find('\n', end + 1);
            d.body_md = body_start == std::string::npos ? "" : raw.substr(body_start + 1);
            // parse key: value lines
            std::size_t i = 0;
            while (i < fm.size()) {
                std::size_t nl = fm.find('\n', i);
                std::string line = fm.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
                i = nl == std::string::npos ? fm.size() : nl + 1;
                std::size_t c = line.find(':');
                if (c == std::string::npos) continue;
                std::string k = line.substr(0, c);
                std::string v = line.substr(c + 1);
                // trim
                auto trim = [](std::string s){
                    std::size_t a = s.find_first_not_of(" \t\"'");
                    std::size_t b = s.find_last_not_of(" \t\"'\r");
                    return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
                };
                k = trim(k); v = trim(v);
                if (k == "title") d.title = v;
                else if (k == "description") d.description = v;
                else if (k == "nav_section") d.nav_section = v;
                else if (k == "nav_order") { try { d.nav_order = std::stoi(v); } catch (...) {} }
                else if (k == "date") d.date = v;
                else if (k == "author") d.author = v;
                else if (k == "excerpt") d.excerpt = v;
                else if (k == "slug" && d.slug.empty()) d.slug = v;
            }
            pos = 0; (void)pos;
        }
    } else {
        d.body_md = raw;
    }
    if (d.title.empty()) d.title = slug.empty() ? "Introduction" : slug;
    d.ok = true;
    return d;
}

inline constexpr const char* DOCS_BASE =
    "https://raw.githubusercontent.com/1ay1/agentty/master/docs/website/";

/// Fetch + parse a single doc by slug ("" = index).
inline Doc get_doc(const std::string& slug) {
    std::string file = slug.empty() ? "index" : slug;
    return parse_doc(http_get(std::string(DOCS_BASE) + file + ".md"), slug);
}

// ── sidebar nav (canonical order + curated slugs) ───────────────────────────
// Mirrors docs-nav.generated.ts / SECTION_ORDER. Titles are shown as-is; the
// live title comes from each doc's frontmatter when the page loads.
struct NavLink { std::string title, slug; };
struct NavSection { std::string title; std::vector<NavLink> items; };

inline const std::vector<NavSection>& docs_nav() {
    static const std::vector<NavSection> nav = {
        { "Getting Started", {
            { "Introduction", "" },
            { "agentty vs Claude Code", "vs-claude-code" },
            { "Installation", "installation" },
            { "Quick Start", "quick-start" },
            { "Authentication", "authentication" },
            { "Providers & Models", "providers" } } },
        { "User Manual", {
            { "The Interface", "interface" },
            { "Keybindings", "keybindings" },
            { "Permission Profiles", "profiles" },
            { "Threads & Persistence", "threads" },
            { "Configuration", "configuration" },
            { "CLI Reference", "cli" } } },
        { "Tools", {
            { "Tool Overview", "tools" },
            { "Sandboxing", "sandboxing" },
            { "Workspace Boundary", "workspace" },
            { "Retrieval (RAG)", "retrieval" } } },
        { "Advanced", {
            { "Architecture", "architecture" },
            { "SSH Air-gap", "airgap" },
            { "Runs in Zed (ACP)", "acp" },
            { "MCP", "mcp" },
            { "Proxies", "proxies" },
            { "Building from Source", "building" } } },
        { "Help", {
            { "FAQ", "faq" } } },
    };
    return nav;
}

// ── blog ────────────────────────────────────────────────────────────────────
inline constexpr const char* BLOG_BASE =
    "https://raw.githubusercontent.com/1ay1/agentty.org/master/content/blog/";

// Blog post slugs (newest first). Mirrors content/blog.
inline const std::vector<std::string>& blog_slugs() {
    static const std::vector<std::string> s = {
        "agentty-0-2-12-acp-and-retrieval",
        "agentty-0-2-11-cost-aware-agent",
        "agentty-0-2-9-retrieval-engine",
        "agentty-0-2-0-zed-acp",
        "implementing-acp-in-cpp",
        "cpp-design-of-a-terminal-agent",
        "why-cpp-coding-agent-50x-faster",
        "why-terminal-first-ai-feels-faster",
    };
    return s;
}

inline Doc get_post(const std::string& slug) {
    return parse_doc(http_get(std::string(BLOG_BASE) + slug + ".md"), slug);
}

} // namespace agentty::docs
