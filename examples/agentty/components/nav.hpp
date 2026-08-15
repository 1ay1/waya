#pragma once
// examples/agentty/components/nav.hpp
//
// SiteNav — the top navigation bar. 1:1 port of components/SiteNav.tsx +
// the .nav/.brand/.nav-links/.cmdk-trigger/.ghbtn/.nav-toggle/.mobile-menu
// rules from globals.css. Fixed, blurred, hairline-bottomed header with:
//   • brand: blinking ▌ mark + "agentty" + version pill
//   • primary nav links
//   • ⌘K command-palette trigger
//   • ThemeToggle (framework primitive)
//   • Discord + GitHub buttons (exact inline SVGs from source)
//   • a hamburger that opens a mobile menu (client-only toggle)
//
// The real CSS is registered verbatim so layout/hover/responsive match exactly.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "theme.hpp"

#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

namespace nav_detail {

inline const char* NAV_CSS =
".wrap{width:100%;max-width:var(--maxw);margin:0 auto;padding:0 28px}"
".nav{position:fixed;top:0;left:0;right:0;z-index:100;height:var(--nav-h);display:flex;align-items:center;"
"background:var(--bg);border-bottom:1px solid var(--border-soft)}"
"@supports (backdrop-filter:blur(1px)){.nav{background:var(--nav-bg);backdrop-filter:saturate(180%) blur(16px);-webkit-backdrop-filter:saturate(180%) blur(16px)}}"
".nav-inner{display:flex;align-items:center;gap:22px;width:100%}"
".brand{display:flex;align-items:center;gap:9px;color:var(--text);font-weight:700;letter-spacing:-.03em;font-size:17px;text-decoration:none}"
".brand:hover{text-decoration:none}"
".brand-mark{color:var(--accent);font-family:var(--mono);font-weight:700;animation:blink 1.3s steps(2,start) infinite}"
".brand-ver{font-family:var(--mono);font-size:10.5px;font-weight:600;letter-spacing:.02em;color:var(--accent-2);"
"background:rgba(210,168,255,.10);border:1px solid rgba(210,168,255,.28);border-radius:999px;padding:1px 7px;margin-left:2px;line-height:1.4;white-space:nowrap}"
"html[data-theme=light] .brand-ver{background:rgba(130,80,223,.10);border-color:rgba(130,80,223,.30)}"
".nav-links{display:flex;gap:2px;margin-left:10px}"
".nav-links a{color:var(--text-dim);padding:7px 12px;border-radius:8px;font-size:14px;font-weight:450;transition:color .15s,background .15s;text-decoration:none}"
".nav-links a:hover{color:var(--text);background:var(--bg-elev);text-decoration:none}"
".nav-spacer{flex:1}"
".nav-cta{display:flex;align-items:center;gap:10px}"
".ghbtn{display:inline-flex;align-items:center;gap:8px;color:var(--text);background:var(--bg-elev);border:1px solid var(--border);"
"padding:7px 13px;border-radius:9px;font-size:13px;font-weight:500;transition:border-color .15s,background .15s;text-decoration:none}"
".ghbtn:hover{border-color:var(--accent);background:var(--bg-soft);text-decoration:none}"
".discordbtn:hover{border-color:#5865F2;color:#fff;background:#5865F2}.discordbtn:hover svg{color:#fff}"
".cmdk-trigger{display:inline-flex;align-items:center;gap:8px;background:var(--bg-elev);border:1px solid var(--border);color:var(--text-dim);"
"padding:7px 11px;border-radius:9px;font-size:13px;cursor:pointer;font-family:var(--sans);transition:color .15s,border-color .15s}"
".cmdk-trigger:hover{border-color:var(--accent);color:var(--text)}"
".cmdk-trigger-ico{font-family:var(--mono);font-size:13px;opacity:.7}"
".cmdk-trigger-kbd{font-family:var(--mono);font-size:11px;background:var(--bg);border:1px solid var(--border);border-radius:5px;padding:1px 5px;color:var(--text-faint)}"
".nav-toggle{display:none;background:var(--bg-elev);border:1px solid var(--border);color:var(--text);border-radius:9px;width:38px;height:36px;font-size:17px;cursor:pointer;transition:border-color .15s,background .15s,color .15s}"
".nav-toggle:hover{border-color:var(--accent);color:var(--accent)}"
".nav-toggle[aria-expanded=true]{border-color:var(--accent);color:var(--accent);background:var(--bg-soft)}"
".mobile-menu{display:none}"
"@keyframes mm-slide{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}"
"@media (max-width:720px){"
".nav-links{display:none}.cmdk-trigger{display:none}.brand-ver{display:none}"
".ghbtn{padding:7px 9px}.ghbtn span{display:none}.discordbtn span{display:none}"
".nav-cta{gap:6px}.nav-inner{gap:10px}"
".nav-toggle{display:flex;align-items:center;justify-content:center}"
".mobile-menu.open{display:flex;flex-direction:column;gap:2px;position:fixed;top:var(--nav-h);left:0;right:0;"
"max-height:calc(100dvh - var(--nav-h));overflow-y:auto;padding:14px 20px 20px;border-bottom:1px solid var(--border);"
"background:var(--bg-soft);box-shadow:0 24px 50px -18px rgba(0,0,0,.55);z-index:99;animation:mm-slide .22s cubic-bezier(.2,.8,.2,1)}"
".mobile-menu.open a{color:var(--text-dim);padding:11px 8px;font-size:15px;text-decoration:none;border-radius:8px}"
".mobile-menu.open a:hover{color:var(--text);background:var(--bg-elev)}"
"}";

// exact Discord + GitHub glyph paths from SiteNav.tsx (viewBox 0 0 16 16)
inline const char* DISCORD_PATH =
"M13.545 2.907a13.2 13.2 0 0 0-3.257-1.011.05.05 0 0 0-.052.025c-.141.25-.297.577-.406.833a12.2 12.2 0 0 0-3.658 0 8 8 0 0 0-.412-.833.05.05 0 0 0-.052-.025c-1.125.194-2.22.534-3.257 1.011a.04.04 0 0 0-.021.018C.356 6.024-.213 9.047.066 12.032q.003.022.021.037a13.3 13.3 0 0 0 3.995 2.02.05.05 0 0 0 .056-.019q.463-.63.818-1.329a.05.05 0 0 0-.01-.059l-.018-.011a9 9 0 0 1-1.248-.595.05.05 0 0 1-.02-.066l.015-.019q.127-.095.248-.195a.05.05 0 0 1 .051-.007c2.619 1.196 5.454 1.196 8.041 0a.05.05 0 0 1 .053.007q.121.1.248.195a.05.05 0 0 1-.004.085 8 8 0 0 1-1.249.594.05.05 0 0 0-.03.03.05.05 0 0 0 .003.041q.36.698.817 1.329a.05.05 0 0 0 .056.019 13.2 13.2 0 0 0 4.001-2.02.05.05 0 0 0 .021-.037c.334-3.451-.559-6.449-2.366-9.106a.03.03 0 0 0-.02-.019m-8.198 7.307c-.789 0-1.438-.724-1.438-1.612s.637-1.613 1.438-1.613c.807 0 1.45.73 1.438 1.613 0 .888-.637 1.612-1.438 1.612m5.316 0c-.788 0-1.438-.724-1.438-1.612s.637-1.613 1.438-1.613c.807 0 1.451.73 1.438 1.613 0 .888-.631 1.612-1.438 1.612";
inline const char* GITHUB_PATH =
"M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z";

inline NodeRef gh_svg(const char* path) {
    return svg(std::string("<path fill='currentColor' d='") + path + "'/>", "0 0 16 16")
        | w(16) | h(16);
}

// an external link button (.ghbtn), label hidden on mobile via CSS
inline NodeRef ghbtn(const char* extra_cls, const char* path, std::string label, std::string href_url, std::string aria) {
    auto a = row(gh_svg(path), text(std::move(label)) | as("span"))
        | add_class(std::string("ghbtn ") + extra_cls)
        | href(std::move(href_url));
    a->attrs.emplace_back("target", "_blank");
    a->attrs.emplace_back("rel", "noopener noreferrer");
    if (!aria.empty()) a->attrs.emplace_back("aria-label", aria);
    return a;
}

} // namespace nav_detail

