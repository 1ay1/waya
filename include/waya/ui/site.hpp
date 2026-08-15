#pragma once
/// \file ui/site.hpp
/// site — a marketing/docs-site toolkit. Every landing page is the same handful
/// of blocks: a hero, content sections (eyebrow + title + sub + body), feature-
/// card grids, a stats row, an install/copy band, a comparison table, a nav, a
/// footer, a CTA. This header makes each ONE call, composed from the core
/// vocabulary — no new mechanism, just the patterns named so a whole page is a
/// readable list of sections instead of a wall of `raw_css`.
///
///   view() {
///     return page(
///       site_nav("agentty", "v0.2", { {"Docs","/docs"}, {"Blog","/blog"} },
///                nav_cta("Star on GitHub", "https://github.com/…")),
///       hero("Blazing-fast coding agent", "in your terminal.",
///            "A drop-in claude-code, in C++26.",
///            cta_row(cta_primary("Get started", "/docs"),
///                    cta_ghost("GitHub →", "https://github.com/…"))),
///       section("Speed", "Native, not interpreted.",
///               "Measured on the same box, same day.",
///               compare_table({"", "agentty", "claude-code"}, {
///                 {"Cold start", "~2 ms", "~150 ms"},
///                 {"Binary",     "13 MB", "222 MB"} })),
///       feature_section("Features", "Everything you'd expect.", "", {
///         feature("Sandboxed", "Every tool runs jailed by default."),
///         feature("Any model", "Claude, OpenAI, Groq, or local Ollama.") }),
///       cta_band("Ready?", "One line to install.",
///                copy_line("curl -fsSL https://… | sh")),
///       site_footer("agentty", "MIT licensed.", { {"GitHub","/gh"} }));
///   }
///
/// Colours default to a GitHub-dark palette but every builder takes a `SiteTheme`
/// (pass one to `page(theme, …)`), so a whole site re-skins from one struct.

#include "../surface/node.hpp"
#include "../surface/sugar.hpp"
#include "components.hpp"
#include "icons.hpp"

#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── theme ─────────────────────────────────────────────────────────────────────
/// The palette + measurements every site builder reads. Defaults to a clean
/// GitHub-dark look; override any field and pass to `page(theme, …)`.
struct SiteTheme {
    std::uint32_t bg      = 0x0d1117;   // page background
    std::uint32_t surface = 0x161b22;   // cards / raised panels
    std::uint32_t border  = 0x30363d;   // hairlines
    std::uint32_t text    = 0xe6edf3;   // primary text
    std::uint32_t dim     = 0x8b949e;   // secondary text
    std::uint32_t faint   = 0x656d76;   // captions / notes
    std::uint32_t accent  = 0x58a6ff;   // links / primary
    std::uint32_t accent2 = 0xd2a8ff;   // gradient partner
    std::uint32_t code_bg = 0x010409;   // code / terminal panes
    int           maxw    = 1120;       // content column width (px)
};

namespace site_detail {
// The active theme for this render. Set by `page(theme, …)`; builders read it so
// you don't thread a theme argument through every call. Thread-local: view()
// runs on one owner thread per session.
inline thread_local SiteTheme g_theme{};
inline const SiteTheme& T(){ return g_theme; }

/// A centered, max-width content column — the `.wrap` every section uses.
inline NodeRef wrap(NodeRef inner){
    return box(std::move(inner)) | mx_auto | w_full
        | max_w((float)T().maxw)
        | pad_x(24);
}
/// A node rendered as an <a href> — real navigable link (keyboard + crawlable),
/// URL scheme-sanitised. Used by nav/footer/cta so links are proper anchors.
inline Mod link(std::string url){ return as("a") | href(std::move(url)); }
}

// ── page ──────────────────────────────────────────────────────────────────────
/// `page(theme, sections…)` — the site root: sets the theme for every builder
/// below, paints the page background + base text colour, and stacks the
/// sections. `page(sections…)` uses the default theme.
template <typename... Sections>
inline NodeRef site_page(SiteTheme theme, Sections... sections){
    site_detail::g_theme = theme;
    auto root = col(std::move(sections)...)
        | w_full | fg(theme.text) | bg(theme.bg)
        | detail::raw_css("min-height", "100dvh")
        | font_family("var(--font-sans), Inter, system-ui, -apple-system, Segoe UI, Roboto, sans-serif");
    return root;
}
template <typename S0, typename... Sections>
    requires (!std::is_same_v<std::decay_t<S0>, SiteTheme>)
