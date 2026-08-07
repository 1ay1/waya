#pragma once
/// \file complete.hpp
/// BROWSER-PARITY mods — the long tail of CSS/HTML capabilities given a
/// first-class, named home so `css()` stays a rare escape hatch, not a routine
/// crutch. Everything the box model, flexbox, grid, transforms, filters,
/// scroll-snap, and the form/text layers can do has a mod here.
///
/// These are thin, honest wrappers over the style channel (via `raw_css`), so
/// they compose with every other mod and with `theme()`. If a value you need
/// isn't here, `css(prop, value)` always is — but you should rarely need it.
///
///   img | object_pos("top") | fit("cover")
///   card | clip_path("circle(50%)") | mix_blend("screen")
///   panel | scroll_snap_y() ; item | snap_start()
///   hero | perspective(800) ; layer | rotate_x(12) | translate_z(40)
///   input(v) | accent(0x6366f1) | caret(0x22d3ee)

#include "node.hpp"
#include <string>

namespace waya::surface {

namespace detail {
inline std::string px_(float v){ std::string s = numstr(v); return s + "px"; }
}

// ═══════════════════════════════════════════════════════════════════════════
//  FLEXBOX — the members not already covered by grow/shrink/justify/align/wrap.
// ═══════════════════════════════════════════════════════════════════════════

/// `basis(rem(20))` / `basis("0%")` — flex-basis (the item's initial main size).
inline Mod basis(Len l){ return sty([=](Style& s){ std::string v; switch(l.unit){
    case Unit::px: v=detail::numstr(l.value)+"px"; break; case Unit::pct: v=detail::numstr(l.value)+"%"; break;
    case Unit::rem: v=detail::numstr(l.value)+"rem"; break; case Unit::vw: v=detail::numstr(l.value)+"vw"; break;
    case Unit::vh: v=detail::numstr(l.value)+"vh"; break; case Unit::fill: v="100%"; break;
    case Unit::hug: v="auto"; break; default: v=detail::numstr(l.value)+"px"; }
    s.extra.emplace_back("flex-basis", v); }); }
inline Mod basis(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("flex-basis", v); }); }

/// `self_start/self_center/self_end/self_stretch/self_baseline` — align-self
/// overrides for ONE flex/grid child (against its siblings' cross-axis).
inline const Mod self_start    = sty([](Style& s){ s.extra.emplace_back("align-self","flex-start"); });
inline const Mod self_center   = sty([](Style& s){ s.extra.emplace_back("align-self","center"); });
inline const Mod self_end      = sty([](Style& s){ s.extra.emplace_back("align-self","flex-end"); });
inline const Mod self_stretch  = sty([](Style& s){ s.extra.emplace_back("align-self","stretch"); });
inline const Mod self_baseline = sty([](Style& s){ s.extra.emplace_back("align-self","baseline"); });

/// `order(n)` — reorder a flex/grid child visually without touching the DOM order.
inline Mod order(int n){ return sty([=](Style& s){ s.extra.emplace_back("order", std::to_string(n)); }); }

/// `content_start/center/end/between/around/evenly` — align-content: distribute
/// WRAPPED flex lines (or grid rows) along the cross axis.
inline const Mod content_start   = sty([](Style& s){ s.extra.emplace_back("align-content","flex-start"); });
inline const Mod content_center  = sty([](Style& s){ s.extra.emplace_back("align-content","center"); });
inline const Mod content_end     = sty([](Style& s){ s.extra.emplace_back("align-content","flex-end"); });
inline const Mod content_between = sty([](Style& s){ s.extra.emplace_back("align-content","space-between"); });
inline const Mod content_around  = sty([](Style& s){ s.extra.emplace_back("align-content","space-around"); });
inline const Mod content_evenly  = sty([](Style& s){ s.extra.emplace_back("align-content","space-evenly"); });

/// `row_gap(n)` / `col_gap(n)` — independent axis gaps (gap() sets both).
inline Mod row_gap(float px){ return sty([=](Style& s){ s.extra.emplace_back("row-gap", detail::px_(px)); }); }
inline Mod col_gap(float px){ return sty([=](Style& s){ s.extra.emplace_back("column-gap", detail::px_(px)); }); }

