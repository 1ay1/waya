#pragma once
// examples/agentty/components/tui.hpp
//
// AgenttyTui — a faithful replica of the agentty TUI (the demo panel on the
// right of the hero). 1:1 port of components/AgenttyTui.tsx + agentty-tui.css.
//
// The React component *animates* a session; we render the SETTLED FINAL FRAME
// it converges to (phase "ready": all four tool events done, prose fully typed,
// status bar Ready). Every glyph, colour, class and layout row is lifted
// verbatim from the source so the rendered DOM is byte-for-byte the same shape
// as the React output's resting state. The real agentty-tui.css is registered
// as-is; nodes carry the same class names via add_class(), so styling matches
// exactly. The pointer-tilt + glare are the source's own CSS-var effect, driven
// by one small client_effect (no Model state, no server tick).

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <string>
#include <vector>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

namespace tui_detail {

// The agentty-tui.css, verbatim from the source. Registered once.
inline const char* TUI_CSS =
".ttui-stage{perspective:1600px;perspective-origin:50% 30%}"
".ttui{position:relative;z-index:9999;background:#010409;border:1px solid #30363d;border-radius:12px;"
"overflow:hidden;box-shadow:0 40px 100px -24px rgba(0,0,0,.75),0 0 0 1px rgba(255,255,255,.02);"
"max-width:780px;margin:56px auto 0;transform:rotateX(var(--rx,0deg)) rotateY(var(--ry,0deg)) translateZ(0);"
"transform-style:preserve-3d;transition:transform .45s cubic-bezier(.2,.8,.2,1),box-shadow .45s ease;will-change:transform}"
".ttui-stage:hover .ttui{box-shadow:0 50px 120px -30px rgba(0,0,0,.85),0 0 0 1px rgba(88,166,255,.18),0 0 60px -20px rgba(88,166,255,.35)}"
".ttui-glare{position:absolute;inset:0;z-index:5;pointer-events:none;border-radius:inherit;opacity:var(--glare,0);"
"transition:opacity .3s ease;background:radial-gradient(600px circle at var(--gx,50%) var(--gy,0%),"
"rgba(255,255,255,.10),rgba(88,166,255,.06) 35%,transparent 60%);mix-blend-mode:screen}"
"@media (prefers-reduced-motion:reduce){.ttui{transition:none;transform:none}.ttui-glare{display:none}}"
".ttui-bar{display:flex;align-items:center;gap:7px;padding:10px 14px;background:#0d1117;border-bottom:1px solid #30363d}"
".ttui-dot{width:11px;height:11px;border-radius:50%}"
".ttui-dot.r{background:#ff5f56}.ttui-dot.y{background:#ffbd2e}.ttui-dot.g{background:#27c93f}"
".ttui-title{margin-left:8px;color:#656d76;font-family:var(--mono,ui-monospace,monospace);font-size:12px}"
".ttui-body{padding:18px 20px 14px;font-family:var(--font-mono,'JetBrains Mono',ui-monospace,monospace);"
"font-size:12.5px;line-height:1.65;color:#e6edf3;height:540px;display:flex;flex-direction:column;overflow:hidden}"
".ttui-body .row{white-space:pre-wrap;word-break:break-word}"
".ttui-blank{height:.7em}"
".ttui-scroll{flex:1 1 auto;min-height:0;overflow:hidden;display:flex;flex-direction:column;justify-content:flex-end}"
".ttui-turn{flex:none;border-left:2px solid transparent;padding-left:16px;margin-bottom:2px}"
".ttui-turn.rail-mag{border-color:#c586c0}.ttui-turn.rail-bmag{border-color:#e29be0}"
".ttui-chrome{flex:none;margin-top:auto;padding-top:12px}"
".ttui-head{display:flex;align-items:baseline;white-space:pre}"
".ttui-meta{margin-left:auto;padding-left:18px}"
".ttui-panel{font-size:12px;line-height:1.7}"
".ttui-panel-top,.ttui-panel-bot{display:flex;white-space:pre;align-items:center}"
".ttui-panel-top .ttui-fill{flex:1;border-top:1px solid #656d76;align-self:center;height:0;opacity:.55}"
".ttui-panel-bot .ttui-fill-bot,.ttui-comp-top .ttui-fill-bot,.ttui-comp-bot .ttui-fill-bot{flex:1;border-top:1px solid #656d76;align-self:center;height:0;opacity:.55}"
".ttui-panel-line{display:flex;white-space:pre;align-items:baseline}"
".ttui-panel-line .ttui-pad{flex:1;padding:0 8px;min-width:0;display:flex;align-items:baseline;overflow:hidden}"
".ttui-edge{flex:none}"
".ttui-elapsed{margin-left:auto;padding-left:16px;white-space:pre}"
".ttui-composer{font-size:12px}"
".ttui-comp-top,.ttui-comp-bot{display:flex;white-space:pre;align-items:center}"
".ttui-comp-mid{display:flex;align-items:baseline;white-space:pre}"
".ttui-comp-right{flex:1}"
".ttui-accent{height:0;border-top:1px solid currentColor;opacity:.4;margin:2px 0}"
".ttui-status{display:flex;align-items:center;justify-content:space-between;gap:12px;font-size:12px;white-space:pre;overflow:hidden}"
".ttui-status-left,.ttui-status-right{display:flex;align-items:center;white-space:pre;flex:none;min-width:0}"
".ttui-status-right{margin-left:auto}"
".ttui .mag{color:#c586c0}.ttui .bmag{color:#e29be0}.ttui .cyan{color:#4ec9d8}.ttui .bcyan{color:#6fe0ee}"
".ttui .blue{color:#61afef}.ttui .green{color:#98c379}.ttui .bgreen{color:#7ee787}.ttui .yellow{color:#e5c07b}"
".ttui .red{color:#e06c75}.ttui .white{color:#aab1bd}.ttui .bwhite{color:#e6edf3}.ttui .dim{color:#656d76}"
".ttui .b{font-weight:700}.ttui .i{font-style:italic}";

// A styled inline span: <span class="…">text</span>. `cls` is space-separated
// class names from the source; text is escaped by construction (text node).
inline NodeRef s(std::string cls, std::string txt) {
    auto n = text(std::move(txt));
    if (!cls.empty()) n = n | add_class(std::move(cls));
    return n;
}
inline NodeRef s(std::string cls) { return box() | add_class(std::move(cls)); }  // empty filler span

// a .row / .ttui-* line: a flex box of spans with the given class.
template <typename... Kids>
inline NodeRef line(std::string cls, Kids... kids) {
    auto n = row(std::move(kids)...);
    return n | add_class(std::move(cls));
}

// PanelRow: │ <padded content> │
template <typename... Kids>
inline NodeRef panel_row(Kids... kids) {
    return line("ttui-panel-line",
        s("ttui-edge dim", "\xe2\x94\x82"),
        row(std::move(kids)...) | add_class("ttui-pad"),
        s("ttui-edge dim", "\xe2\x94\x82"));
}

} // namespace tui_detail

