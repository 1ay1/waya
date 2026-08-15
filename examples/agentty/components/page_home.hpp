#pragma once
// examples/agentty/components/page_home.hpp
//
// The agentty.org homepage — a faithful, structure-for-structure port of
// app/page.tsx + the homepage rules from globals.css. The real stylesheet
// sections are registered verbatim (HOME_CSS) and the DOM is built with the
// exact class names, so the page matches the Next.js output section by section:
//   HERO (2-col grid: text+logo left, TUI right) · INSTALL BAND · STATS (5) ·
//   SPEED table · FEATURE GROUPS (categorized bento) · PROVIDERS · COMPARE ·
//   TOOLS · QUOTE · OPEN SOURCE (3 bigbox) · CTA (animated mesh).
//
// Rich inline copy (with <code>/<strong>/<a>) is emitted via markup() — trusted,
// author-authored static content, not user data. Reveal/magnetic/count-up/
// typewriter come from the framework primitives.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include "theme.hpp"
#include "logo.hpp"
#include "hero_background.hpp"
#include "tui.hpp"

#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

namespace home_detail {

inline const char* HOME_CSS =
// buttons
".btn{display:inline-flex;align-items:center;gap:8px;justify-content:center;padding:12px 24px;border-radius:11px;"
"font-weight:600;font-size:15px;border:1px solid transparent;cursor:pointer;transition:transform .18s cubic-bezier(.2,.9,.2,1),background .15s,border-color .15s,box-shadow .2s;text-decoration:none}"
".btn:hover{text-decoration:none;transform:translateY(-2px)}.btn:active{transform:translateY(0)}"
".btn-primary{background:var(--accent);color:var(--on-accent);box-shadow:0 10px 34px -8px rgba(88,166,255,.55)}"
".btn-primary:hover{background:var(--accent-hover);color:var(--on-accent);box-shadow:0 14px 40px -8px rgba(88,166,255,.7)}"
".btn-ghost{background:var(--bg-elev);border-color:var(--border);color:var(--text)}"
".btn-ghost:hover{border-color:var(--accent);background:var(--bg-soft)}"
// hero
".hero{padding:40px 0 0;position:relative;overflow:hidden}.hero .wrap{position:relative;z-index:1}"
".hero-grid{display:grid;grid-template-columns:minmax(360px,1fr) minmax(0,1.05fr);gap:56px;align-items:start}"
".hero-inner{position:relative;z-index:1;display:flex;flex-direction:column;align-items:flex-start;text-align:left}"
".hero-logo{margin:-16px 0 22px;max-width:100%}"
".hero-inner .lede{margin-left:0;margin-right:0}.hero-inner .hero-actions{justify-content:flex-start}"
".hero-tui{min-width:0}.hero-tui .ttui{margin:-8px 0 0;max-width:100%;width:100%;position:relative}"
".hero-tui .ttui::before{content:'';position:absolute;inset:-1px;border-radius:13px;padding:1px;"
"background:linear-gradient(135deg,rgba(88,166,255,.55),rgba(210,168,255,.35),transparent 60%);"
"-webkit-mask:linear-gradient(#000 0 0) content-box,linear-gradient(#000 0 0);-webkit-mask-composite:xor;"
"mask:linear-gradient(#000 0 0) content-box,linear-gradient(#000 0 0);mask-composite:exclude;pointer-events:none;opacity:.8}"
".hero h1{font-size:clamp(36px,5.4vw,56px);margin:0 0 20px;letter-spacing:-.045em;line-height:1.05}"
".hero h1 .grad{background:linear-gradient(110deg,var(--accent) 8%,var(--accent-2) 48%,var(--accent-hover) 90%);"
"-webkit-background-clip:text;background-clip:text;color:transparent}"
".lede{font-size:clamp(17px,2.1vw,20px);color:var(--text-dim);max-width:600px;margin:0 0 34px;line-height:1.6}"
".lede strong{color:var(--text);font-weight:600}"
".hero-actions{display:flex;gap:14px;flex-wrap:wrap;margin-bottom:34px}"
"@media (max-width:960px){.hero-grid{grid-template-columns:1fr;gap:40px}.hero-inner{align-items:center;text-align:center}"
".hero-logo{align-self:center}.hero-inner .lede{margin-left:auto;margin-right:auto}.hero-inner .hero-actions{justify-content:center}"
".hero-tui .ttui{max-width:780px;margin:0 auto}}"
// copyrow
".copyrow{display:flex;align-items:center;gap:10px;max-width:600px;background:var(--code-bg);border:1px solid var(--border);"
"border-radius:12px;padding:13px 15px;font-family:var(--mono);font-size:14px;color:var(--text)}"
".copyrow .prompt{color:var(--accent);user-select:none}.copyrow code{flex:1;overflow-x:auto;white-space:nowrap}"
".copybtn{background:var(--bg-elev);border:1px solid var(--border);color:var(--text-dim);border-radius:7px;padding:5px 11px;"
"font-size:12px;cursor:pointer;font-family:var(--sans);transition:color .15s,border-color .15s}"
".copybtn:hover{color:var(--text);border-color:var(--accent)}"
".copyrow-caret{display:inline-block;width:7px;height:1.05em;margin-left:1px;vertical-align:text-bottom;background:var(--accent);animation:cr-blink 1s steps(2,start) infinite}"
"@keyframes cr-blink{to{opacity:0}}"
// install band
".install-band{padding:0 0 8px;margin-top:-8px}"
".install-inner{display:flex;flex-direction:column;align-items:center;text-align:center}"
".install-kicker{color:var(--text-faint);font-size:12px;font-weight:600;letter-spacing:.12em;text-transform:uppercase;margin:0 0 14px}"
".install-band .copyrow{width:100%;max-width:660px;box-shadow:0 0 0 1px rgba(88,166,255,.10),0 18px 50px -28px rgba(88,166,255,.45)}"
".install-band .copyrow code{font-size:13.5px}"
".install-note{color:var(--text-dim);font-size:14px;margin:16px 0 0}"
".install-note code{background:var(--bg-elev);border:1px solid var(--border-soft);border-radius:5px;padding:1px 6px;font-size:.86em;color:var(--accent-2)}"
// sections
"section.block{padding:76px 0;border-top:1px solid var(--border-soft);position:relative}"
"section.block:nth-of-type(even){background:linear-gradient(180deg,var(--bg-soft),transparent 40%)}"
".eyebrow{display:inline-flex;align-items:center;gap:9px;color:var(--accent);font-weight:600;font-size:13px;letter-spacing:.1em;text-transform:uppercase}"
".eyebrow::before{content:'';width:22px;height:1px;background:linear-gradient(90deg,var(--accent),transparent)}"
".section-title{font-size:clamp(28px,4vw,38px);margin:12px 0 16px;letter-spacing:-.03em}"
".section-sub{color:var(--text-dim);font-size:17px;max-width:600px;margin:0 0 40px;line-height:1.6}"
// cards
".card{position:relative;background:var(--bg-card);border:1px solid var(--border);border-radius:var(--radius);padding:26px;overflow:hidden;"
"transition:border-color .25s ease,transform .25s cubic-bezier(.2,.8,.2,1),background .25s ease,box-shadow .25s ease}"
".card::after{content:'';position:absolute;inset:0;border-radius:inherit;background:radial-gradient(420px 180px at 50% -40%,rgba(88,166,255,.10),transparent 70%);opacity:0;transition:opacity .25s ease;pointer-events:none}"
".card:hover{border-color:rgba(88,166,255,.4);transform:translateY(-4px);background:var(--bg-soft);box-shadow:0 22px 48px -28px rgba(88,166,255,.45)}"
".card:hover::after{opacity:1}"
".card h3{font-size:17px;margin:0 0 8px;letter-spacing:-.02em}.card p{color:var(--text-dim);margin:0;font-size:14.5px;line-height:1.6}"
// feature groups
".feat-groups{display:flex;flex-direction:column;gap:48px;margin-top:24px}"
".feat-head{display:flex;align-items:center;gap:13px;margin:0 0 20px;padding-bottom:15px;border-bottom:1px solid var(--border-soft)}"
".feat-head .fg-ico{font-size:17px;line-height:1;width:40px;height:40px;flex-shrink:0;display:inline-flex;align-items:center;justify-content:center;"
"background:linear-gradient(150deg,rgba(88,166,255,.14),rgba(210,168,255,.08));border:1px solid rgba(88,166,255,.28);border-radius:11px}"
".feat-head h3{font-size:15px;font-weight:700;letter-spacing:.02em;margin:0;color:var(--text);text-transform:uppercase}"
".feat-cards{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;grid-auto-rows:1fr}"
".feat-cards .card{padding:22px}"
".feat-cards .card h4{font-size:16px;margin:0 0 8px;letter-spacing:-.02em;color:var(--text)}"
".feat-cards .card p{color:var(--text-dim);margin:0;font-size:14px;line-height:1.6}"
".feat-cards .card.lead{grid-column:span 2;grid-row:span 2;background:linear-gradient(158deg,var(--bg-elev),var(--bg-card));border-color:rgba(88,166,255,.28)}"
".feat-cards .card.lead::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;background:linear-gradient(90deg,var(--accent),var(--accent-2),transparent);border-radius:var(--radius) var(--radius) 0 0}"
".feat-cards .card.lead h4{font-size:20px;margin-bottom:10px}.feat-cards .card.lead p{font-size:15px;color:var(--text-dim)}"
"@media (max-width:820px){.feat-cards{grid-template-columns:1fr}.feat-cards .card.lead{grid-column:span 1;grid-row:span 1}}"
// tables
".tablewrap{overflow-x:auto;border:1px solid var(--border);border-radius:var(--radius)}"
"table{width:100%;border-collapse:collapse;font-size:14.5px}"
"th,td{text-align:left;padding:14px 18px;border-bottom:1px solid var(--border-soft)}"
"thead th{background:var(--bg-soft);color:var(--text-faint);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.04em}"
"tbody tr{transition:background .15s}tbody tr:hover{background:rgba(88,166,255,.025)}tbody tr:last-child td{border-bottom:none}"
"td .win{color:var(--accent);font-weight:600}td.mono,th.mono{font-family:var(--mono)}"
// stats
".stats{display:grid;grid-template-columns:repeat(5,1fr);gap:1px;background:var(--border-soft);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden}"
".stat{background:var(--bg-card);padding:28px 20px;text-align:center;transition:background .2s ease}.stat:hover{background:var(--bg-soft)}"
".stat-link{display:block;color:inherit;text-decoration:none}.stat-link:hover{background:var(--bg-soft)}.stat-link:hover .num{filter:brightness(1.15)}"
".stat .num{font-size:32px;font-weight:800;letter-spacing:-.03em;background:linear-gradient(120deg,var(--accent),var(--accent-2));-webkit-background-clip:text;background-clip:text;color:transparent;font-variant-numeric:tabular-nums;min-height:1.05em}"
".stat .lbl{color:var(--text-dim);font-size:13.5px;margin-top:6px}"
".stats-note{text-align:center;color:var(--text-faint);font-size:12.5px;margin:14px 0 0}"
".stats-note a{color:var(--text-dim);text-decoration:underline;text-underline-offset:2px}"
"@media (max-width:720px){.stats{grid-template-columns:repeat(2,1fr)}}"
// quote
".quote{position:relative;border-left:3px solid var(--accent);padding:8px 0 8px 28px;font-size:22px;line-height:1.5;color:var(--text);max-width:760px;letter-spacing:-.01em}"
".quote::before{content:'\\201d';position:absolute;left:14px;top:-18px;font-size:64px;line-height:1;color:rgba(88,166,255,.18);font-family:Georgia,serif}"
".quote .by{display:block;font-size:15px;color:var(--text-faint);margin-top:14px;letter-spacing:0}"
// bigbox
".boxrow{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:18px}"
".bigbox{background:var(--bg-card);border:1px solid var(--border);border-radius:var(--radius);padding:28px;transition:border-color .25s ease,transform .25s cubic-bezier(.2,.8,.2,1),box-shadow .25s ease}"
".bigbox:hover{border-color:rgba(88,166,255,.4);transform:translateY(-3px);box-shadow:0 22px 48px -28px rgba(88,166,255,.4)}"
".bigbox h3{margin:0 0 8px;font-size:19px}.bigbox p{color:var(--text-dim);margin:0 0 16px;font-size:14.5px}"
// tilt
".card.tilt,.bigbox.tilt{transition:border-color .25s ease,background .25s ease,box-shadow .3s ease,transform .3s cubic-bezier(.2,.8,.2,1);transform-style:preserve-3d}"
".card.tilt:hover,.bigbox.tilt:hover{transform:perspective(900px) rotateX(4deg) rotateY(-4deg) translateY(-6px) scale(1.015)}"
"@media (prefers-reduced-motion:reduce){.card.tilt:hover,.bigbox.tilt:hover{transform:translateY(-3px)}}"
// cta
".cta{text-align:center;padding:80px 0;position:relative;overflow:hidden}"
".cta h2{font-size:clamp(28px,4vw,42px);margin:0 0 12px}.cta p{color:var(--text-dim);font-size:18px;margin:0 0 28px}"
".cta-mesh{position:absolute;inset:-20% -10%;z-index:0;pointer-events:none;"
"background:radial-gradient(40% 50% at 20% 30%,rgba(88,166,255,.18),transparent 60%),"
"radial-gradient(38% 46% at 80% 25%,rgba(210,168,255,.14),transparent 60%),"
"radial-gradient(45% 50% at 60% 80%,rgba(210,168,255,.16),transparent 62%);filter:blur(28px);will-change:transform;animation:mesh-drift 18s ease-in-out infinite alternate}"
".cta > .wrap{position:relative;z-index:1}"
"@keyframes mesh-drift{from{transform:translate3d(0,0,0) scale(1)}to{transform:translate3d(4%,-3%,0) scale(1.12) rotate(2deg)}}"
"@media (prefers-reduced-motion:reduce){.cta-mesh{animation:none}}"
// content offset for the fixed nav
"#main{padding-top:var(--nav-h)}";

// a raw-HTML block with a class (trusted author markup)
inline NodeRef html(std::string cls, std::string inner) {
    auto n = markup(std::move(inner));
    if (!cls.empty()) n = n | add_class(std::move(cls));
    return n;
}

// CopyRow.tsx: `$ <cmd>` + copy button. `typed` types the command out on
// scroll-in via the framework typewriter(); clicking dispatches copy_msg.
template <typename CopyMsg>
inline NodeRef copy_row(std::string cmd, CopyMsg copy_msg, bool typed) {
    auto code = text(cmd);
    if (typed) code = code | typewriter();
    auto btn = text("copy") | add_class("copybtn") | as("button") | tap(copy_msg);
    return row(
        text("$") | add_class("prompt"),
        code | as("code"),
        btn
    ) | add_class("copyrow");
}

// a .card.tilt with an <h4> title + rich <p> body; `lead` spans 2 cols
inline NodeRef feat_card(std::string title, std::string body_html, bool lead = false) {
    return html(lead ? "card tilt lead" : "card tilt",
        "<h4>" + title + "</h4><p>" + body_html + "</p>") | reveal();
}

// a feature group: icon + UPPERCASE heading, then the 3-up card grid
inline NodeRef feat_group(std::string icon, std::string heading, std::vector<NodeRef> cards) {
    auto head = html("feat-head", "<span class=\"fg-ico\">" + icon + "</span><h3>" + heading + "</h3>");
    auto grid = box(); grid->kids = std::move(cards); grid->attrs.emplace_back("class", "feat-cards"); finalize(*grid);
    return box(head, grid) | add_class("feat-group") | reveal();
}

// a bordered data table from raw <tbody>/<thead> HTML
inline NodeRef data_table(std::string inner_html) {
    return html("tablewrap", "<table>" + inner_html + "</table>");
}

// a .block section: eyebrow + title + optional sub + body nodes
template <typename... Body>
inline NodeRef section_block(std::string id, std::string eyebrow, std::string title,
                             std::string sub, Body... body) {
    std::string head = "<p class=\"eyebrow\">" + eyebrow + "</p>"
        "<h2 class=\"section-title\">" + title + "</h2>";
    if (!sub.empty()) head += "<p class=\"section-sub\">" + sub + "</p>";
    auto inner = box(markup(head), std::move(body)...) | add_class("wrap");
    auto sec = box(inner) | as("section") | add_class("block") | reveal();
    if (!id.empty()) sec->attrs.emplace_back("id", id);
    return sec;
}

} // namespace home_detail