// ═══════════════════════════════════════════════════════════════════════════
//  GRID — placement for grid children + track sugar beyond grid_cols/template.
// ═══════════════════════════════════════════════════════════════════════════

/// `col_line("1 / 3")` / `row_line("2 / span 2")` — explicit grid line placement.
inline Mod col_line(std::string spec){ return sty([=](Style& s){ s.extra.emplace_back("grid-column", spec); }); }
inline Mod row_line(std::string spec){ return sty([=](Style& s){ s.extra.emplace_back("grid-row", spec); }); }
/// `row_span(n)` — span n grid rows (mirrors the existing col_span).
inline Mod row_span_(int n){ return sty([=](Style& s){ s.extra.emplace_back("grid-row", "span " + std::to_string(n)); }); }
/// `place_self("center")` / `place_self("start end")` — align+justify self, one call.
inline Mod place_self(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("place-self", v); }); }
/// `justify_self_start/center/end/stretch` — a grid child's inline-axis alignment.
inline const Mod justify_self_start   = sty([](Style& s){ s.extra.emplace_back("justify-self","start"); });
inline const Mod justify_self_center  = sty([](Style& s){ s.extra.emplace_back("justify-self","center"); });
inline const Mod justify_self_end     = sty([](Style& s){ s.extra.emplace_back("justify-self","end"); });
inline const Mod justify_self_stretch = sty([](Style& s){ s.extra.emplace_back("justify-self","stretch"); });
/// `auto_rows("minmax(80px,auto)")` / `auto_cols(...)` / `auto_flow("dense")`.
inline Mod auto_rows(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("grid-auto-rows", v); }); }
inline Mod auto_cols(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("grid-auto-columns", v); }); }
inline Mod auto_flow(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("grid-auto-flow", v); }); }

// ═══════════════════════════════════════════════════════════════════════════
//  DISPLAY & BOX
// ═══════════════════════════════════════════════════════════════════════════

inline const Mod inline_block = sty([](Style& s){ s.extra.emplace_back("display","inline-block"); });
inline const Mod inline_flex  = sty([](Style& s){ s.extra.emplace_back("display","inline-flex"); });
inline const Mod block        = sty([](Style& s){ s.extra.emplace_back("display","block"); });
inline const Mod contents     = sty([](Style& s){ s.extra.emplace_back("display","contents"); });
/// `box_sizing("content-box")` — override the default border-box.
inline Mod box_sizing(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("box-sizing", v); }); }
/// `object_pos("top")` / `object_pos("50% 20%")` — where a cover-cropped image sits.
inline Mod object_pos(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("object-position", v); }); }
/// `visible()` / `invisible()` — visibility (keeps layout space, unlike hidden()).
inline const Mod visible   = sty([](Style& s){ s.extra.emplace_back("visibility","visible"); });
inline const Mod invisible = sty([](Style& s){ s.extra.emplace_back("visibility","hidden"); });
/// `isolate()` — a new stacking context (mix-blend-mode containment).
inline const Mod isolate = sty([](Style& s){ s.extra.emplace_back("isolation","isolate"); });
/// `content_visibility("auto")` — skip rendering off-screen subtrees (perf).
inline Mod content_visibility(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("content-visibility", v); }); }
/// `will_change("transform")` — hint the compositor for a smoother animation.
inline Mod will_change(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("will-change", v); }); }
/// `contain("layout paint")` — CSS containment for isolation/perf.
inline Mod contain_(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("contain", v); }); }
/// `appearance_none()` — strip native control chrome (custom checkboxes etc.).
inline const Mod appearance_none = sty([](Style& s){ s.extra.emplace_back("appearance","none"); s.extra.emplace_back("-webkit-appearance","none"); });

// ═══════════════════════════════════════════════════════════════════════════
//  INTERACTION — pointer, touch, cursor, selection, resize.
// ═══════════════════════════════════════════════════════════════════════════

