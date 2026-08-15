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
".ttui .b{font-weight:700}.ttui .i{font-style:italic}"
"@keyframes ttui-in{from{opacity:0;transform:translateY(3px)}to{opacity:1;transform:translateY(0)}}"
".ttui-fade{animation:ttui-in .22s ease-out both}.ttui-evgroup{animation:ttui-in .18s ease-out both}"
"@media (prefers-reduced-motion:reduce){.ttui-fade,.ttui-evgroup{animation:none}}"
".term-cursor{background:#6fe0ee;color:#010409}"
"@media (max-width:640px){.ttui-body{font-size:11px;padding:14px;height:440px}.ttui-status{font-size:10.5px}.ttui{margin-left:0;margin-right:0}}";

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
            s("dim b", " A C T I O N S  \xc2\xb7  4/4 "),
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
    scroll->attrs.emplace_back("data-tui-scroll", "");   // hook: client animation rewrites this

    // ── COMPOSER ──
    auto composer = col(
        line("ttui-comp-top", s("dim", "\xe2\x95\xad"), s("dim ttui-fill-bot"), s("dim", "\xe2\x95\xae")),
        line("ttui-comp-mid",
            s("dim", "\xe2\x94\x82 "), s("bmag b", "\xe2\x9d\xaf "), s("dim", "type a message\xe2\x80\xa6"),
            s("ttui-comp-right"), s("dim", " \xe2\x94\x82")),
        line("ttui-comp-bot", s("dim", "\xe2\x95\xb0"), s("dim ttui-fill-bot"), s("dim", "\xe2\x95\xaf"))
    ) | add_class("ttui-composer");

    // ── STATUS BAR (phase ready) ──
    auto status_accent_top = box() | add_class("ttui-accent dim");
    status_accent_top->attrs.emplace_back("data-tui-accent", "");
    auto status_left = line("ttui-status-left",
            s("", " "), s("cyan", "\xe2\x96\x8e"), s("white", " refactor auth"),
            s("dim", "   \xc2\xb7   "),
            s("dim", "\xe2\x96\x8c"), s("", " "), s("dim", "\xe2\x97\x8f"), s("dim b", " Ready"));
    status_left->attrs.emplace_back("data-tui-status-left", "");
    auto status_right = line("ttui-status-right",
            s("yellow", "\xe2\x9a\xa1 "), s("cyan", "  0.0"), s("cyan", " t/s "),
            s("dim", "\xe2\x96\x81\xe2\x96\x81\xe2\x96\x82\xe2\x96\x81\xe2\x96\x83\xe2\x96\x82\xe2\x96\x81\xe2\x96\x81\xe2\x96\x82\xe2\x96\x81\xe2\x96\x82\xe2\x96\x83\xe2\x96\x82\xe2\x96\x81\xe2\x96\x81\xe2\x96\x81"));
    status_right->attrs.emplace_back("data-tui-status-right", "");
    auto status_accent_bot = box() | add_class("ttui-accent dim");
    status_accent_bot->attrs.emplace_back("data-tui-accent", "");
    auto status = col(
        status_accent_top,
        line("ttui-status", status_left, status_right),
        status_accent_bot
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

    // The animation is a 1:1 replica of AgenttyTui.tsx's state machine, run as
    // ONE client effect against the SSR'd DOM (which already shows the settled
    // frame, so no-JS/crawlers see a complete panel). It holds the same EVENTS /
    // PROSE / schedule() and rebuilds the .ttui-scroll innerHTML per state — the
    // way React re-renders — plus the pointer tilt+glare. No Model state, no
    // server tick: pure client-owned decoration, armed on scroll-in + idle,
    // paused off-screen, looping while visible. Inert under reduced-motion.
    const char* anim_js =
        "const card=el.querySelector('.ttui');const scr=el.querySelector('[data-tui-scroll]');"
        "const sL=el.querySelector('[data-tui-status-left]');const sR=el.querySelector('[data-tui-status-right]');"
        "const accs=el.querySelectorAll('[data-tui-accent]');"
        "const RM=matchMedia&&matchMedia('(prefers-reduced-motion:reduce)').matches;"
        // ── pointer tilt + glare (source's own CSS-var effect) ──
        "if(!(matchMedia&&matchMedia('(pointer:coarse)').matches)&&!RM){"
        "let raf=0,tx=0,ty=0,gx=50,gy=0,gl=0;"
        "function apply(){raf=0;card.style.setProperty('--rx',ty.toFixed(2)+'deg');"
        "card.style.setProperty('--ry',tx.toFixed(2)+'deg');card.style.setProperty('--gx',gx.toFixed(1)+'%');"
        "card.style.setProperty('--gy',gy.toFixed(1)+'%');card.style.setProperty('--glare',gl.toFixed(2));}"
        "function q(){if(!raf)raf=requestAnimationFrame(apply);}"
        "el.addEventListener('pointermove',function(e){const r=card.getBoundingClientRect();"
        "const px=(e.clientX-r.left)/r.width,py=(e.clientY-r.top)/r.height;const M=7;"
        "tx=(px-0.5)*2*M;ty=-(py-0.5)*2*M;gx=px*100;gy=py*100;gl=1;q();});"
        "el.addEventListener('pointerleave',function(){tx=0;ty=0;gx=50;gy=0;gl=0;q();});}"
        // ── the animated session (skipped under reduced-motion: settled frame stays) ──
        "if(RM)return;"
        // exact source data
        "const SPIN=['\u280b','\u2819','\u2839','\u2838','\u283c','\u2834','\u2826','\u2827','\u2807','\u280f'];"
        "const EV=["
        "{name:'Read',detail:'src/auth/handler.cpp  \u00b7  214 lines',cat:'inspect',elapsed:'142ms',ec:'dim',run:700},"
        "{name:'Grep',detail:'TokenCache  \u00b7  3 matches',cat:'inspect',elapsed:' 89ms',ec:'dim',run:600},"
        "{name:'Edit',detail:'src/auth/handler.cpp  (+18 \u22129)',cat:'mutate',elapsed:'  6ms',ec:'green',run:900,"
        "body:[{c:'dim',t:'@@ resolve(id) @@'},{c:'red',t:'- return fetch_remote(id);'},{c:'green',t:'+ if (auto v = cache.lookup(id)) return *v;'}]},"
        "{name:'Bash',detail:'cmake --build build -j',cat:'execute',elapsed:'  3.6s',ec:'yellow',run:1400,body:[{c:'dim',t:'[100%] Built target agentty'}]}];"
        "const PROSE='Auth handler now resolves through TokenCache::lookup, falling back to a network refresh only on a miss. Build is green.';"
        "const CAT={inspect:'cyan',mutate:'mag',execute:'cyan'};"
        "const esc=s=>s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');"
        // schedule(): cumulative offsets, matching source
        "function sched(){const st=[];let t=1100;EV.forEach((e,i)=>{st.push({at:t,run:i});t+=e.run;st.push({at:t,done:i});t+=120;});return{steps:st,settleAt:t};}"
        // state
        "let running=-1,done=-1,frame=0,proseLen=0,phase='idle',userTyped=false,timers=[],spinId=0;"
        "function px(ms,fn){timers.push(setTimeout(fn,ms));}"
        "function clearAll(){timers.forEach(clearTimeout);timers=[];if(spinId){clearInterval(spinId);spinId=0;}}"
        // render(): rebuild .ttui-scroll innerHTML from state (mirrors JSX)
        "function proseHTML(txt){const r='TokenCache::lookup';const i=txt.indexOf(r);"
        "if(i<0)return '<span class=\"bwhite\">'+esc(txt)+'</span>';"
        "return '<span class=\"bwhite\">'+esc(txt.slice(0,i))+'</span><span class=\"bcyan\">'+esc(txt.slice(i,i+r.length))+'</span><span class=\"bwhite\">'+esc(txt.slice(i+r.length))+'</span>';}"
        "function prow(inner){return '<div class=\"ttui-panel-line\"><span class=\"ttui-edge dim\">\u2502</span><span class=\"ttui-pad\">'+inner+'</span><span class=\"ttui-edge dim\">\u2502</span></div>';}"
        // FLICKER-FREE: only re-write scroll HTML when the STRUCTURE changes
        // (new event / done / prose char / phase). The 90ms spinner tick calls
        // tickSpin() to patch only .ttui-spin text nodes in place — no reflow,
        // no fade-keyframe re-fire per frame (React-style reconciliation).
        "let lastSig='';"
        "function tickSpin(){const g=SPIN[frame];scr.querySelectorAll('.ttui-spin').forEach(e=>{e.textContent=g;});"
        "const e=sL.querySelector('.ttui-spin');if(e)e.textContent=g;}"
        "function render(){"
        "const total=EV.length,doneCount=done+1,allDone=doneCount===total;"
        "const visible=Math.max(running+1,doneCount),showPanel=running>=0;"
        "const seen=EV.slice(0,visible);"
        "const insp=seen.filter(e=>e.cat==='inspect').length,mut=seen.filter(e=>e.cat==='mutate').length,exe=seen.filter(e=>e.cat==='execute').length;"
        "let h='';"
        // user turn
        "h+='<div class=\"ttui-turn rail-mag\"><div class=\"row ttui-head\"><span class=\"mag\">\u276f</span><span> </span><span class=\"mag b\">You</span><span class=\"ttui-meta dim\">12:34</span></div><div class=\"row ttui-blank\"></div><div class=\"row\"><span class=\"bwhite\">refactor the auth handler to use the new token cache</span>'+(!userTyped?'<span class=\"term-cursor\"> </span>':'')+'</div></div>';"
        // assistant turn
        "if(userTyped){h+='<div class=\"ttui-turn rail-bmag\"><div class=\"row ttui-head\"><span class=\"bmag\">\u2726</span><span> </span><span class=\"bmag b\">Opus 4.5</span><span class=\"ttui-meta dim\">12:34  \u00b7  '+(allDone?'4.2s':'\u2026')+'  \u00b7  turn 3</span></div>';"
        "if(showPanel){"
        "h+='<div class=\"row ttui-blank\"></div><div class=\"ttui-panel\">';"
        "h+='<div class=\"ttui-panel-top\"><span class=\"dim\">\u256d\u2500</span><span class=\"dim b\"> A C T I O N S  \u00b7  '+doneCount+'/'+total+' </span><span class=\"dim ttui-fill\"></span><span class=\"dim b\"> 4.2s </span><span class=\"dim\">\u2500\u256e</span></div>';"
        "h+=prow('<span class=\"cyan b\">I N S P E C T</span><span class=\"white\"> '+insp+'</span><span class=\"dim\">  \u00b7  </span><span class=\"mag b\">M U T A T E</span><span class=\"white\"> '+mut+'</span><span class=\"dim\">  \u00b7  </span><span class=\"cyan b\">E X E C U T E</span><span class=\"white\"> '+exe+'</span>');"
        "h+=prow('');"
        "for(let i=0;i<visible;i++){const ev=EV[i],isDone=i<=done,isLast=i===total-1;"
        "const glyph=total===1?'\u2500\u2500':i===0?'\u256d\u2500':isLast?'\u2570\u2500':'\u251c\u2500';const c=CAT[ev.cat];"
        "const icon=isDone?'<span class=\"bgreen b\">\u2713</span>':'<span class=\"bcyan b ttui-spin\">'+SPIN[frame]+'</span>';"
        "const dd=isDone?'dim':'';"
        "let rowh='<span class=\"'+c+' dim\">'+glyph+'</span><span> </span>'+icon+'<span>  </span><span class=\"'+c+' '+dd+' b\">'+ev.name+'</span><span>  </span><span class=\"'+c+' '+dd+' i\">'+esc(isDone?ev.detail:'running\u2026')+'</span>'+(isDone?'<span class=\"ttui-elapsed '+ev.ec+'\">'+esc(ev.elapsed)+'</span>':'');"
        "h+=prow(rowh);"
        "if(isDone&&ev.body)ev.body.forEach(b=>{h+=prow('<span class=\"'+c+' dim\">   \u2502  </span><span class=\"'+b.c+'\">'+esc(b.t)+'</span>');});"
        "if(!isLast){const nd=(i+1)<=done;h+=prow('<span class=\"'+(nd?'dim':(c+' dim'))+'\">   \u2502</span>');}}"
        "if(allDone){h+=prow('');h+=prow('<span>   </span><span class=\"bgreen b\">\u2713 </span><span class=\"bgreen b\">D O N E</span><span class=\"white\">   4 actions   4.2s</span>');}"
        "h+='<div class=\"ttui-panel-bot\"><span class=\"dim\">\u2570</span><span class=\"dim ttui-fill-bot\"></span><span class=\"dim\">\u256f</span></div></div>';}"
        // prose is patched in place (its own target) so per-char typing never
        // rebuilds the panel above it; render its container once, fill later.
        "if(proseLen>0||phase!=='idle'&&userTyped&&allDone){h+='<div class=\"row ttui-blank\"></div><div class=\"row ttui-prose\" data-tui-prose></div>';}"
        "h+='</div>';}"
        // structural signature EXCLUDES proseLen: prose is patched separately
        "const hasProse=proseLen>0;"
        "const sig=running+'|'+done+'|'+phase+'|'+userTyped+'|'+hasProse;"
        "const changed=sig!==lastSig;"
        "if(changed){lastSig=sig;scr.innerHTML=h;}else{tickSpin();}"
        // prose: update only the prose element's text, never the panel above it
        "const pel=scr.querySelector('[data-tui-prose]');"
        "if(pel){pel.innerHTML=proseHTML(PROSE.slice(0,proseLen))+(phase!=='ready'?'<span class=\"term-cursor\"> </span>':'');}"
        // status bar is only rewritten on a STRUCTURAL change (phase flip etc.);
        // its spinner is patched by tickSpin() in place, so it never flickers.
        "if(!changed)return;"
        "const streaming=phase==='stream';"
        "accs.forEach(a=>{a.className='ttui-accent '+(streaming?'bcyan':'dim');});"
        "if(streaming){const nm=(allDone&&proseLen>0)?'Streaming':(EV[running]?EV[running].name:'Streaming');"
        "sL.innerHTML='<span> </span><span class=\"cyan\">\u258e</span><span class=\"white\"> refactor auth</span><span class=\"dim\">   \u00b7   </span><span class=\"bcyan b\">\u258c</span><span> </span><span class=\"bcyan b ttui-spin\">'+SPIN[frame]+'</span><span class=\"bcyan b\"> '+nm+'</span>';"
        "sR.innerHTML='<span class=\"yellow\">\u26a1 </span><span class=\"cyan\">78.3</span><span class=\"cyan\"> t/s </span><span class=\"cyan\">\u2581\u2582\u2584\u2586\u2587\u2585\u2583\u2582\u2581\u2582\u2584\u2586\u2587\u2585\u2583\u2582\u2581</span>';}"
        "else{sL.innerHTML='<span> </span><span class=\"cyan\">\u258e</span><span class=\"white\"> refactor auth</span><span class=\"dim\">   \u00b7   </span><span class=\"dim\">\u258c</span><span> </span><span class=\"dim\">\u25cf</span><span class=\"dim b\"> Ready</span>';"
        "sR.innerHTML='<span class=\"yellow\">\u26a1 </span><span class=\"cyan\">  0.0</span><span class=\"cyan\"> t/s </span><span class=\"dim\">\u2581\u2581\u2582\u2581\u2583\u2582\u2581\u2581\u2582\u2581\u2582\u2583\u2582\u2581\u2581\u2581</span>';}}"
        // master loop
        "function run(){running=-1;done=-1;proseLen=0;phase='idle';userTyped=false;render();"
        "px(300,()=>{userTyped=true;render();});px(700,()=>{phase='stream';render();"
        "if(!spinId)spinId=setInterval(()=>{frame=(frame+1)%10;tickSpin();},90);});"
        "const{steps,settleAt}=sched();steps.forEach(s=>{px(s.at,()=>{if(s.run!==undefined)running=s.run;if(s.done!==undefined)done=s.done;render();});});"
        "const ps=settleAt+250;for(let i=1;i<=PROSE.length;i++)px(ps+i*11,()=>{proseLen=i;render();});"
        "const pe=ps+PROSE.length*11;px(pe+150,()=>{phase='ready';if(spinId){clearInterval(spinId);spinId=0;}render();});"
        "px(pe+4200,()=>{clearAll();if(!document.hidden)run();else{const rz=()=>{if(!document.hidden){document.removeEventListener('visibilitychange',rz);run();}};document.addEventListener('visibilitychange',rz);}});}"
        // arm on scroll-in + idle
        "let armed=false;const io=new IntersectionObserver(es=>{es.forEach(e=>{if(e.isIntersecting&&!armed){armed=true;io.disconnect();"
        "const go=()=>run();if(window.requestIdleCallback)requestIdleCallback(go,{timeout:1800});else setTimeout(go,900);}});},{threshold:.25});io.observe(el);";

    return (box(card) | add_class("ttui-stage") | client_effect("agentty-tui", anim_js));
}

} // namespace agentty