/// `agentty::home()` — the full agentty.org homepage, faithful to page.tsx.
template <typename CopyMsg>
NodeRef home(std::string install_cmd, CopyMsg copy_msg) {
    using namespace home_detail;
    install_theme();
    assets().css(HOME_CSS);

    const std::string SIZE = "13\xc2\xa0MB";   // stats.sizeMB with nbsp

    // ── HERO ──
    auto hero_inner = box(
        box(logo()) | add_class("hero-logo"),
        markup("<h1>Blazing-fast <span class=\"grad\">coding agent</span><br/> in your terminal.</h1>"),
        markup("<p class=\"lede\">A drop-in alternative to <code>claude-code</code>, written in C++26. "
               "<strong>13&nbsp;MB binary</strong>, <strong>millisecond cold start</strong>, "
               "<strong>sandboxed by default</strong>, SSH air-gap in one command, and "
               "<strong>runs inside Zed</strong> over ACP. Signs in with your existing "
               "<strong>Claude Pro/Max</strong> &mdash; or point it at OpenAI, Groq, OpenRouter, "
               "Cerebras, or a local Ollama.</p>"),
        row(
            html("", "<a class=\"btn btn-primary\" href=\"/docs/quick-start\">Get started</a>") | magnetic(),
            html("", "<a class=\"btn btn-ghost\" href=\"https://github.com/1ay1/agentty\" target=\"_blank\" rel=\"noopener noreferrer\">Star on GitHub &rarr;</a>") | magnetic())
            | add_class("hero-actions")
    ) | add_class("hero-inner");

    auto hero_tui = box(tui()) | add_class("hero-tui");

    auto hero = box(
        hero_background(0x58a6ff),
        box(hero_inner, hero_tui) | add_class("wrap hero-grid")
    ) | as("section") | add_class("hero");

    // ── INSTALL BAND ──
    auto install = box(
        box(
            markup("<p class=\"install-kicker\">Install in one line</p>"),
            copy_row(install_cmd, copy_msg, /*typed=*/true),
            markup("<p class=\"install-note\">Works on Linux, macOS &amp; Windows \xc2\xb7 x86_64 &amp; aarch64. "
                   "Or grab a <code>release binary</code>.</p>")
        ) | add_class("wrap install-inner")
    ) | as("section") | add_class("install-band");

    // ── STATS ──
    auto stat = [](std::string num, std::string lbl) {
        return box(text(num) | add_class("num") | count_up(), text(lbl) | add_class("lbl")) | add_class("stat");
    };
    auto stats = box(
        box(
            stat("13 MB", "static binary"),
            stat("2 ms", "cold start"),
            stat("0", "runtimes to install"),
            stat("7", "model providers"),
            html("stat stat-link", "<div class=\"num\">\xe2\x98\x85</div><div class=\"lbl\">Star on GitHub</div>")
                | href("https://github.com/1ay1/agentty")
        ) | add_class("stats"),
        markup("<p class=\"stats-note\">Measured on the same box \xc2\xb7 "
               "<a href=\"https://github.com/1ay1/agentty\">see the benchmarks</a></p>")
    ) | add_class("wrap") | detail::raw_css("padding-top", "48px");

    // ── SPEED ──
    auto speed = section_block("speed", "Speed", "Native, not interpreted.",
        "Measured on the same box, same shell, same day. No JIT warmup, no require() graph "
        "to walk, no GC ticking while bytes stream in.",
        data_table(
            "<thead><tr><th></th><th>agentty (C++26)</th><th>claude-code (Node)</th></tr></thead><tbody>"
            "<tr><td>Cold-start <code>--help</code></td><td><span class=\"win\">~2 ms</span></td><td>~150 ms</td></tr>"
            "<tr><td><code>--version</code></td><td><span class=\"win\">~2 ms</span></td><td>~60 ms</td></tr>"
            "<tr><td>Binary on disk</td><td><span class=\"win\">13 MB</span></td><td>222 MB + Node</td></tr>"
            "<tr><td>Install</td><td><span class=\"win\">curl | sh</span></td><td>npm i -g + Node</td></tr>"
            "<tr><td>GC pauses mid-stream</td><td><span class=\"win\">None</span></td><td>V8 GC</td></tr>"
            "</tbody>"));

    // ── FEATURES ──
    auto features = box(box(
        markup("<p class=\"eyebrow\">Why agentty</p>"
               "<h2 class=\"section-title\">Everything the official client does &mdash; and the things it doesn't.</h2>"
               "<p class=\"section-sub\">A full coding agent, not a thin wrapper. Grouped by what you came for.</p>"),
        box(
            feat_group("\xe2\x9a\xa1", "Performance &amp; footprint", {
                feat_card("Native speed", "C++26, statically linked, <code>posix_spawn</code> everywhere. Spawns in microseconds, no GC pauses mid-stream, no warmup.", true),
                feat_card("One static binary", "13 MB. <code>curl | chmod +x | run</code>. No Node runtime, no <code>npm install</code>, no version drift."),
                feat_card("Inline render", "Lives at the bottom of your terminal, preserves scrollback, never takes over the screen.") }),
            feat_group("\xf0\x9f\x94\x8c", "Models &amp; auth", {
                feat_card("Any model", "Claude by default via your Pro/Max subscription &mdash; or GPT, Groq, OpenRouter, Together, Cerebras, and local Ollama. Switch live with <code>^P</code>. <a href=\"/docs/providers\">Providers &rarr;</a>", true),
                feat_card("Adjustable reasoning", "Dial thinking effort per model &mdash; fast for small edits, deep for hard refactors."),
                feat_card("Paste images", "Drop a PNG/JPEG/GIF/WebP path or <code>^V</code> from the clipboard &mdash; inline to the model.") }),
            feat_group("\xf0\x9f\x9b\xa1\xef\xb8\x8f", "Safety &amp; isolation", {
                feat_card("Sandbox by default", "Every shell and build call runs inside <code>bwrap</code> (Linux) / <code>sandbox-exec</code> (macOS). An approved bash call still can't <code>cat ~/.ssh/id_rsa</code>.", true),
                feat_card("Permission profiles", "Start in <strong>Ask</strong>; <code>S-Tab</code> cycles to <strong>Write</strong> or <strong>Minimal</strong>. <a href=\"/docs/profiles\">Profiles &rarr;</a>"),
                feat_card("Workspace boundary", "Filesystem tools refuse paths outside the launch directory. Opt out with <code>--workspace /</code>.") }),
            feat_group("\xf0\x9f\xa7\xa0", "Workflow &amp; memory", {
                feat_card("Learns your codebase", "Agent Skills teach it your conventions; <code>remember</code>/<code>forget</code> give durable memory; <code>search_docs</code> runs a local <a href=\"/docs/retrieval\">retrieval engine</a> &mdash; hybrid BM25 + dense, HNSW, GraphRAG. <a href=\"/docs/skills\">Skills &rarr;</a>", true),
                feat_card("Threads that persist", "Every conversation is a saved thread you reopen with <code>^J</code>. Long threads compact automatically."),
                feat_card("Isolated subagents", "The <code>task</code> tool spawns a subagent with its own context window, returns one condensed report."),
                feat_card("Rewind to any checkpoint", "Every user turn in a git repo pins a worktree snapshot. <code>Enter</code> rewinds files <em>and</em> transcript.") }),
            feat_group("\xf0\x9f\xa7\xa9", "Reach &amp; extensibility", {
                feat_card("One-command SSH air-gap", "<code>agentty airgap user@host</code> runs the agent on a box with no direct internet &mdash; SOCKS5-over-SSH, TLS pinned end-to-end.", true),
                feat_card("Runs inside Zed (ACP)", "<code>agentty acp</code> speaks the Agent Client Protocol &mdash; a first-class agent panel in Zed. <a href=\"/docs/acp\">Set it up &rarr;</a>"),
                feat_card("MCP, both ways", "Serve agentty's tools with <code>mcp-serve</code>, or consume other MCP servers from <code>.agentty/mcp.json</code>. <a href=\"/docs/mcp\">MCP &rarr;</a>") })
        ) | add_class("feat-groups")
    ) | add_class("wrap")) | as("section") | add_class("block") | attr("id", "features");

    // ── PROVIDERS ──
    auto providers = section_block("providers", "Bring your own model", "Claude by default. Any model on demand.",
        "Sign in once with your Claude Pro/Max subscription, or point agentty at any "
        "OpenAI-compatible backend. Switch live mid-thread with ^P.",
        data_table("<tbody>"
            "<tr><td class=\"mono\"><code>agentty</code></td><td>Claude via OAuth (Pro/Max) or API key &mdash; the default</td></tr>"
            "<tr><td class=\"mono\"><code>--provider openai</code></td><td>GPT and o-series on <code>api.openai.com</code></td></tr>"
            "<tr><td class=\"mono\"><code>--provider groq</code></td><td>Llama/Mixtral on Groq LPUs &mdash; very fast</td></tr>"
            "<tr><td class=\"mono\"><code>--provider openrouter</code></td><td>Any model via <code>openrouter.ai</code></td></tr>"
            "<tr><td class=\"mono\"><code>--provider together</code></td><td>Open models on <code>together.ai</code></td></tr>"
            "<tr><td class=\"mono\"><code>--provider cerebras</code></td><td>Wafer-scale inference &mdash; very fast</td></tr>"
            "<tr><td class=\"mono\"><code>--provider ollama</code></td><td>Local models at <code>localhost:11434</code> &mdash; no key, no cloud</td></tr>"
            "<tr><td class=\"mono\"><code>--provider host:port</code></td><td>Any raw OpenAI-compatible endpoint</td></tr>"
            "</tbody>"));

    // ── COMPARE ──
    auto compare = section_block("compare", "How it compares", "The single-binary pick.", "",
        data_table(
            "<thead><tr><th></th><th>agentty</th><th>claude-code</th><th>aider</th></tr></thead><tbody>"
            "<tr><td>Language / runtime</td><td><span class=\"win\">C++26 &mdash; static binary</span></td><td>TypeScript / Node</td><td>Python</td></tr>"
            "<tr><td>Footprint</td><td><span class=\"win\">13 MB</span></td><td>npm + Node runtime</td><td>pip + Python runtime</td></tr>"
            "<tr><td>Platforms</td><td><span class=\"win\">Linux \xc2\xb7 macOS \xc2\xb7 Windows</span></td><td>Linux \xc2\xb7 macOS \xc2\xb7 Windows</td><td>Linux \xc2\xb7 macOS \xc2\xb7 Windows</td></tr>"
            "<tr><td>Air-gapped mode</td><td><span class=\"win\">Yes (SOCKS5/SSH)</span></td><td>No</td><td>No</td></tr>"
            "<tr><td>Editor integration (ACP)</td><td><span class=\"win\">Yes (Zed)</span></td><td>Yes (Zed)</td><td>No</td></tr>"
            "<tr><td>Models</td><td><span class=\"win\">Claude \xc2\xb7 GPT \xc2\xb7 Groq \xc2\xb7 OpenRouter \xc2\xb7 Together \xc2\xb7 Cerebras \xc2\xb7 Ollama</span></td><td>Claude (Anthropic)</td><td>many providers</td></tr>"
            "</tbody>"));

    // ── TOOLS ──
    auto tools = section_block("tools", "Tools", "A purpose-built widget for everything.",
        "Diffs render as diffs, search groups by file, bash shows exit codes, todos become checklists.",
        data_table("<tbody>"
            "<tr><td class=\"mono\"><code>read \xc2\xb7 write \xc2\xb7 edit</code></td><td>File IO with atomic writes and diff rendering</td></tr>"
            "<tr><td class=\"mono\"><code>grep \xc2\xb7 glob \xc2\xb7 list_dir \xc2\xb7 find_definition</code></td><td>Search, listing, and symbol lookup across the codebase</td></tr>"
            "<tr><td class=\"mono\"><code>bash \xc2\xb7 diagnostics</code></td><td>Sandboxed shell and build, with exit codes</td></tr>"
            "<tr><td class=\"mono\"><code>git_status \xc2\xb7 git_diff \xc2\xb7 git_log \xc2\xb7 git_commit</code></td><td>Version control, rendered natively</td></tr>"
            "<tr><td class=\"mono\"><code>web_fetch \xc2\xb7 web_search</code></td><td>Reach the web for docs and APIs</td></tr>"
            "<tr><td class=\"mono\"><code>search_docs \xc2\xb7 search_code \xc2\xb7 skill \xc2\xb7 task</code></td><td>Local RAG, semantic code search, on-demand skills, and isolated subagents</td></tr>"
            "<tr><td class=\"mono\"><code>todo \xc2\xb7 remember \xc2\xb7 forget \xc2\xb7 wipe_memory</code></td><td>Planning and durable cross-session memory</td></tr>"
            "</tbody>"));

    // ── QUOTE ──
    auto quote = box(box(markup(
        "<blockquote class=\"quote\">&ldquo;No JIT warmup, no <code>require()</code> graph to walk, "
        "no GC ticking while bytes stream in from the API. The redraw loop is a <code>poll(2)</code> "
        "over the model stream and your input fd &mdash; every keystroke lands on the next frame.&rdquo;"
        "<span class=\"by\">&mdash; from the design notes</span></blockquote>")) | add_class("wrap"))
        | as("section") | add_class("block") | reveal();

    // ── OPEN SOURCE ──
    auto opensource = box(box(
        markup("<p class=\"eyebrow\">Open source</p><h2 class=\"section-title\">Built in the open, MIT licensed.</h2>"),
        box(
            html("bigbox tilt", "<h3>Read the source</h3><p>The reducer is one <code>std::visit</code> over a closed event sum; the view is a single <code>Model &rarr; Element</code> function; the permission matrix is a <code>constexpr</code> with <code>static_assert</code>s.</p><a class=\"btn btn-ghost\" href=\"https://github.com/1ay1/agentty\" target=\"_blank\" rel=\"noopener noreferrer\">Browse the repo &rarr;</a>") | reveal(),
            html("bigbox tilt", "<h3>Get involved</h3><p>Bug reports, fixes, and well-scoped features are all welcome. Start with the contributing guide.</p><a class=\"btn btn-ghost\" href=\"/contributing\">How to contribute &rarr;</a>") | reveal(),
            html("bigbox tilt", "<h3>Join the community</h3><p>Hang out in the Discord to ask questions and get help &mdash; there's an AI helper bot that answers agentty questions using the real agent.</p><a class=\"btn btn-primary\" href=\"https://discord.gg/qhb9AZ8f3c\" target=\"_blank\" rel=\"noopener noreferrer\">Join the Discord &rarr;</a>") | reveal()
        ) | add_class("boxrow") | detail::raw_css("margin-top", "28px")
    ) | add_class("wrap")) | as("section") | add_class("block") | attr("id", "open-source");

    // ── CTA ──
    auto cta = box(
        box() | add_class("cta-mesh"),
        box(
            markup("<h2>Ready in one line.</h2><p>Linux, macOS &amp; Windows \xc2\xb7 x86_64 &amp; aarch64. The same line updates it.</p>"),
            box(copy_row(install_cmd, copy_msg, true)) | detail::raw_css("max-width", "620px") | detail::raw_css("margin", "0 auto"),
            row(
                html("", "<a class=\"btn btn-primary\" href=\"/docs/quick-start\">Quick start guide</a>") | magnetic(),
                html("", "<a class=\"btn btn-ghost\" href=\"https://discord.gg/qhb9AZ8f3c\" target=\"_blank\" rel=\"noopener noreferrer\">Join the Discord &rarr;</a>") | magnetic(),
                html("", "<a class=\"btn btn-ghost\" href=\"https://github.com/1ay1/agentty\" target=\"_blank\" rel=\"noopener noreferrer\">Star on GitHub &rarr;</a>") | magnetic())
                | add_class("hero-actions") | detail::raw_css("justify-content", "center") | detail::raw_css("margin-top", "26px")
        ) | add_class("wrap")
    ) | as("section") | add_class("cta");

    return box(hero, install, stats, speed, features, providers, compare, tools, quote, opensource, cta)
        | as("main") | attr("id", "main");
}

} // namespace agentty