/// `pointer_events("none")` / the ready `no_pointer` (in node.hpp) for the common case.
inline Mod pointer_events(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("pointer-events", v); }); }
/// `touch_action("pan-y")` — control browser gestures (e.g. allow vertical scroll only).
inline Mod touch_action(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("touch-action", v); }); }
/// `user_select("text"|"all"|"none")` — override text selectability.
inline Mod user_select(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("user-select", v); s.extra.emplace_back("-webkit-user-select", v); }); }
/// Cursor variants beyond `pointer`: grab/grabbing/text/move/wait/help/crosshair/…
inline Mod cursor_(std::string name){ return sty([=](Style& s){ s.extra.emplace_back("cursor", name); }); }
inline const Mod cursor_grab     = sty([](Style& s){ s.extra.emplace_back("cursor","grab"); });
inline const Mod cursor_grabbing = sty([](Style& s){ s.extra.emplace_back("cursor","grabbing"); });
inline const Mod cursor_text     = sty([](Style& s){ s.extra.emplace_back("cursor","text"); });
inline const Mod cursor_move     = sty([](Style& s){ s.extra.emplace_back("cursor","move"); });
inline const Mod cursor_wait     = sty([](Style& s){ s.extra.emplace_back("cursor","wait"); });
inline const Mod cursor_help     = sty([](Style& s){ s.extra.emplace_back("cursor","help"); });
inline const Mod cursor_disabled = sty([](Style& s){ s.extra.emplace_back("cursor","not-allowed"); });
/// `resize("vertical"|"horizontal"|"both"|"none")` — a textarea/box resize grip.
inline Mod resize(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("resize", v); }); }
/// `accent(0x6366f1)` — the browser accent for checkboxes, radios, ranges, progress.
inline Mod accent(std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("accent-color", detail::hexstr(c)); }); }
/// `caret(0x22d3ee)` — the text-cursor colour in an input/textarea.
inline Mod caret(std::uint32_t c){ return sty([=](Style& s){ s.extra.emplace_back("caret-color", detail::hexstr(c)); }); }

// ═══════════════════════════════════════════════════════════════════════════
//  SCROLL — snap, behaviour, overscroll, scrollbar gutter.
// ═══════════════════════════════════════════════════════════════════════════

/// `scroll_snap_x()` / `scroll_snap_y()` — make a container a mandatory snap track.
inline const Mod scroll_snap_x = sty([](Style& s){ s.extra.emplace_back("scroll-snap-type","x mandatory"); s.extra.emplace_back("overflow-x","auto"); });
inline const Mod scroll_snap_y = sty([](Style& s){ s.extra.emplace_back("scroll-snap-type","y mandatory"); s.extra.emplace_back("overflow-y","auto"); });
/// `snap_start/snap_center/snap_end` — a snap child's alignment.
inline const Mod snap_start  = sty([](Style& s){ s.extra.emplace_back("scroll-snap-align","start"); });
inline const Mod snap_center = sty([](Style& s){ s.extra.emplace_back("scroll-snap-align","center"); });
inline const Mod snap_end    = sty([](Style& s){ s.extra.emplace_back("scroll-snap-align","end"); });
/// `smooth_scroll()` — animate programmatic/anchor scrolling.
inline const Mod smooth_scroll = sty([](Style& s){ s.extra.emplace_back("scroll-behavior","smooth"); });
/// `overscroll(std::string)` — control scroll chaining ("contain" / "none").
inline Mod overscroll_(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("overscroll-behavior", v); }); }
/// `scrollbar_gutter("stable")` — reserve the scrollbar's space to avoid layout jump.
inline Mod scrollbar_gutter(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("scrollbar-gutter", v); }); }
/// `scroll_padding(px)` — inset for where snap/anchor scrolling lands (sticky headers).
inline Mod scroll_padding(float px){ return sty([=](Style& s){ s.extra.emplace_back("scroll-padding", detail::px_(px)); }); }

