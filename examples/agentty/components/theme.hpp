#pragma once
// examples/agentty/components/theme.hpp
//
// The agentty.org design tokens — the :root custom properties from
// app/globals.css (GitHub-dark default + the [data-theme=light] overrides).
// Registered once so every agentty component can reference --accent / --text /
// --bg-elev / --mono / … exactly as the source stylesheet does. Pairs with the
// framework's theme_toggle() (which flips data-theme on <html>).

#include <waya/surface/live.hpp>

namespace agentty {

using namespace waya::surface;

/// Install the site's CSS variables (dark + light) + layout tokens. Idempotent.
inline void install_theme() {
    static bool done = false;
    if (done) return; done = true;
    assets().css(
        ":root{"
        "--bg:#0d1117;--bg-soft:#161b22;--bg-elev:#1c2128;--bg-card:#161b22;"
        "--border:#30363d;--border-soft:#21262d;"
        "--text:#e6edf3;--text-dim:#8b949e;--text-faint:#656d76;"
        "--accent:#58a6ff;--accent-hover:#79c0ff;--accent-2:#d2a8ff;--accent-warm:#d29922;"
        "--on-accent:#ffffff;--green:#3fb950;--red:#f85149;--yellow:#d29922;"
        "--code-bg:#010409;--radius:14px;"
        "--hero-glow-1:rgba(88,166,255,0.14);--hero-glow-2:rgba(210,168,255,0.08);"
        "--nav-bg:rgba(13,17,23,0.72);--matrix-fade:rgba(13,17,23,0.10);"
        "--maxw:1120px;--nav-h:64px;"
        "--mono:'JetBrains Mono',ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;"
        "--sans:'Inter',system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;"
        "--font-mono:'JetBrains Mono',ui-monospace,monospace;--font-sans:'Inter',system-ui,sans-serif}"
        "html[data-theme=light]{"
        "--bg:#fbfcfe;--bg-soft:#f4f7fb;--bg-elev:#ffffff;--bg-card:#ffffff;"
        "--border:#dbe1ea;--border-soft:#e9edf3;"
        "--text:#111826;--text-dim:#4a5568;--text-faint:#6b7688;"
        "--accent:#2563eb;--accent-hover:#1d4ed8;--accent-2:#8b5cf6;--accent-warm:#b45309;"
        "--green:#15803d;--red:#dc2626;--yellow:#b45309;"
        "--on-accent:#ffffff;--code-bg:#0d1117;"
        "--hero-glow-1:rgba(37,99,235,0.10);--hero-glow-2:rgba(139,92,246,0.09);"
        "--nav-bg:rgba(251,252,254,0.78);--matrix-fade:rgba(251,252,254,0.14)}"
        // base element styles (from globals.css). NOTE: waya's shell paints an
        // opaque html,body{background:#0d1117} in its own <style>; we set the bg
        // on <html> only and make <body> transparent so body::before's hero glow
        // (z-index:-1) is actually visible instead of being hidden under an
        // opaque body background.
        "*{box-sizing:border-box}"
        "html{scroll-behavior:smooth;scroll-padding-top:calc(var(--nav-h) + 16px);background:var(--bg)}"
        "body{margin:0;background:transparent;color:var(--text);font-family:var(--sans);font-size:16px;"
        "line-height:1.7;letter-spacing:-.011em;-webkit-font-smoothing:antialiased;text-rendering:optimizeLegibility;"
        "overflow-x:hidden;transition:color .3s ease}"
        "body::before{content:'';position:fixed;inset:0;z-index:-1;pointer-events:none;"
        "background:radial-gradient(1000px 560px at 84% -12%,var(--hero-glow-1),transparent 62%),"
        "radial-gradient(720px 460px at 6% -4%,var(--hero-glow-2),transparent 58%)}"
        "a{color:var(--accent);text-decoration:none}a:hover{color:var(--accent-hover)}"
        "h1,h2,h3,h4{line-height:1.15;font-weight:700;letter-spacing:-.025em}"
        "code,pre,kbd{font-family:var(--mono)}"
        ".wrap{width:100%;max-width:var(--maxw);margin:0 auto;padding:0 28px}"
        "@keyframes blink{to{opacity:.3}}"
        // ── light-mode polish: soft real shadows, tinted washes, crisp cards.
        "html[data-theme=light]{color-scheme:light}"
        "html[data-theme=light] body::before{background:"
        "radial-gradient(1100px 640px at 82% -16%,rgba(37,99,235,.12),transparent 58%),"
        "radial-gradient(860px 560px at 2% -8%,rgba(139,92,246,.11),transparent 56%),"
        "radial-gradient(900px 620px at 50% 118%,rgba(37,99,235,.06),transparent 60%)}"
        "html[data-theme=light] .card{background:#fff;border-color:#e6ebf2;box-shadow:0 1px 2px rgba(16,24,40,.05),0 4px 12px -6px rgba(16,24,40,.06)}"
        "html[data-theme=light] .card:hover{border-color:rgba(37,99,235,.45);box-shadow:0 16px 34px -14px rgba(37,99,235,.30),0 3px 10px rgba(16,24,40,.07)}"
        "html[data-theme=light] .bigbox{background:#fff;border-color:#e6ebf2;box-shadow:0 1px 2px rgba(16,24,40,.05),0 4px 12px -6px rgba(16,24,40,.06)}"
        "html[data-theme=light] .bigbox:hover{border-color:rgba(37,99,235,.45);box-shadow:0 18px 38px -16px rgba(37,99,235,.28)}"
        "html[data-theme=light] .stats{background:#e6ebf2;box-shadow:0 4px 16px -8px rgba(16,24,40,.10)}"
        "html[data-theme=light] .tablewrap{box-shadow:0 4px 16px -10px rgba(16,24,40,.10)}"
        "html[data-theme=light] thead th{background:#eef2f8;color:#4a5568}"
        "html[data-theme=light] tbody tr:hover{background:rgba(37,99,235,.045)}"
        "html[data-theme=light] td .win{color:#2563eb}"
        "html[data-theme=light] .nav{border-bottom-color:#e2e8f2}"
        "html[data-theme=light] .btn-primary{box-shadow:0 8px 22px -8px rgba(37,99,235,.45)}"
        "html[data-theme=light] .btn-primary:hover{box-shadow:0 12px 30px -8px rgba(37,99,235,.6)}"
        "html[data-theme=light] .btn-ghost{background:#fff;border-color:#dbe1ea}"
        "html[data-theme=light] .btn-ghost:hover{border-color:#2563eb;background:#f4f7fb}"
        "html[data-theme=light] .install-band .copyrow{box-shadow:0 0 0 1px rgba(37,99,235,.14),0 18px 46px -22px rgba(37,99,235,.40)}"
        "html[data-theme=light] section.block{border-top-color:#e9edf3}"
        "html[data-theme=light] section.block:nth-of-type(even){background:linear-gradient(180deg,#f2f6fc,transparent 44%)}"
        "html[data-theme=light] .feat-cards .card.lead{background:linear-gradient(158deg,#eef3fb,#ffffff);border-color:rgba(37,99,235,.30)}"
        "html[data-theme=light] .feat-head{border-bottom-color:#e9edf3}"
        "html[data-theme=light] .feat-head .fg-ico{background:linear-gradient(150deg,rgba(37,99,235,.12),rgba(139,92,246,.10));border-color:rgba(37,99,235,.24)}"
        "html[data-theme=light] .eyebrow{color:#2563eb}"
        "html[data-theme=light] .quote{border-left-color:#2563eb}"
        "html[data-theme=light] .quote::before{color:rgba(37,99,235,.22)}"
        "html[data-theme=light] .cta-mesh{background:"
        "radial-gradient(40% 50% at 20% 30%,rgba(37,99,235,.16),transparent 60%),"
        "radial-gradient(38% 46% at 80% 25%,rgba(139,92,246,.14),transparent 60%),"
        "radial-gradient(45% 50% at 60% 80%,rgba(139,92,246,.13),transparent 62%)}"
        "html[data-theme=light] .foot{border-top-color:#e9edf3}");
}

} // namespace agentty