inline NodeRef site_page(S0 s0, Sections... sections){
    return site_page(SiteTheme{}, std::move(s0), std::move(sections)...);
}

// ── nav ───────────────────────────────────────────────────────────────────────
struct NavLink { std::string label, href; };

/// `nav_cta(label, href, primary=true)` — the button on the right of the nav.
inline NodeRef nav_cta(std::string label, std::string href, bool primary = true){
    using namespace site_detail;
    auto b = box(text(std::move(label)) | semibold | text_size(14))
        | pad_x(16) | pad_y(9) | round(9) | pointer | no_underline
        | site_detail::link(std::move(href));
    if (primary) b = b | bg(T().accent) | fg(0xffffff);
    else         b = b | fg(T().text) | border(1, rgba(T().border, 1.0f));
    return b | on(Hover, detail::raw_css("filter","brightness(1.08)"));
}

/// `site_nav(brand, version, links, right…)` — a sticky top bar: brand + version
/// chip on the left, links in the middle, your CTAs on the right. Blurred,
/// hairline-bottomed, the standard doc/marketing header.
template <typename... Right>
inline NodeRef site_nav(std::string brand, std::string version,
                        std::vector<NavLink> links, Right... right){
    using namespace site_detail;
    std::vector<NodeRef> link_nodes;
    for (auto& l : links)
        link_nodes.push_back(text(l.label) | fg(T().dim)
            | text_size(14.5f) | pointer | no_underline
            | site_detail::link(l.href)
            | on(Hover, fg(T().text)));
    auto links_row = box(); links_row->kids = std::move(link_nodes);
    links_row->style.flow = Flow::row; finalize(*links_row);
    links_row = links_row | gap(22) | items_center | surface::wrap;

    auto brand_node = row(
        text(std::move(brand)) | bold | fg(T().text) | text_size(16),
        version.empty() ? nothing()
            : (text(std::move(version)) | fg(T().faint) | text_size(12)
               | pad_x(7) | pad_y(2) | round(999) | bg(rgba(T().border, 0.5f))))
        | items_center | gap(9);

    auto right_box = row(std::move(right)...) | items_center | gap(10);

    auto bar = row(brand_node, links_row, box() | grow(), std::move(right_box))
        | items_center | gap(28)
        | mx_auto | max_w((float)T().maxw) | w_full | pad_x(24) | pad_y(0)
        | detail::raw_css("height","64px");

    return box(std::move(bar)) | as_nav | w_full | sticky_top(0) | z(50)
        | detail::raw_css("background", detail::rgba_hex(T().bg, 0.72f))
        | backdrop_blur(12)
        | detail::raw_css("border-bottom", "1px solid " + detail::hexstr(T().border));
}

// ── hero ──────────────────────────────────────────────────────────────────────
/// `cta_primary(label, href)` / `cta_ghost(label, href)` — the two hero buttons.
inline NodeRef cta_primary(std::string label, std::string href){
    using namespace site_detail;
    return box(text(std::move(label)) | semibold | text_size(15))
        | pad_x(24) | pad_y(13) | round(11) | pointer | no_underline
        | bg(T().accent) | fg(0xffffff)
        | site_detail::link(std::move(href))
        | glow_under(T().accent, 34, 10)
        | transition("transform .14s, filter .15s")
        | on(Hover, detail::raw_css("transform","translateY(-2px)"))
        | on(Hover, detail::raw_css("filter","brightness(1.06)"));
}
inline NodeRef cta_ghost(std::string label, std::string href){
    using namespace site_detail;
    return box(text(std::move(label)) | semibold | fg(T().text) | text_size(15))
        | pad_x(24) | pad_y(13) | round(11) | pointer | no_underline
        | border(1, rgba(T().border, 1.0f))
        | site_detail::link(std::move(href))
        | transition("transform .14s, border-color .15s")
        | on(Hover, detail::raw_css("transform","translateY(-2px)"))
        | on(Hover, detail::raw_css("border-color", detail::hexstr(T().accent)));
}
/// `cta_row(buttons…)` — center a set of hero buttons that wrap on mobile.
template <typename... Btns>
inline NodeRef cta_row(Btns... btns){
    return row(std::move(btns)...) | items_center | gap(12)
        | justify_center | wrap
        | detail::raw_css("margin-top","28px");
}