// ═══════════════════════════════════════════════════════════════════════════
//  TRANSFORMS — 2D extras + full 3D.
// ═══════════════════════════════════════════════════════════════════════════

/// `translate_x/translate_y/translate_z(px)` — single-axis translate (composes).
inline Mod translate_x(float px){ return sty([=](Style& s){ s.extra.emplace_back("transform","translateX("+detail::px_(px)+")"); }); }
inline Mod translate_y(float px){ return sty([=](Style& s){ s.extra.emplace_back("transform","translateY("+detail::px_(px)+")"); }); }
inline Mod translate_z(float px){ return sty([=](Style& s){ s.extra.emplace_back("transform","translateZ("+detail::px_(px)+")"); }); }
/// `rotate_x/rotate_y/rotate_z(deg)` — 3D rotation about an axis.
inline Mod rotate_x(float deg){ return sty([=](Style& s){ s.extra.emplace_back("transform","rotateX("+detail::numstr(deg)+"deg)"); }); }
inline Mod rotate_y(float deg){ return sty([=](Style& s){ s.extra.emplace_back("transform","rotateY("+detail::numstr(deg)+"deg)"); }); }
inline Mod rotate_z(float deg){ return sty([=](Style& s){ s.extra.emplace_back("transform","rotateZ("+detail::numstr(deg)+"deg)"); }); }
/// `skew(x_deg, y_deg)` — skew transform.
inline Mod skew(float x, float y=0){ return sty([=](Style& s){ s.extra.emplace_back("transform","skew("+detail::numstr(x)+"deg,"+detail::numstr(y)+"deg)"); }); }
/// `scale_xy(sx, sy)` — non-uniform scale.
inline Mod scale_xy(float sx, float sy){ return sty([=](Style& s){ s.extra.emplace_back("transform","scale("+detail::numstr(sx)+","+detail::numstr(sy)+")"); }); }
/// `transform("...")` — a fully custom transform string (composes several ops).
inline Mod transform(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("transform", v); }); }
/// `transform_origin("top left")` — the pivot for rotate/scale.
inline Mod transform_origin(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("transform-origin", v); }); }
/// `perspective(800)` — 3D depth for child transforms (set on the parent).
inline Mod perspective(float px){ return sty([=](Style& s){ s.extra.emplace_back("perspective", detail::px_(px)); }); }
/// `preserve_3d()` — children keep their 3D positions (a 3D scene).
inline const Mod preserve_3d = sty([](Style& s){ s.extra.emplace_back("transform-style","preserve-3d"); });
/// `backface_hidden()` — hide a card's back face when flipped.
inline const Mod backface_hidden = sty([](Style& s){ s.extra.emplace_back("backface-visibility","hidden"); });

// ═══════════════════════════════════════════════════════════════════════════
//  VISUAL — clip, mask, blend, filters, outline, shadows, gradients.
// ═══════════════════════════════════════════════════════════════════════════

/// `clip_path("circle(50%)")` / `clip_path("polygon(...)")` — non-rect clipping.
inline Mod clip_path(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("clip-path", v); }); }
/// `mask(std::string)` — a CSS mask image/gradient (fades, reveals).
inline Mod mask_(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("-webkit-mask", v); s.extra.emplace_back("mask", v); }); }
/// `mix_blend("multiply"|"screen"|"overlay"|…)` — blend with what's behind.
inline Mod mix_blend(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("mix-blend-mode", v); }); }
/// `bg_blend(std::string)` — blend a node's own background layers.
inline Mod bg_blend(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("background-blend-mode", v); }); }
/// `filter("...")` — a raw filter chain (compose blur/brightness/etc. yourself).
inline Mod filter(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("filter", v); }); }
/// `outline(width, color)` / `outline_offset(px)` — an outline that doesn't affect layout.
inline Mod outline(float width, std::uint32_t color, std::string style_ = "solid"){
    return sty([=](Style& s){ s.extra.emplace_back("outline", detail::px_(width)+" "+style_+" "+detail::hexstr(color)); }); }
