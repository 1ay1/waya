#pragma once
/// \file reveal.hpp
/// Client-owned decorative MOTION — the maya-style answer to the pile of
/// `useEffect` + `IntersectionObserver` + `requestAnimationFrame` a React site
/// hand-rolls for its flourishes (scroll reveals, typewriters, odometers,
/// magnetic buttons, a scroll-progress bar).
///
/// WHY THIS IS CORE, NOT AN EXAMPLE. These behaviours share one shape: they are
/// PURELY presentational, driven by the browser (viewport, pointer, clock), and
/// carry NO application state — a reveal looks identical whether or not the
/// server ever hears about it. Routing them through the Model (a `Sub::every`
/// tick, an `on_appear` Msg) would be per-visitor server work for zero
/// authoritative value — the "server-tick trap". So they belong on the SAME
/// seam as `ripple`/`tap_pop`/`group`: a named, declarative mod you apply with
/// `|`, that registers its keyframes + one delegated JS handler ONCE in the
/// asset registry (deduped) and marks the node with a `data-wa-*` attribute.
///
/// The author writes WHAT ("reveal this on scroll", "type this out", "count
/// this up"), never HOW. The raw DOM/rAF lives once, inside the core, behind a
/// name — so a whole site's motion is readable vocabulary at the call site, and
/// escape-by-construction still holds (no `markup`, no per-call `client_effect`
/// JS to audit). This is styling ownership extended to motion: waya owns it the
/// way maya owns it.
///
///   title("Blazing fast")           | reveal()
///   box(cards...)                    | reveal_stagger(90)
///   text("agentty --version")        | typewriter()
///   text("8.8 MB")                   | count_up()
///   button("Get started")            | magnetic()
///   page(scroll_progress(), ...)     // one document-level bar
///
/// All of it is inert under `prefers-reduced-motion: reduce` (the boot script
/// checks the media query and reveals everything immediately, statically).

#include "node.hpp"
#include "assets.hpp"

#include <string>