/// `hero(title, title_accent, lede, actions…)` — the top of the page. The title
/// renders big + fluid; `title_accent` is the gradient-highlighted second line
/// (pass "" for none). `lede` is the sub-paragraph; `actions` is usually a
/// `cta_row(...)`. Sits on a soft radial glow.
template <typename... Actions>
inline NodeRef site_hero(std::string title, std::string title_accent, std::string lede,
                    Actions... actions){
    using namespace site_detail;
    std::vector<NodeRef> head;
    head.push_back(text(std::move(title))
        | font_fluid(34, 62) | detail::raw_css("font-weight","800")
        | tracking_em(-0.03f) | leading(1.05f) | text_center);
    if (!title_accent.empty())
        head.push_back(text(std::move(title_accent))
            | font_fluid(34, 62) | detail::raw_css("font-weight","800")
            | tracking_em(-0.03f) | leading(1.05f) | text_center
            | gradient_text(T().accent, T().accent2, 100));
    if (!lede.empty())
        head.push_back(text(std::move(lede)) | fg(T().dim)
            | detail::raw_css("font-size","clamp(16px, 2.2vw, 20px)")
            | leading(1.6f) | text_center | max_w(660) | mx_auto
            | detail::raw_css("margin-top","20px"));
    (head.push_back(std::move(actions)), ...);

    auto inner = box(); inner->kids = std::move(head); inner->style.flow = Flow::col;
    finalize(*inner);
    inner = inner | items_center | gap(0);

    return box(wrap(inner)) | as_section | w_full
        | detail::raw_css("padding","96px 0 80px")
        | radial(T().accent, 50, -10, T().bg, 90);
}

// ── section (the content workhorse) ──────────────────────────────────────────
namespace site_detail {
/// The eyebrow + title + sub header shared by section()/feature_section().
inline std::vector<NodeRef> section_head(std::string eyebrow, std::string title, std::string sub){
    std::vector<NodeRef> k;
    if (!eyebrow.empty())
        k.push_back(text(std::move(eyebrow)) | fg(T().accent) | semibold
            | text_size(13)
            | tracking_em(0.06f) | uppercase);
    if (!title.empty())
        k.push_back(text(std::move(title))
            | font_fluid(26, 38) | detail::raw_css("font-weight","700")
            | tracking_em(-0.02f) | detail::raw_css("margin-top","10px"));
    if (!sub.empty())
        k.push_back(text(std::move(sub)) | fg(T().dim)
            | text_size(17) | leading(1.6f)
            | max_w(680) | detail::raw_css("margin-top","12px"));
    return k;
}
}

/// `section(eyebrow, title, sub, body…)` — a titled content block with a top
/// hairline. Pass any nodes as the body (a table, a paragraph, a grid).
template <typename... Body>
inline NodeRef site_section(std::string eyebrow, std::string title, std::string sub, Body... body){
    using namespace site_detail;
    auto kids = section_head(std::move(eyebrow), std::move(title), std::move(sub));
    (kids.push_back(std::move(body)), ...);
    auto inner = box(); inner->kids = std::move(kids); inner->style.flow = Flow::col;
    finalize(*inner);
    inner = inner | gap(0);
    return box(wrap(inner)) | as_section | w_full
        | detail::raw_css("padding","72px 0")
        | detail::raw_css("border-top","1px solid " + detail::hexstr(T().border));
}

