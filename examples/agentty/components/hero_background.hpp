#pragma once
// examples/agentty/components/hero_background.hpp
//
// HeroBackground — the geeky hero backdrop. 1:1 port of
// components/HeroBackground.tsx + hero-background.css:
//   • a canvas "digital rain" of monospace glyphs (the exact source algorithm)
//   • a faint blueprint dot/line lattice (CSS)
//   • a drifting, breathing brand glow (CSS)
//   • CRT scanlines (CSS)
//
// The rain is the source's exact draw loop — same GLYPHS/COLORS, FONT=15,
// per-column drops/speeds, theme-aware --matrix-fade trail, 30fps cap, paused
// off-screen (IntersectionObserver) and when the tab is hidden, re-reads its
// fade colour on data-theme toggle. Shipped as one client_effect: a
// browser-owned rAF loop, zero server cost per frame, no Model state.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <string>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

/// `agentty::matrix_rain()` — the canvas rain effect as a Mod you can drop on any
/// full-size box. It appends its own <canvas class="hero-bg-canvas"> and runs the
/// exact source draw loop against it.
inline Mod matrix_rain() {
    // --matrix-fade is theme-scoped (dark vs light); register both.
    assets().root_var("--matrix-fade", "rgba(13, 17, 23, 0.10)");
    assets().css("[data-theme=light]{--matrix-fade:rgba(255,255,255,0.12)}");

    // GLYPHS + COLORS verbatim from the source (\u-escaped for the C++ string).
    const std::string js =
        "const GLYPHS=('01{}[]()<>/=+-*&|!?;:.#$@_\\u03bb\\u2192\\u2234\\u27e8\\u27e9\\u2207\\u2202\\u2211"
        "01\\u30a2\\u30a4\\u30a6\\u30a8\\u30aa\\u30ab\\u30ad\\u30af\\u30b1\\u30b3\\u2593\\u2592\\u2591"
        "\\u2571\\u2572\\u2503\\u2501\\u250f\\u2513\\u2517\\u251b').split('');"
        "const COLORS=['#58a6ff','#d2a8ff','#79c0ff','#656d76'];"
        // append the canvas (matches the source's <canvas class=hero-bg-canvas/>)
        "const canvas=document.createElement('canvas');canvas.className='hero-bg-canvas';"
        "el.appendChild(canvas);const ctx=canvas.getContext('2d');if(!ctx)return;"
        "const reduced=window.matchMedia&&matchMedia('(prefers-reduced-motion:reduce)').matches;"
        "const FONT=15;let cols=0,drops=[],speeds=[],dpr=1;"
        "const mono=(getComputedStyle(document.documentElement).getPropertyValue('--font-mono').trim())||'monospace';"
        "let fade='rgba(13,17,23,0.10)';"
        "function readFade(){fade=(getComputedStyle(document.documentElement).getPropertyValue('--matrix-fade').trim())||'rgba(13,17,23,0.10)';}"
        "readFade();"
        "function resize(){dpr=Math.min(window.devicePixelRatio||1,2);"
        "const w=canvas.clientWidth,h=canvas.clientHeight;"
        "canvas.width=Math.floor(w*dpr);canvas.height=Math.floor(h*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);"
        "cols=Math.ceil(w/FONT);"
        "drops=new Array(cols).fill(0).map(()=>Math.random()*-40);"
        "speeds=new Array(cols).fill(0).map(()=>0.25+Math.random()*0.55);"
        "ctx.font=FONT+'px '+mono+', monospace';}"
        "function frame(){const w=canvas.clientWidth,h=canvas.clientHeight;"
        "ctx.fillStyle=fade;ctx.fillRect(0,0,w,h);"
        "for(let i=0;i<cols;i++){const x=i*FONT,y=drops[i]*FONT;"
        "const g=GLYPHS[(Math.random()*GLYPHS.length)|0];"
        "const lead=Math.random()<0.04;"
        "ctx.fillStyle=lead?'#e6edf3':COLORS[(i+(drops[i]|0))%COLORS.length];"
        "ctx.globalAlpha=lead?0.9:0.42;ctx.fillText(g,x,y);ctx.globalAlpha=1;"
        "if(y>h&&Math.random()>0.975)drops[i]=Math.random()*-20;drops[i]+=speeds[i];}}"
        "resize();window.addEventListener('resize',resize);"
        "new ResizeObserver(resize).observe(canvas);"   // (waya): the box sizes late; re-fit
        // re-read theme colours on data-theme toggle
        "new MutationObserver(readFade).observe(document.documentElement,{attributes:true,attributeFilter:['data-theme']});"
        "let raf=0,last=0;const FRAME_MS=1000/30;let visible=true,active=!document.hidden;"
        "const io=new IntersectionObserver(([e])=>{visible=e.isIntersecting;},{threshold:0});io.observe(canvas);"
        "document.addEventListener('visibilitychange',()=>{active=!document.hidden;});"
        "if(reduced){for(let p=0;p<60;p++)frame();}"
        "else{const loop=(now)=>{raf=requestAnimationFrame(loop);"
        "if(!visible||!active)return;if(now-last<FRAME_MS)return;last=now;frame();};raf=requestAnimationFrame(loop);}";
    return client_effect("agentty-rain", js);
}

