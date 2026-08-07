#pragma once
/// \file media.hpp
/// CAPABILITY elements — the pieces with real browser BEHAVIOUR you can't build
/// from box/text/style: video & audio playback, canvas, inline SVG, and safe
/// embeds (YouTube, maps, any iframe). First-class and dead-simple.
///
///   video("/clip.mp4") | autoplay() | loop_media() | silent()  // a silent looping hero
///   audio("/song.mp3")
///   youtube("dQw4w9WgXcQ")                                // just the id
///   embed("https://www.google.com/maps/embed?...")        // any iframe, sandboxed
///   svg("<circle cx='12' cy='12' r='10'/>")               // inline vector art
///   canvas(320, 200) | id("chart")                        // a real <canvas> to draw on
///
/// video()/audio() already exist in node.hpp (a controls-on player); this adds
/// the option mods (autoplay/loop_media/silent/poster/no_controls/plays_inline)
/// the elements that were missing entirely.

#include "node.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace waya::surface {

// ═══════════════════════════════════════════════════════════════════════════
//  VIDEO / AUDIO options — make the existing video()/audio() players usable.
// ═══════════════════════════════════════════════════════════════════════════

/// `autoplay()` — start playing on load. Browsers require `silent()` too for
/// video autoplay, so this pairs with it for a silent background/hero clip.
inline Mod autoplay(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("autoplay",""); }}; }
/// `loop_media()` — restart when it ends (a looping hero background).
inline Mod loop_media(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("loop",""); }}; }
/// `silent()` — start muted (required for video autoplay). (Named `silent` so it
/// doesn't collide with the `muted` colour token.)
inline Mod silent(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("muted",""); }}; }
/// `no_controls()` — hide the native player chrome (a decorative background clip).
inline Mod no_controls(){ return {[=](Node& n){
    n.attrs.erase(std::remove_if(n.attrs.begin(), n.attrs.end(),
                  [](const std::pair<std::string,std::string>& a){ return a.first=="controls"; }),
                  n.attrs.end());
}}; }
/// `poster(url)` — the still image shown before a video plays.
inline Mod poster(std::string url){ return {[=](Node& n){ n.attrs.emplace_back("poster", url); }}; }
/// `preload(std::string)` — "none"/"metadata"/"auto" load hint.
inline Mod preload(std::string how){ return {[=](Node& n){ n.attrs.emplace_back("preload", how); }}; }
/// `plays_inline()` — play inline on iOS instead of going fullscreen.
inline Mod plays_inline(){ return {[=](Node& n){ n.attrs.emplace_back("playsinline",""); }}; }

namespace detail {
inline std::string attr_q(const std::string& s){
    std::string o; o.reserve(s.size()+4);
    for(char c : s){ if(c=='"') o+="&quot;"; else if(c=='<') o+="&lt;"; else if(c=='>') o+="&gt;"; else if(c=='&') o+="&amp;"; else o+=c; }
    return o;
}
}

// ═══════════════════════════════════════════════════════════════════════════
//  EMBEDS — an iframe, made safe and simple. The 90% cases (YouTube, maps) are
//  one call; any URL works. Sandboxed by default so an embed can't script your
//  page. Wrap in `video_box(...)` (layout.hpp) for a responsive 16:9 frame.
// ═══════════════════════════════════════════════════════════════════════════

/// `embed(url)` — a sandboxed <iframe> for any embeddable page (a map, a form,
/// a widget, a doc). `title` is announced to screen readers. Fills its parent;
/// wrap in `video_box(embed(...))` or size it with `w`/`h`.
inline NodeRef embed(std::string url, std::string title = "Embedded content"){
    std::string safe = safe_url(std::move(url));
    std::string html =
        "<iframe src=\"" + detail::attr_q(safe) + "\" title=\"" + detail::attr_q(title) + "\""
        " loading=\"lazy\" style=\"border:0;width:100%;height:100%;display:block\""
        " sandbox=\"allow-scripts allow-same-origin allow-popups allow-forms\""
        " allow=\"accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture\""
        " referrerpolicy=\"no-referrer-when-downgrade\" allowfullscreen></iframe>";
    // The wrapper fills its parent so the iframe's height:100% has a real box to
    // resolve against. Give the embed a height (via video_box / h()) and it fills.
    auto n = markup(std::move(html));
    n->style.extra.emplace_back("display", "block");
    n->style.extra.emplace_back("width", "100%");
    n->style.extra.emplace_back("height", "100%");
    return n;
}