inline Mod outline_offset(float px){ return sty([=](Style& s){ s.extra.emplace_back("outline-offset", detail::px_(px)); }); }
/// `inset_shadow("...")` — an inner shadow (the default shadow() is outer).
inline Mod inset_shadow(std::string spec){ return sty([=](Style& s){ s.extra.emplace_back("box-shadow", "inset " + spec); }); }
/// `ring_color(0x…)` / `ring(px)` alt: an outline ring at an offset (focus styling).
inline Mod ring_at(float width, std::uint32_t color, float offset = 2){
    return sty([=](Style& s){ s.extra.emplace_back("box-shadow",
        "0 0 0 "+detail::px_(offset)+" transparent, 0 0 0 "+detail::px_(offset+width)+" "+detail::hexstr(color)); }); }
/// `conic(from, list…)` — a conic gradient (pie charts, colour wheels). Give a
/// full CSS colour-stop list string for max control.
inline Mod conic_gradient(std::string stops, std::string at = "50% 50%"){
    return sty([=](Style& s){ s.extra.emplace_back("background", "conic-gradient(from 0deg at "+at+", "+stops+")"); }); }
/// `radial(stops, at)` — a radial gradient with an explicit stop list.
inline Mod radial_gradient(std::string stops, std::string at = "50% 50%"){
    return sty([=](Style& s){ s.extra.emplace_back("background", "radial-gradient(at "+at+", "+stops+")"); }); }
/// `linear_gradient("135deg, #a, #b")` — full-control linear gradient.
inline Mod linear_gradient(std::string spec){ return sty([=](Style& s){ s.extra.emplace_back("background", "linear-gradient("+spec+")"); }); }

// ═══════════════════════════════════════════════════════════════════════════
//  TEXT — the typography long tail.
// ═══════════════════════════════════════════════════════════════════════════

/// `text_shadow("0 1px 2px rgba(0,0,0,.5)")` — a drop shadow on text.
inline Mod text_shadow(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("text-shadow", v); }); }
/// `text_indent(px)` — first-line indent.
inline Mod text_indent(float px){ return sty([=](Style& s){ s.extra.emplace_back("text-indent", detail::px_(px)); }); }
/// `white_space("pre"|"pre-wrap"|"nowrap"|"pre-line")` — whitespace/wrapping mode.
inline Mod white_space(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("white-space", v); }); }
/// `word_break("break-all"|"keep-all")` / `overflow_wrap("anywhere")`.
inline Mod word_break(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("word-break", v); }); }
inline Mod overflow_wrap(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("overflow-wrap", v); }); }
/// `hyphens("auto")` — automatic hyphenation for justified text.
inline Mod hyphens(std::string v = "auto"){ return sty([=](Style& s){ s.extra.emplace_back("hyphens", v); }); }
/// `word_spacing(px)` — extra space between words.
inline Mod word_spacing(float px){ return sty([=](Style& s){ s.extra.emplace_back("word-spacing", detail::px_(px)); }); }
/// `vertical_align("middle"|"top"|…)` — inline/table-cell vertical alignment.
inline Mod vertical_align(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("vertical-align", v); }); }
/// `writing_mode("vertical-rl")` — vertical text (CJK, sidebars).
inline Mod writing_mode(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("writing-mode", v); }); }
/// `text_columns(n, gap_px)` — flow text into n newspaper columns.
inline Mod text_columns(int n, float gap_px = 24){ return sty([=](Style& s){
    s.extra.emplace_back("column-count", std::to_string(n));
    s.extra.emplace_back("column-gap", detail::px_(gap_px)); }); }
/// `text_decoration("underline dotted #f00")` — full-control decoration.
inline Mod text_decoration(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("text-decoration", v); }); }
/// `text_wrap("balance"|"pretty")` — modern headline/paragraph wrapping.
inline Mod text_wrap(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("text-wrap", v); }); }
/// `list_style("none"|"disc inside"|…)` — bullet/number styling for as("ul"/"ol").
inline Mod list_style(std::string v){ return sty([=](Style& s){ s.extra.emplace_back("list-style", v); }); }

} // namespace waya::surface