struct NavItem { std::string title, href; };

/// `agentty::site_nav(version, items)` — the exact agentty top bar.
inline NodeRef site_nav(std::string version, std::vector<NavItem> items) {
    using namespace nav_detail;
    install_theme();
    assets().css(NAV_CSS);

    // ⌘K trigger: dispatches the palette-open event (see command palette).
    assets().script("agentty-cmdk-trigger",
        "document.addEventListener('click',function(e){var b=e.target.closest&&e.target.closest('.cmdk-trigger');"
        "if(!b)return;window.dispatchEvent(new Event('agentty:open-palette'));});"
        // \u2318K / Ctrl-K also opens it
        "document.addEventListener('keydown',function(e){if((e.metaKey||e.ctrlKey)&&(e.key==='k'||e.key==='K')){"
        "e.preventDefault();window.dispatchEvent(new Event('agentty:open-palette'));}});");
    // hamburger toggles .mobile-menu.open + aria-expanded (client-only)
    assets().script("agentty-nav-toggle",
        "document.addEventListener('click',function(e){var t=e.target.closest&&e.target.closest('.nav-toggle');"
        "if(t){var m=document.querySelector('.mobile-menu');var open=t.getAttribute('aria-expanded')==='true';"
        "t.setAttribute('aria-expanded',String(!open));t.textContent=open?'\\u2630':'\\u2715';"
        "if(m)m.classList.toggle('open',!open);return;}"
        "var ml=e.target.closest&&e.target.closest('.mobile-menu a');"
        "if(ml){var mm=document.querySelector('.mobile-menu');var tt=document.querySelector('.nav-toggle');"
        "if(mm)mm.classList.remove('open');if(tt){tt.setAttribute('aria-expanded','false');tt.textContent='\\u2630';}}});");

    // ── brand ──
    auto brand = row(
        text("\xe2\x96\x8c") | add_class("brand-mark"),
        text("agentty") | add_class("brand-name"),
        text(version) | add_class("brand-ver"))
        | add_class("brand") | href("/");
    brand->attrs.emplace_back("aria-label", "agentty home");
    auto brand_link = brand;

    // ── nav links ──
    std::vector<NodeRef> link_nodes;
    for (auto& it : items) link_nodes.push_back(link_to(it.title, it.href));
    auto links = row(); links->kids = std::move(link_nodes); finalize(*links);
    links = links | add_class("nav-links");
    links->attrs.emplace_back("aria-label", "Primary");

    // ── ⌘K trigger ──
    auto cmdk = row(
        text("\xe2\x8c\x98") | add_class("cmdk-trigger-ico"),
        text("Jump to\xe2\x80\xa6") | add_class("cmdk-trigger-label"),
        text("\xe2\x8c\x98K") | as("kbd") | add_class("cmdk-trigger-kbd"))
        | add_class("cmdk-trigger");
    cmdk->attrs.emplace_back("role", "button");
    cmdk->attrs.emplace_back("aria-label", "Open command palette");

    // ── discord + github ──
    auto discord = ghbtn("discordbtn", DISCORD_PATH, "Discord",
                         "https://discord.gg/qhb9AZ8f3c", "Join the agentty Discord");
    auto github = ghbtn("", GITHUB_PATH, "Star on GitHub",
                        "https://github.com/1ay1/agentty", "");

    auto cta = row(cmdk, theme_toggle(), discord, github) | add_class("nav-cta");

    // ── hamburger ──
    auto toggle = text("\xe2\x98\xb0") | add_class("nav-toggle");   // ☰
    toggle->kind = Kind::button;
    toggle->attrs.emplace_back("aria-label", "Toggle menu");
    toggle->attrs.emplace_back("aria-expanded", "false");

    auto inner = row(brand_link, links, box() | add_class("nav-spacer"), cta, toggle)
        | add_class("wrap nav-inner");

    // ── mobile menu ──
    // NOTE: build as a BARE box (no col()/row() flow) so it carries NO interned
    // display — the .mobile-menu / .mobile-menu.open CSS fully owns display, and
    // `display:none` isn't overridden by an interned `display:flex`.
    std::vector<NodeRef> mm;
    for (auto& it : items) mm.push_back(link_to(it.title, it.href));
    mm.push_back(link_to("Discord \xe2\x86\x97", "https://discord.gg/qhb9AZ8f3c"));
    mm.push_back(link_to("GitHub \xe2\x86\x97", "https://github.com/1ay1/agentty"));
    auto mobile = box(); mobile->kids = std::move(mm);
    mobile->attrs.emplace_back("class", "mobile-menu");
    finalize(*mobile);

    auto header = col(inner, mobile) | add_class("nav");
    header->kind = Kind::box;
    header->attrs.emplace_back("id", "top");
    finalize(*header);
    return header;
}

} // namespace agentty