/// `youtube("VIDEO_ID")` — a YouTube player embed by id. Combine with
/// `video_box(youtube(id))` for a responsive 16:9 frame.
inline NodeRef youtube(std::string video_id, bool autoplay_ = false){
    std::string url = "https://www.youtube-nocookie.com/embed/" + video_id
                    + (autoplay_ ? "?autoplay=1&mute=1" : "");
    return embed(url, "YouTube video");
}
/// `vimeo("VIDEO_ID")` — a Vimeo player embed by id.
inline NodeRef vimeo(std::string video_id){
    return embed("https://player.vimeo.com/video/" + video_id, "Vimeo video");
}
/// `google_map("place or query")` — a Google Maps embed for a search query.
inline NodeRef google_map(std::string query){
    std::string q; for(char c : query){ if(c==' ') q+="+"; else if(c=='&'||c=='"'||c=='<') continue; else q+=c; }
    return embed("https://maps.google.com/maps?q=" + q + "&output=embed", "Map");
}

// ═══════════════════════════════════════════════════════════════════════════
//  INLINE SVG — arbitrary vector art beyond a single `path`. Give the INNER
//  markup (shapes); the wrapper <svg> + viewBox is provided.
// ═══════════════════════════════════════════════════════════════════════════

/// `svg(inner)` — an inline SVG with a 0 0 24 24 viewBox (icon-sized). Pass the
/// shapes: `svg("<circle cx='12' cy='12' r='10' fill='red'/>")`. Colour with
/// `fg(...)` (shapes using fill="currentColor" pick it up). Size with `size(...)`.
inline NodeRef svg(std::string inner, std::string view_box = "0 0 24 24"){
    return markup("<svg viewBox=\"" + detail::attr_q(view_box) + "\" xmlns=\"http://www.w3.org/2000/svg\""
                  " width=\"100%\" height=\"100%\" fill=\"currentColor\">" + inner + "</svg>");
}
/// `svg_raw(fullSvg)` — inject a COMPLETE <svg>…</svg> string unchanged (an
/// exported illustration). Trusted content only.
inline NodeRef svg_raw(std::string full){ return markup(std::move(full)); }

// ═══════════════════════════════════════════════════════════════════════════
//  CANVAS — a real <canvas> element you can draw on. waya renders the surface
//  server-side, so the drawing itself is done by a small client script you point
//  at the canvas by `id`; this builder gives you the sized, addressable element.
// ═══════════════════════════════════════════════════════════════════════════

/// `canvas(w, h)` — a <canvas> of the given pixel resolution. Give it an `id`
/// (from forms.hpp) and drive it with a client script, or use it as a paint
/// target. It scales to its CSS box; `w`/`h` here are the drawing buffer size.
inline NodeRef canvas(int w = 300, int h = 150){
    return markup("<canvas width=\"" + std::to_string(w) + "\" height=\"" + std::to_string(h)
                  + "\" style=\"display:block;width:100%;height:100%\"></canvas>");
}

// ═══════════════════════════════════════════════════════════════════════════
//  PICTURE — an <img> with responsive/art-directed sources + a graceful <img>.
// ═══════════════════════════════════════════════════════════════════════════

/// `picture(fallback_src, {{media, src}…})` — serve different images per media
/// query (art direction / resolution). Falls back to `fallback_src` everywhere.
inline NodeRef picture(std::string fallback_src,
                       std::vector<std::pair<std::string,std::string>> sources = {},
                       std::string alt_text = ""){
    std::string html = "<picture>";
    for(auto& [media, src] : sources)
        html += "<source media=\"" + detail::attr_q(media) + "\" srcset=\"" + detail::attr_q(src) + "\">";
    html += "<img src=\"" + detail::attr_q(safe_url(fallback_src)) + "\" alt=\"" + detail::attr_q(alt_text)
          + "\" style=\"display:block;max-width:100%;height:auto\"></picture>";
    return markup(std::move(html));
}

} // namespace waya::surface
