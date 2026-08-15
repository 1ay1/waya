#pragma once
// examples/agentty/components/hero_background.hpp
//
// HeroBackground — the geeky hero backdrop. Faithful port of
// components/HeroBackground.tsx + hero-background.css:
//   • digital-rain <canvas> (falling monospace glyphs, brand palette)
//   • a faint blueprint dot/line lattice
//   • a drifting, breathing brand glow
//   • CRT scanlines
//
// The rain is the EXACT algorithm from the source (per-frame glyph mutation, a
// fading trail via a translucent fillRect, variable-speed drops), shipped as
// ONE `canvas_fx` — a browser-owned rAF loop. The server renders the markup
// once and never touches it again: client-owned decoration, zero server cost
// per frame, no Model state, no ticks.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <cstdio>
#include <string>

namespace agentty {

using namespace waya::surface;
using namespace waya::ui;

/// `agentty::matrix_rain(accent)` — the canvas rain effect on its own, as a Mod
/// you can drop on any full-size box.
inline Mod matrix_rain(std::uint32_t accent = 0x58a6ff) {
    char acc[8]; std::snprintf(acc, sizeof acc, "#%06x", accent & 0xffffff);
    // GLYPHS lifted from the source: operators, hex, braille, box-drawing, katakana.
    std::string draw = std::string(
        "const G='01{}[]()<>/=+-*&|!?;:.#$@_\\u03bb\\u2192\\u2234\\u27e8\\u27e9\\u2207\\u2202\\u2211"
        "\\u30a2\\u30a4\\u30a6\\u30a8\\u30aa\\u30ab\\u30ad\\u30af\\u30b1\\u30b3\\u2593\\u2592\\u2591"
        "\\u2571\\u2572\\u2503\\u2501\\u250f\\u2513\\u2517\\u251b';"
        "const C=['") + acc + "','#d2a8ff','#79c0ff','#656d76'];let drops=[],cw=0,ch=0,fs=15;"
        "function reset(){cw=cvs.clientWidth;ch=cvs.clientHeight;const cols=Math.ceil(cw/fs);"
        "drops=[];for(let i=0;i<cols;i++)drops[i]={y:Math.random()*ch/fs,sp:0.4+Math.random()*0.9};}"
        "reset();new ResizeObserver(reset).observe(el);ctx.font=fs+'px ui-monospace,monospace';"
        "function frame(){"
        "ctx.fillStyle='rgba(13,17,23,0.10)';ctx.fillRect(0,0,cw,ch);"          // fading trail
        "for(let i=0;i<drops.length;i++){const d=drops[i];const x=i*fs;const y=d.y*fs;"
        "const g=G[(Math.random()*G.length)|0];"
        // bright head glyph occasionally, else a brand-palette body colour
        "ctx.fillStyle=Math.random()<0.06?'#bfe0ff':C[(Math.random()*C.length)|0];"
        "ctx.globalAlpha=0.85;ctx.fillText(g,x,y);ctx.globalAlpha=1;"
        "d.y+=d.sp;if(y>ch&&Math.random()>0.975){d.y=0;d.sp=0.4+Math.random()*0.9;}}"
        "requestAnimationFrame(frame);}requestAnimationFrame(frame);";
    return canvas_fx("agentty-rain", std::move(draw));
}

/// `agentty::hero_background(accent)` — the full four-layer backdrop, positioned
/// absolute to fill its (positioned) parent. Pass to a hero's background slot.
inline NodeRef hero_background(std::uint32_t accent = 0x58a6ff) {
    assets().keyframes("agentty-hero-glow",
        "from{transform:translate(0,0) scale(1);opacity:.8}"
        "to{transform:translate(120px,60px) scale(1.12);opacity:1}");

    auto grid = box() | absolute() | detail::raw_css("inset", "-2px")
        | detail::raw_css("background-image",
            "linear-gradient(" + detail::rgba_hex(accent, 0.045f) + " 1px, transparent 1px),"
            "linear-gradient(90deg," + detail::rgba_hex(accent, 0.045f) + " 1px, transparent 1px)")
        | detail::raw_css("background-size", "44px 44px")
        | detail::raw_css("mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)")
        | detail::raw_css("-webkit-mask-image", "radial-gradient(900px 520px at 78% 4%, #000, transparent 72%)");

    auto rain = box() | absolute() | detail::raw_css("inset", "0") | z(0) | no_pointer
        | detail::raw_css("opacity", "0.5")
        | matrix_rain(accent);

    auto glow = box() | absolute()
        | detail::raw_css("width", "720px") | detail::raw_css("height", "720px")
        | detail::raw_css("top", "-260px") | detail::raw_css("left", "8%")
        | detail::raw_css("background", "radial-gradient(circle," + detail::rgba_hex(accent, 0.16f) + ", transparent 62%)")
        | detail::raw_css("filter", "blur(8px)")
        | detail::raw_css("animation", "agentty-hero-glow 12s ease-in-out infinite alternate");

    auto scan = box() | absolute() | detail::raw_css("inset", "0")
        | detail::raw_css("background",
            "repeating-linear-gradient(180deg, rgba(255,255,255,.015) 0px, rgba(255,255,255,.015) 1px, transparent 1px, transparent 3px)")
        | detail::raw_css("mix-blend-mode", "overlay");

    return box(grid, rain, glow, scan)
        | absolute() | detail::raw_css("inset", "0") | z(0)
        | detail::raw_css("overflow", "hidden") | no_pointer
        | detail::raw_css("mask-image", "linear-gradient(180deg,#000 0%,#000 62%,transparent 100%)")
        | detail::raw_css("-webkit-mask-image", "linear-gradient(180deg,#000 0%,#000 62%,transparent 100%)");
}

} // namespace agentty