/// `agentty::tui()` — the hero's terminal demo, the settled frame of a live
/// agentty session (identical to the React component at rest).
inline NodeRef tui() {
    using namespace tui_detail;
    assets().css(TUI_CSS);

    // pointer tilt + glare: the source's own CSS-var effect, one client_effect.
    // Sets --rx/--ry (lean) and --gx/--gy/--glare (sheen) from the pointer.
    const char* tilt_js =
        "if(matchMedia&&(matchMedia('(pointer:coarse)').matches||matchMedia('(prefers-reduced-motion:reduce)').matches))return;"
        "const card=el.querySelector('.ttui');let raf=0,tx=0,ty=0,gx=50,gy=0,gl=0;"
        "function apply(){raf=0;card.style.setProperty('--rx',ty.toFixed(2)+'deg');"
        "card.style.setProperty('--ry',tx.toFixed(2)+'deg');card.style.setProperty('--gx',gx.toFixed(1)+'%');"
        "card.style.setProperty('--gy',gy.toFixed(1)+'%');card.style.setProperty('--glare',gl.toFixed(2));}"
        "function q(){if(!raf)raf=requestAnimationFrame(apply);}"
        "el.addEventListener('pointermove',function(e){const r=card.getBoundingClientRect();"
        "const px=(e.clientX-r.left)/r.width,py=(e.clientY-r.top)/r.height;"
        "tx=(px-0.5)*10;ty=-(py-0.5)*8;gx=px*100;gy=py*100;gl=0.85;q();});"
        "el.addEventListener('pointerleave',function(){tx=ty=0;gx=50;gy=0;gl=0;q();});";

    // ── USER TURN (rail-mag) ──
    auto user_turn = col(
        line("row ttui-head",
            s("mag", "\xe2\x9d\xaf"), s("", " "), s("mag b", "You"),
            s("ttui-meta dim", "12:34")),
        box() | add_class("row ttui-blank"),
        line("row", s("bwhite", "refactor the auth handler to use the new token cache"))
    ) | add_class("ttui-turn rail-mag");

    // ── ACTIONS PANEL (all four events done) ──
    // stats: inspect=2, mutate=1, execute=1
    auto stats_line = panel_row(
        s("cyan b", "I N S P E C T"), s("white", " 2"),
        s("dim", "  \xc2\xb7  "), s("mag b", "M U T A T E"), s("white", " 1"),
        s("dim", "  \xc2\xb7  "), s("cyan b", "E X E C U T E"), s("white", " 1"));

    // events: connector glyph (cat-color + dim), ✓, name (cat dim b),
    // detail (cat dim i), elapsed. cat: inspect/execute=cyan, mutate=mag.
    auto ev_read = col(
        panel_row(s("cyan dim", "\xe2\x95\xad\xe2\x94\x80"), s("", " "), s("bgreen b", "\xe2\x9c\x93"),
            s("", "  "), s("cyan dim b", "Read"), s("", "  "),
            s("cyan dim i", "src/auth/handler.cpp  \xc2\xb7  214 lines"),
            s("ttui-elapsed dim", "142ms")),
        panel_row(s("cyan", "   \xe2\x94\x82"))   // connector, next done → dim... source uses nextDone?dim:blue; all done → dim
    );
    auto ev_grep = col(
        panel_row(s("cyan dim", "\xe2\x94\x9c\xe2\x94\x80"), s("", " "), s("bgreen b", "\xe2\x9c\x93"),
            s("", "  "), s("cyan dim b", "Grep"), s("", "  "),
            s("cyan dim i", "TokenCache  \xc2\xb7  3 matches"),
            s("ttui-elapsed dim", " 89ms")),
        panel_row(s("dim", "   \xe2\x94\x82"))
    );
    auto ev_edit = col(
        panel_row(s("mag dim", "\xe2\x94\x9c\xe2\x94\x80"), s("", " "), s("bgreen b", "\xe2\x9c\x93"),
            s("", "  "), s("mag dim b", "Edit"), s("", "  "),
            s("mag dim i", "src/auth/handler.cpp  (+18 \xe2\x88\x92 9)"),
            s("ttui-elapsed green", "  6ms")),
        panel_row(s("mag dim", "   \xe2\x94\x82  "), s("dim", "@@ resolve(id) @@")),
        panel_row(s("mag dim", "   \xe2\x94\x82  "), s("red", "- return fetch_remote(id);")),
        panel_row(s("mag dim", "   \xe2\x94\x82  "), s("green", "+ if (auto v = cache.lookup(id)) return *v;")),
        panel_row(s("dim", "   \xe2\x94\x82"))
    );
    auto ev_bash = col(
        panel_row(s("cyan dim", "\xe2\x95\xb0\xe2\x94\x80"), s("", " "), s("bgreen b", "\xe2\x9c\x93"),
            s("", "  "), s("cyan dim b", "Bash"), s("", "  "),
            s("cyan dim i", "cmake --build build -j"),
            s("ttui-elapsed yellow", "  3.6s")),
        panel_row(s("cyan dim", "   \xe2\x94\x82  "), s("dim", "[100%] Built target agentty"))
        // last event: no trailing connector
    );

    auto panel = col(
        line("ttui-panel-top",
            s("dim", "\xe2\x95\xad\xe2\x94\x80"),
            s("dim b ttui-cap", " A C T I O N S  \xc2\xb7  4/4 "),
            s("dim ttui-fill"),
            s("dim b", " 4.2s "),
            s("dim", "\xe2\x94\x80\xe2\x95\xae")),
        stats_line,
        panel_row(box() | add_class("ttui-pad")),   // blank line
        ev_read, ev_grep, ev_edit, ev_bash,
        // footer (allDone)
        panel_row(box() | add_class("ttui-pad")),
        panel_row(s("", "   "), s("bgreen b", "\xe2\x9c\x93 "), s("bgreen b", "D O N E"),
            s("white", "   4 actions   4.2s")),
        line("ttui-panel-bot", s("dim", "\xe2\x95\xb0"), s("dim ttui-fill-bot"), s("dim", "\xe2\x95\xaf"))
    ) | add_class("ttui-panel");

    // prose (fully typed): "…through " + TokenCache::lookup (bcyan) + " …"
    const std::string prose_pre  = "Auth handler now resolves through ";
    const std::string prose_ref  = "TokenCache::lookup";
    const std::string prose_post = ", falling back to a network refresh only on a miss. Build is green.";
    auto prose = line("row ttui-prose",
        s("bwhite", prose_pre), s("bcyan", prose_ref), s("bwhite", prose_post));

    auto assistant_turn = col(
        line("row ttui-head",
            s("bmag", "\xe2\x9c\xa6"), s("", " "), s("bmag b", "Opus 4.5"),
            s("ttui-meta dim", "12:34  \xc2\xb7  4.2s  \xc2\xb7  turn 3")),
        box() | add_class("row ttui-blank"),
        panel,
        box() | add_class("row ttui-blank"),
        prose
    ) | add_class("ttui-turn rail-bmag");

    auto scroll = col(user_turn, assistant_turn) | add_class("ttui-scroll");

    // ── COMPOSER ──
    auto composer = col(
        line("ttui-comp-top", s("dim", "\xe2\x95\xad"), s("dim ttui-fill-bot"), s("dim", "\xe2\x95\xae")),
        line("ttui-comp-mid",
            s("dim", "\xe2\x94\x82 "), s("bmag b", "\xe2\x9d\xaf "), s("dim", "type a message\xe2\x80\xa6"),
            s("ttui-comp-right"), s("dim", " \xe2\x94\x82")),
        line("ttui-comp-bot", s("dim", "\xe2\x95\xb0"), s("dim ttui-fill-bot"), s("dim", "\xe2\x95\xaf"))
    ) | add_class("ttui-composer");

    // ── STATUS BAR (phase ready) ──
    auto status = col(
        box() | add_class("ttui-accent dim"),
        line("ttui-status",
            line("ttui-status-left",
                s("", " "), s("cyan", "\xe2\x96\x8e"), s("white", " refactor auth"),
                s("dim", "   \xc2\xb7   "),
                s("dim", "\xe2\x96\x8c"), s("", " "), s("dim", "\xe2\x97\x8f"), s("dim b", " Ready")),
            line("ttui-status-right",
                s("yellow", "\xe2\x9a\xa1 "), s("cyan", "  0.0"), s("cyan", " t/s "),
                s("dim", "\xe2\x96\x81\xe2\x96\x81\xe2\x96\x82\xe2\x96\x81\xe2\x96\x83\xe2\x96\x82\xe2\x96\x81\xe2\x96\x81\xe2\x96\x82\xe2\x96\x81\xe2\x96\x82\xe2\x96\x83\xe2\x96\x82\xe2\x96\x81\xe2\x96\x81\xe2\x96\x81"),
                s("dim", "   \xc2\xb7   "),
                s("bmag", "\xe2\x97\x8f "), s("bmag", "Opus 4.5"), s("dim", " \xc2\xb7 "),
                s("green", "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88"), s("dim", "\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91 38%"), s("", " "))),
        box() | add_class("ttui-accent dim")
    );

    auto chrome = col(composer, status) | add_class("ttui-chrome");

    auto body = col(scroll, chrome) | add_class("ttui-body");

    auto bar = line("ttui-bar",
        box() | add_class("ttui-dot r"),
        box() | add_class("ttui-dot y"),
        box() | add_class("ttui-dot g"),
        s("ttui-title", "agentty \xe2\x80\x94 ~/projects/app"));

    auto card = col(
        box() | add_class("ttui-glare"),
        bar,
        body
    ) | add_class("ttui");

    return (box(card) | add_class("ttui-stage") | client_effect("agentty-tui-tilt", tilt_js));
}

} // namespace agentty
