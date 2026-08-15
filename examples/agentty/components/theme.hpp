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
        "--bg:#ffffff;--bg-soft:#f6f8fa;--bg-elev:#eaeef2;--bg-card:#ffffff;"
        "--border:#d0d7de;--border-soft:#e4e8ec;"
        "--text:#1f2328;--text-dim:#59636e;--text-faint:#6e7781;"
        "--accent:#0969da;--accent-hover:#0550ae;--accent-2:#8250df;--accent-warm:#9a6700;"
        "--on-accent:#ffffff;--code-bg:#0d1117;"
        "--hero-glow-1:rgba(9,105,218,0.10);--hero-glow-2:rgba(130,80,223,0.06);"
        "--nav-bg:rgba(255,255,255,0.82);--matrix-fade:rgba(255,255,255,0.12)}"
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
        "@keyframes blink{to{opacity:.3}}");
}

} // namespace agentty