/// `agentty::hero_background(accent)` — the full four-layer backdrop, positioned
/// absolute to fill its (positioned) parent. Matches hero-background.css exactly.
inline NodeRef hero_background(std::uint32_t accent = 0x58a6ff) {
    assets().keyframes("agentty-hero-glow",
        "from{transform:translate(0,0) scale(1);opacity:.8}"
        "to{transform:translate(120px,60px) scale(1.12);opacity:1}");
    // .hero-bg-canvas — the source's own class (position/size/opacity)
    assets().css(".hero-bg-canvas{position:absolute;inset:0;width:100%;height:100%;opacity:0.5}"
                 "@media (max-width:640px){.hero-bg-canvas{opacity:0.32}}");

    auto grid = box() | absolute() | detail::raw_css("inset", "-2px")
        | detail::raw_css("background-image",
            "linear-gradient(" + detail::rgba_hex(accent, 0.045f) + " 1px, transparent 1px),"
            "linear-gradient(90deg," + detail::rgba_hex(accent, 0.045f) + " 1px, transparent 1px)")
        | detail::raw_css("background-size", "44px 44px")
        | detail::raw_css("mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)")
        | detail::raw_css("-webkit-mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)");

    // rain lives on its OWN full-size layer; the client_effect appends the canvas.
    auto rain = box() | absolute() | detail::raw_css("inset", "0") | no_pointer
        | matrix_rain();

    auto glow = box() | absolute()
        | detail::raw_css("width", "720px") | detail::raw_css("height", "720px")
        | detail::raw_css("top", "-260px") | detail::raw_css("left", "8%")
        | detail::raw_css("background", "radial-gradient(circle," + detail::rgba_hex(accent, 0.16f) + ", transparent 62%)")
        | detail::raw_css("filter", "blur(8px)")
        | detail::raw_css("will-change", "transform, opacity")
        | detail::raw_css("animation", "agentty-hero-glow 12s ease-in-out infinite alternate");

    auto scan = box() | absolute() | detail::raw_css("inset", "0")
        | detail::raw_css("background",
            "repeating-linear-gradient(180deg, rgba(255,255,255,.015) 0px, rgba(255,255,255,.015) 1px, transparent 1px, transparent 3px)")
        | detail::raw_css("mix-blend-mode", "overlay");

    return box(rain, grid, glow, scan)
        | absolute() | detail::raw_css("inset", "0") | z(0)
        | detail::raw_css("overflow", "hidden") | no_pointer
        | detail::raw_css("mask-image", "linear-gradient(180deg,#000 0%,#000 30%,transparent 96%)")
        | detail::raw_css("-webkit-mask-image", "linear-gradient(180deg,#000 0%,#000 30%,transparent 96%)");
}

} // namespace agentty
