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
        // ── light-mode polish: softer real shadows (dark-mode shadows read as
        // muddy smudges on white), a gentle tinted page wash, crisper cards.
        "html[data-theme=light]{color-scheme:light}"
        "html[data-theme=light] body::before{background:"
        "radial-gradient(1100px 620px at 84% -14%,rgba(37,99,235,.10),transparent 60%),"
        "radial-gradient(820px 520px at 4% -6%,rgba(139,92,246,.09),transparent 58%),"
        "radial-gradient(700px 500px at 50% 120%,rgba(37,99,235,.05),transparent 60%)}"
        "html[data-theme=light] .card{box-shadow:0 1px 2px rgba(16,24,40,.04),0 1px 3px rgba(16,24,40,.03)}"
        "html[data-theme=light] .card:hover{box-shadow:0 12px 28px -12px rgba(37,99,235,.28),0 2px 8px rgba(16,24,40,.06)}"
        "html[data-theme=light] .bigbox{box-shadow:0 1px 2px rgba(16,24,40,.04)}"
        "html[data-theme=light] .bigbox:hover{box-shadow:0 14px 32px -14px rgba(37,99,235,.26)}"
        "html[data-theme=light] .stats{box-shadow:0 1px 2px rgba(16,24,40,.05)}"
        "html[data-theme=light] .tablewrap{box-shadow:0 1px 2px rgba(16,24,40,.04)}"
        "html[data-theme=light] thead th{background:#f4f7fb}"
        "html[data-theme=light] tbody tr:hover{background:rgba(37,99,235,.04)}"
        "html[data-theme=light] .nav{border-bottom-color:var(--border)}"
        "html[data-theme=light] .btn-primary{box-shadow:0 8px 22px -8px rgba(37,99,235,.45)}"
        "html[data-theme=light] .btn-primary:hover{box-shadow:0 12px 30px -8px rgba(37,99,235,.55)}"
        "html[data-theme=light] .install-band .copyrow{box-shadow:0 0 0 1px rgba(37,99,235,.12),0 14px 40px -24px rgba(37,99,235,.35)}"
        "html[data-theme=light] section.block:nth-of-type(even){background:linear-gradient(180deg,#f4f7fb,transparent 42%)}"
        "html[data-theme=light] .feat-cards .card.lead{background:linear-gradient(158deg,#f4f7fb,#ffffff)}"
        "html[data-theme=light] .feat-head .fg-ico{background:linear-gradient(150deg,rgba(37,99,235,.10),rgba(139,92,246,.08));border-color:rgba(37,99,235,.22)}");
}

} // namespace agentty