namespace waya::surface {

namespace detail {

// One delegated bootstrap, registered once, that powers every reveal-family
// mod below. It is the decorative sibling of the runtime's `data-ev-appear`
// path (which fires a Msg): this one stays entirely client-side. A single
// IntersectionObserver serves every marked node; a MutationObserver re-arms
// nodes inserted by later paints; everything is a no-op (revealed immediately)
// under reduced-motion. Handlers dispatch by the `data-wa-reveal` kind.
inline void ensure_reveal_boot() {
    assets().css(
        "[data-wa-reveal]{opacity:0;transform:translateY(14px);"
        "transition:opacity .6s cubic-bezier(.22,.61,.36,1),transform .6s cubic-bezier(.22,.61,.36,1)}"
        "[data-wa-reveal].wa-seen{opacity:1;transform:none}"
        "@media (prefers-reduced-motion:reduce){[data-wa-reveal]{opacity:1;transform:none;transition:none}}");
    assets().script("wa-reveal-boot",
        "(function(){"
        "var RM=window.matchMedia&&matchMedia('(prefers-reduced-motion:reduce)').matches;"
        // per-kind entrance: mark seen, then run the kind's one-shot behaviour.
        "function typeOut(el){var full=el.getAttribute('data-wa-type')||el.textContent||'';"
        "var car=el.getAttribute('data-wa-caret')!=null;if(car)el.classList.add('wa-caret');"
        "el.textContent='';var i=0;(function step(){el.textContent=full.slice(0,i);"
        "if(i>=full.length){if(car)setTimeout(function(){el.classList.remove('wa-caret')},600);return;}"
        "i++;setTimeout(step,26+Math.random()*30);})();}"
        "function countUp(el){var full=el.getAttribute('data-wa-count')||el.textContent||'';"
        "var m=full.match(/^(\\D*?)([\\d.]+)(.*)$/);if(!m){el.textContent=full;return;}"
        "var pre=m[1],num=m[2],suf=m[3],target=parseFloat(num),dec=(num.split('.')[1]||'').length;"
        "var t0=performance.now(),dur=1200;"
        "(function tick(now){var p=Math.min(1,(now-t0)/dur);var e=1-Math.pow(1-p,3);"
        "el.textContent=pre+(target*e).toFixed(dec)+suf;"
        "if(p<1)requestAnimationFrame(tick);else el.textContent=full;})(t0);}"
        "function run(el){el.classList.add('wa-seen');"
        "var k=el.getAttribute('data-wa-reveal');"
        "var st=el.getAttribute('data-wa-stagger');"
        "if(st){var kids=el.children,d=parseInt(st,10)||80;"
        "for(var i=0;i<kids.length;i++){(function(c,idx){c.setAttribute('data-wa-reveal','');"
        "if(RM){c.classList.add('wa-seen');return;}"
        "setTimeout(function(){c.classList.add('wa-seen');},idx*d);})(kids[i],i);}}"
        "if(RM){el.textContent=el.getAttribute('data-wa-type')||el.getAttribute('data-wa-count')||el.textContent;return;}"
        "if(k==='type')typeOut(el);else if(k==='count')countUp(el);}"
        "var io=window.IntersectionObserver?new IntersectionObserver(function(es){"
        "for(var i=0;i<es.length;i++){if(!es[i].isIntersecting)continue;"
        "var el=es[i].target;io.unobserve(el);run(el);}},{rootMargin:'0px 0px -8% 0px',threshold:.15}):null;"
        "function arm(root){(root||document).querySelectorAll('[data-wa-reveal]:not(.wa-seen)').forEach(function(el){"
        "if(el.__waArm)return;el.__waArm=1;if(RM||!io){run(el);return;}io.observe(el);});}"
        "new MutationObserver(function(){arm();}).observe(document.documentElement,{childList:true,subtree:true});"
        "if(document.readyState!=='loading')arm();else document.addEventListener('DOMContentLoaded',function(){arm();});"
        "})();");
}

} // namespace detail

/// `reveal()` — fade + rise this node into view the first time it scrolls onto
/// screen, then leave it. Pure client polish (one shared IntersectionObserver),
/// no Model round-trip, inert under reduced-motion. The declarative replacement
/// for a React `[data-reveal]` + `useEffect(IntersectionObserver…)`.
inline Mod reveal() {
    detail::ensure_reveal_boot();
    return {[](Node& n){ n.attrs.emplace_back("data-wa-reveal", ""); }};
}

/// `reveal_stagger(ms)` — reveal this node's DIRECT CHILDREN one after another,
/// `ms` apart, when the container scrolls into view (a feature grid that cascades
/// in). The container itself is revealed; children inherit the marker at runtime.
inline Mod reveal_stagger(int ms = 80) {
    detail::ensure_reveal_boot();
    return {[ms](Node& n){
        n.attrs.emplace_back("data-wa-reveal", "");
        n.attrs.emplace_back("data-wa-stagger", std::to_string(ms));
    }};
}

/// `typewriter([caret])` — type this text node's content out, character by
/// character, when it scrolls into view. If `caret` is true a blinking block
/// caret trails the text until it finishes. The text is captured from the node
/// at build time, so it's still fully present for crawlers/no-JS (SSR-safe).
inline Mod typewriter(bool caret = true) {
    detail::ensure_reveal_boot();
    if (caret) assets().css(
        ".wa-caret::after{content:'';display:inline-block;width:.6ch;height:1.05em;"
        "margin-left:1px;vertical-align:-.15em;background:currentColor;"
        "animation:wa-caret-blink 1s steps(1) infinite}"
        "@keyframes wa-caret-blink{50%{opacity:0}}");
    return {[caret](Node& n){
        // Snapshot the settled text so the boot script can retype it.
        std::string full = n.text;
        n.attrs.emplace_back("data-wa-reveal", "type");
        n.attrs.emplace_back("data-wa-type", full);
        // The caret is a CLASS the boot script toggles at runtime (add on start,
        // remove when done) rather than a server-emitted `class` attr — the
        // node already carries an interned styling class, and two `class`
        // attributes on one element is invalid HTML (the browser drops one).
        if (caret) n.attrs.emplace_back("data-wa-caret", "");
    }};
}

/// `count_up()` — odometer this text from zero to its value when it scrolls into
/// view. Understands a leading prefix + number + trailing suffix ("8.8 MB",
/// "< 1 ms", "$1,299") and animates only the number, preserving decimals.
/// Non-numeric text ("C++26") is left untouched. SSR-safe: the final value is
/// the node's real text, so no-JS/crawlers see the true number.
inline Mod count_up() {
    detail::ensure_reveal_boot();
    return {[](Node& n){
        n.attrs.emplace_back("data-wa-reveal", "count");
        n.attrs.emplace_back("data-wa-count", n.text);
    }};
}

/// `magnetic(strength)` — this element leans toward the cursor while the pointer
/// is near it, springing back on leave. The classic "the button wants to be
/// clicked" flourish. `strength` is the max px pull (default 6). Pure pointer
/// math on one delegated handler; no Model state, off under reduced-motion.
inline Mod magnetic(int strength = 6) {
    assets().css("[data-wa-mag]{transition:transform .18s cubic-bezier(.2,.7,.2,1);will-change:transform}"
                 "@media (prefers-reduced-motion:reduce){[data-wa-mag]{transition:none}}");
    assets().script("wa-magnetic-boot",
        "(function(){if(window.matchMedia&&matchMedia('(prefers-reduced-motion:reduce)').matches)return;"
        "if(matchMedia&&matchMedia('(pointer:coarse)').matches)return;" // no magnet on touch
        "document.addEventListener('pointermove',function(e){"
        "var t=e.target.closest&&e.target.closest('[data-wa-mag]');"
        "document.querySelectorAll('[data-wa-mag].wa-mag-live').forEach(function(el){"
        "if(el!==t){el.classList.remove('wa-mag-live');el.style.transform='';}});"
        "if(!t)return;var s=parseInt(t.getAttribute('data-wa-mag'),10)||6;"
        "var r=t.getBoundingClientRect();var dx=(e.clientX-(r.left+r.width/2))/(r.width/2);"
        "var dy=(e.clientY-(r.top+r.height/2))/(r.height/2);t.classList.add('wa-mag-live');"
        "t.style.transform='translate('+(dx*s).toFixed(1)+'px,'+(dy*s).toFixed(1)+'px)';});"
        "document.addEventListener('pointerleave',function(e){"
        "var t=e.target.closest&&e.target.closest('[data-wa-mag]');if(t){t.classList.remove('wa-mag-live');t.style.transform='';}},true);"
        "})();");
    return {[strength](Node& n){ n.attrs.emplace_back("data-wa-mag", std::to_string(strength)); }};
}

/// `scroll_progress(color)` — a thin gradient bar pinned to the top of the
/// viewport that fills as the reader scrolls the page. Document-level: drop it
/// ANYWHERE in your tree (it positions itself fixed) — one per page. Pure client
/// scroll math on a single rAF-throttled handler.
inline NodeRef scroll_progress(std::uint32_t color = 0x58a6ff,
                               std::uint32_t color2 = 0xd2a8ff) {
    assets().script("wa-scrollprog-boot",
        "(function(){var bar=document.getElementById('wa-scrollprog');"
        "function upd(){bar=bar||document.getElementById('wa-scrollprog');if(!bar)return;"
        "var h=document.documentElement.scrollHeight-innerHeight;"
        "bar.style.transform='scaleX('+(h>0?Math.min(1,scrollY/h):0)+')';}"
        "var t=0;addEventListener('scroll',function(){if(t)return;t=requestAnimationFrame(function(){t=0;upd();});},{passive:true});"
        "addEventListener('resize',upd,{passive:true});"
        "if(document.readyState!=='loading')upd();else addEventListener('DOMContentLoaded',upd);"
        "})();");
    auto n = detail::new_node();
    n->kind = Kind::box;
    n->attrs.emplace_back("id", "wa-scrollprog");
    n->style.extra.emplace_back("position", "fixed");
    n->style.extra.emplace_back("top", "0");
    n->style.extra.emplace_back("left", "0");
    n->style.extra.emplace_back("height", "2px");
    n->style.extra.emplace_back("width", "100%");
    n->style.extra.emplace_back("transform", "scaleX(0)");
    n->style.extra.emplace_back("transform-origin", "0 50%");
    n->style.extra.emplace_back("z-index", "2147483646");
    n->style.extra.emplace_back("pointer-events", "none");
    n->style.extra.emplace_back("background",
        "linear-gradient(90deg," + detail::hexstr(color) + "," + detail::hexstr(color2) + ")");
    finalize(*n);
    return n;
}

namespace detail {
// FOUC-free theme boot: set `data-theme` on <html> from localStorage (or the
// OS `prefers-color-scheme`) as the FIRST thing that runs, before first paint,
// so a dark-mode reader never flashes a white page. Registered as raw <head>
// markup (not a deferred module) precisely so it runs synchronously up-front.
inline void ensure_theme_boot() {
    static bool done = false;
    if (done) return; done = true;
    assets().head(
        "<script>(function(){try{var t=localStorage.getItem('waya:theme');"
        "if(!t)t=(window.matchMedia&&matchMedia('(prefers-color-scheme: light)').matches)?'light':'dark';"
        "document.documentElement.setAttribute('data-theme',t);}catch(e){}})();</script>");
}
} // namespace detail

/// `theme_toggle()` — a one-call light/dark switch. Renders a sun/moon button
/// that flips `data-theme` on <html> and persists the choice to localStorage;
/// a synchronous boot script (auto-registered) applies the saved/OS theme
/// before first paint so there's no flash. Style your palette with
/// `[data-theme="light"]` overrides — no Model state, no server round-trip.
///
///   row(nav_links..., theme_toggle())
///
/// This is the whole feature: in React it's `useState`+`useEffect`+a hydration
/// guard+a separate inline FOUC script; here it's one node.
inline NodeRef theme_toggle() {
    detail::ensure_theme_boot();
    // Icon swap keys off [data-theme] on <html>; the JS hook is a data-attr so
    // it never collides with the node's interned styling class.
    assets().css(
        "[data-wa-theme] .wa-sun{display:none}[data-wa-theme] .wa-moon{display:block}"
        "[data-theme=light] [data-wa-theme] .wa-sun{display:block}"
        "[data-theme=light] [data-wa-theme] .wa-moon{display:none}");
    assets().script("wa-theme-toggle-boot",
        "document.addEventListener('click',function(e){"
        "var b=e.target.closest&&e.target.closest('[data-wa-theme]');if(!b)return;"
        "var cur=document.documentElement.getAttribute('data-theme')||'dark';"
        "var next=cur==='light'?'dark':'light';"
        "document.documentElement.setAttribute('data-theme',next);"
        "try{localStorage.setItem('waya:theme',next);}catch(_){}}); ");
    // sun (shown in light mode) + moon (shown in dark mode). Both escape-safe.
    auto sun = svg("<circle cx='12' cy='12' r='4.2' fill='currentColor'/>"
        "<g stroke='currentColor' stroke-width='1.8' stroke-linecap='round'>"
        "<line x1='12' y1='2.5' x2='12' y2='5'/><line x1='12' y1='19' x2='12' y2='21.5'/>"
        "<line x1='2.5' y1='12' x2='5' y2='12'/><line x1='19' y1='12' x2='21.5' y2='12'/>"
        "<line x1='5.2' y1='5.2' x2='6.9' y2='6.9'/><line x1='17.1' y1='17.1' x2='18.8' y2='18.8'/>"
        "<line x1='5.2' y1='18.8' x2='6.9' y2='17.1'/><line x1='17.1' y1='6.9' x2='18.8' y2='5.2'/></g>");
    auto moon = svg("<path fill='currentColor' d='M21 12.79A9 9 0 1 1 11.21 3a7 7 0 0 0 9.79 9.79z'/>");
    sun->attrs.emplace_back("class", "wa-sun");
    moon->attrs.emplace_back("class", "wa-moon");
    // A tappable box — styled entirely with mods (its interned class carries the
    // look), the JS hook is a plain data-attr, the icons are 17px.
    auto sw = box(std::move(sun), std::move(moon));
    finalize(*sw);
    sw->style.extra.emplace_back("width", "17px");
    sw->style.extra.emplace_back("height", "17px");
    auto b = box(std::move(sw));
    finalize(*b);
    b->style.flow = Flow::row;
    b->style.extra.emplace_back("align-items", "center");
    b->style.extra.emplace_back("justify-content", "center");
    b->style.extra.emplace_back("width", "34px");
    b->style.extra.emplace_back("height", "34px");
    b->style.extra.emplace_back("border-radius", "8px");
    b->style.extra.emplace_back("cursor", "pointer");
    b->style.extra.emplace_back("border", "1px solid var(--wa-border,#30363d)");
    b->style.extra.emplace_back("color", "var(--wa-dim,#8b949e)");
    b->style.extra.emplace_back("transition", "color .15s,border-color .15s");
    b->attrs.emplace_back("data-wa-theme", "");
    b->attrs.emplace_back("role", "button");
    b->attrs.emplace_back("tabindex", "0");
    b->attrs.emplace_back("aria-label", "Toggle color theme");
    b->attrs.emplace_back("title", "Toggle color theme");
    finalize(*b);
    return b;
}

} // namespace waya::surface
