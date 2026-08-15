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
        "--on-accent:#0d1117;"
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
        "--on-accent:#ffffff;"
        "--nav-bg:rgba(255,255,255,0.82);--matrix-fade:rgba(255,255,255,0.12)}"
        "@keyframes blink{to{opacity:.3}}");
}

} // namespace agentty