// ── features ──────────────────────────────────────────────────────────────────
/// `feature(title, body, icon="")` — one card for a feature grid. Lifts on hover.
inline NodeRef site_feature(std::string title, std::string body, std::string icon_name = {}){
    using namespace site_detail;
    return col(
        icon_name.empty() ? nothing()
            : (box(icon(icon_name, 22)) | fg(T().accent) | detail::raw_css("margin-bottom","12px")),
        text(std::move(title)) | semibold | fg(T().text) | text_size(17),
        text(std::move(body)) | fg(T().dim) | text_size(14.5f)
            | leading(1.55f) | detail::raw_css("margin-top","8px"))
        | pad(24) | round(14) | bg(T().surface)
        | border(1, rgba(T().border, 1.0f))
        | transition("transform .15s, border-color .15s")
        | on(Hover, detail::raw_css("transform","translateY(-3px)"))
        | on(Hover, detail::raw_css("border-color", detail::hexstr(T().accent)));
}
/// `feature_grid(cards, minColPx=280)` — a responsive auto-fitting card grid.
inline NodeRef site_feature_grid(std::vector<NodeRef> cards, int min_col = 280){
    auto g = box(); g->kids = std::move(cards); g->style.flow = Flow::grid; finalize(*g);
    return g | auto_grid((float)min_col) | gap(16) | w_full
        | detail::raw_css("margin-top","36px");
}
/// `feature_section(eyebrow, title, sub, cards)` — a section whose body is a
/// feature-card grid (the most common section shape). One call.
inline NodeRef site_features(std::string eyebrow, std::string title, std::string sub,
                               std::vector<NodeRef> cards, int min_col = 280){
    return site_section(std::move(eyebrow), std::move(title), std::move(sub),
                   site_feature_grid(std::move(cards), min_col));
}

// ── stats row ─────────────────────────────────────────────────────────────────
/// `stat(number, label)` — one big-number cell for a stats row.
inline NodeRef site_stat(std::string number, std::string label){
    using namespace site_detail;
    return col(
        text(std::move(number)) | gradient_text(T().accent, T().accent2, 100)
            | font_fluid(30, 46) | detail::raw_css("font-weight","800") | tracking_em(-0.02f),
        text(std::move(label)) | fg(T().dim) | text_size(14)
            | detail::raw_css("margin-top","4px"))
        | items_center | text_center;
}
/// `stats_row(stats…)` — a centered row of big-number stats that wraps on mobile.
template <typename... Stats>
inline NodeRef stats_row(Stats... stats){
    return row(std::move(stats)...) | justify_center | items_start
        | gap(56) | wrap | w_full
        | detail::raw_css("padding","28px 0");
}

// ── install / copy band ───────────────────────────────────────────────────────
/// `copy_line(command)` — a mono command in a code pane with a "copy" affordance.
/// Clicking copies via `Cmd::copy` — wire it: `copy_line(cmd, CopyCmd{cmd})`.
inline NodeRef copy_line_view(std::string command){
    using namespace site_detail;
    return row(
        text("$ ") | fg(T().faint) | mono,
        text(command) | fg(T().text) | mono | text_size(14.5f) | grow(),
        text("copy") | fg(T().dim) | text_size(12.5f)
            | pad_x(9) | pad_y(4) | round(7) | border(1, rgba(T().border,1.0f)))
        | items_center | gap(10)
        | pad_x(18) | pad_y(14) | round(12)
        | bg(T().code_bg) | border(1, rgba(T().border, 1.0f))
        | text_size(14.5f) | max_w(560) | mx_auto | w_full;
}
/// The tappable form: clicking the row copies `command` to the clipboard.
template <typename Msg>
inline NodeRef copy_line(std::string command, Msg on_copy){
    return copy_line_view(command) | pointer | tap(std::move(on_copy));
}

/// `cta_band(title, sub, body…)` — the closing call-to-action: centered title +
/// sub + whatever you pass (a copy_line, a cta_row). Distinct tinted background.
template <typename... Body>
inline NodeRef cta_band(std::string title, std::string sub, Body... body){
    using namespace site_detail;
    std::vector<NodeRef> k;
    k.push_back(text(std::move(title)) | font_fluid(28, 42)
        | detail::raw_css("font-weight","800") | tracking_em(-0.02f) | text_center);
    if (!sub.empty())
        k.push_back(text(std::move(sub)) | fg(T().dim)
            | text_size(17) | text_center | max_w(560) | mx_auto
            | detail::raw_css("margin-top","12px"));
    (k.push_back(std::move(body)), ...);
    auto inner = box(); inner->kids = std::move(k); inner->style.flow = Flow::col;
    finalize(*inner);
    inner = inner | items_center | gap(20);
    return box(wrap(inner)) | as_section | w_full
        | detail::raw_css("padding","88px 0")
        | detail::raw_css("border-top","1px solid " + detail::hexstr(T().border))
        | radial(T().accent2, 50, 120, T().bg, 70);
}

// ── comparison table ──────────────────────────────────────────────────────────
/// `compare_table(headers, rows)` — a bordered comparison grid. The FIRST body
/// column of each row is a row-label; the SECOND column's cell is highlighted as
/// the "winner" (green), the classic "us vs. them" table. `headers[0]` is the
/// empty top-left corner.
inline NodeRef compare_table(std::vector<std::string> headers,
                             std::vector<std::vector<std::string>> rows){
    using namespace site_detail;
    int cols = (int)headers.size();
    std::vector<NodeRef> cells;
    auto th = [&](std::string s, bool win){
        return box(text(std::move(s)) | semibold
                   | fg(win ? T().accent : T().dim) | text_size(13.5f))
            | pad_x(16) | pad_y(12)
            | detail::raw_css("border-bottom","1px solid " + detail::hexstr(T().border)); };
    for (int c = 0; c < cols; ++c) cells.push_back(th(headers[(std::size_t)c], c == 1));
    for (auto& r : rows){
        for (int c = 0; c < cols && c < (int)r.size(); ++c){
            bool win = (c == 1);
            auto cell = box(text(r[(std::size_t)c])
                            | fg(c == 0 ? T().text : (win ? 0x3fb950 : T().dim))
                            | (win ? semibold : Mod{})
                            | mono | text_size(13.5f))
                | pad_x(16) | pad_y(11)
                | detail::raw_css("border-bottom","1px solid " + detail::rgba_hex(T().border, 0.5f));
            cells.push_back(std::move(cell));
        }
    }
    auto g = box(); g->kids = std::move(cells); g->style.flow = Flow::grid; finalize(*g);
    return g | grid_cols("minmax(160px,1.4fr) " + [&]{ std::string t; for(int c=1;c<cols;++c) t += "1fr "; return t; }())
        | round(14) | clip
        | bg(T().surface) | border(1, rgba(T().border, 1.0f))
        | detail::raw_css("margin-top","32px") | w_full;
}

// ── footer ────────────────────────────────────────────────────────────────────
/// `site_footer(brand, tagline, links)` — a hairline-topped footer: brand +
/// small tagline on the left, a row of links on the right.
inline NodeRef site_footer(std::string brand, std::string tagline, std::vector<NavLink> links){
    using namespace site_detail;
    std::vector<NodeRef> link_nodes;
    for (auto& l : links)
        link_nodes.push_back(text(l.label) | fg(T().dim)
            | text_size(14) | pointer | no_underline
            | site_detail::link(l.href) | on(Hover, fg(T().text)));
    auto links_row = box(); links_row->kids = std::move(link_nodes);
    links_row->style.flow = Flow::row; finalize(*links_row);
    links_row = links_row | gap(20) | items_center | surface::wrap;

    auto left = col(
        text(std::move(brand)) | bold | fg(T().text) | text_size(15),
        tagline.empty() ? nothing()
            : (text(std::move(tagline)) | fg(T().faint) | text_size(13)
               | detail::raw_css("margin-top","4px")))
        | gap(0);

    auto bar = row(std::move(left), box() | grow(), std::move(links_row))
        | items_center | gap(24) | surface::wrap
        | mx_auto | max_w((float)T().maxw) | w_full | pad_x(24) | pad_y(40);

    return box(std::move(bar)) | as_footer | w_full
        | detail::raw_css("border-top","1px solid " + detail::hexstr(T().border));
}

} // namespace waya::ui
