#pragma once
/// \file ui/components.hpp
/// The official waya component library — batteries built ENTIRELY on the public
/// surface core. Every component below is just a function returning a NodeRef,
/// composed from the same `box`/`text`/`col`/`row` primitives and `|` mods you'd
/// use yourself. There is no private API here: read any component as a worked
/// example, copy it, and own your variant. That's the whole point of the split —
/// the core lets you build anything; this library is a good default set so you
/// don't have to build the common 90% from scratch.
///
///   #include <waya/ui.hpp>
///   using namespace waya::surface;
///   using namespace waya::ui;
///
///   view = card(
///       row(text("Settings") | heading, push(), badge("beta")),
///       divider(),
///       field("Email", email_input(m.email, Edit{})),
///       button("Save", Save{}, Variant::primary));
///
/// Components read theme tokens (var(--wa-*)) so they recolour with `theme(t)`;
/// each also carries a sensible fallback so it looks right with no theme at all.

#include "theme.hpp"
#include "../surface/sugar.hpp"

namespace waya::ui {

using namespace waya::surface;

// ─────────────────────────────────────────────────────────────────────────────
// Primitives every UI repeats
// ─────────────────────────────────────────────────────────────────────────────

/// `divider()` — a hairline rule. Horizontal by default; `divider(true)` = vertical.
inline NodeRef divider(bool vertical=false){
    auto n = box();
    if (vertical){ n->style.extra.emplace_back("width","1px"); n->style.extra.emplace_back("align-self","stretch"); }
    else         { n->style.extra.emplace_back("height","1px"); n->style.extra.emplace_back("width","100%"); }
    n->style.extra.emplace_back("background","var(--wa-line, rgba(255,255,255,.10))");
    n->style.extra.emplace_back("flex","0 0 auto");
    finalize(*n); return n;
}

/// `link(text)` — an inline link look (primary colour, underline on hover).
/// Pair with `tap(msg)` or `on("click", …)`.
inline NodeRef link(std::string label){
    return text(std::move(label)) | fg_primary | pointer
         | transition("opacity .15s ease") | on(Hover, detail::raw_css("text-decoration","underline"));
}

/// `card(children…)` — the ubiquitous panel: themed surface + border + padding
/// + radius + soft elevation.
template <typename... Cs> NodeRef card(Cs... cs){
    return col(std::move(cs)...) | gap(14) | pad(20) | round(16)
         | bg_surface | border_token() | elevation(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Buttons
// ─────────────────────────────────────────────────────────────────────────────

/// Button emphasis. `primary` = filled brand; `secondary` = raised surface;
/// `ghost` = text-only until hover; `danger` = destructive.
enum class Variant { primary, secondary, ghost, danger };

namespace impl {
inline Mod button_base(){
    return pad_x(16) | pad_y(10) | round(10) | pointer | semibold
         | detail::raw_css("border","1px solid transparent")
         | detail::raw_css("font-size","14px") | detail::raw_css("line-height","1")
         | detail::raw_css("white-space","nowrap") | detail::raw_css("user-select","none")
         | transition("transform .08s ease, background-color .15s ease, opacity .15s ease")
         | on(Active, detail::raw_css("transform","translateY(1px)"));
}
inline Mod button_skin(Variant v){
    switch (v){
    case Variant::primary:
        return bg_primary | fg_on_primary
             | detail::raw_css("box-shadow","0 6px 18px -6px var(--wa-primary, #6366f1)")
             | on(Hover, detail::raw_css("filter","brightness(1.08)"));
    case Variant::secondary:
        return bg_raised | fg_text | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.12))")
             | on(Hover, detail::raw_css("filter","brightness(1.12)"));
    case Variant::ghost:
        return detail::raw_css("background","transparent") | fg_text
             | on(Hover, detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.06))"));
    case Variant::danger:
        return detail::raw_css("background","var(--wa-danger, #ef4444)") | detail::raw_css("color","#fff")
             | on(Hover, detail::raw_css("filter","brightness(1.08)"));
    }
    return noop;
}
} // namespace impl

/// `button("Save", Save{})` — a themed button wired to a tap message.
template <typename Msg>
NodeRef button(std::string label, Msg msg, Variant v = Variant::primary){
    return text(std::move(label)) | impl::button_base() | impl::button_skin(v) | tap(msg);
}
/// `button_node(child, msg, variant)` — button chrome around any node (icon+text).
template <typename Msg>
NodeRef button_node(NodeRef child, Msg msg, Variant v = Variant::primary){
    return box(std::move(child)) | horizontal | gap(8) | center
         | impl::button_base() | impl::button_skin(v) | tap(msg);
}
/// `icon_button(glyph, msg)` — a compact square button for a single glyph.
template <typename Msg>
NodeRef icon_button(std::string glyph, Msg msg, Variant v = Variant::ghost){
    return text(std::move(glyph)) | impl::button_base() | impl::button_skin(v)
         | pad_x(10) | pad_y(10) | round(10) | tap(msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Form fields
// ─────────────────────────────────────────────────────────────────────────────

/// `field("Email", control)` — a labelled control: a small muted label stacked
/// above any input node. `hint` shows a helper/error line below when non-empty.
inline NodeRef field(std::string label, NodeRef control, std::string hint = ""){
    auto lab = text(std::move(label)) | fg_muted | detail::raw_css("font-size","12.5px") | semibold
             | detail::raw_css("letter-spacing",".02em");
    auto col_ = col(std::move(lab), std::move(control)) | gap(6);
    if (!hint.empty())
        col_ = col(std::move(col_), text(std::move(hint)) | fg_muted | detail::raw_css("font-size","12px")) | gap(6);
    return col_;
}

/// `styled_input(input_node)` — apply the library's input chrome to a raw core
/// input()/textarea()/select() so all your fields match. Themed, focus-ringed.
inline Mod input_skin(){
    return pad_x(12) | pad_y(10) | round(10) | fg_text
         | detail::raw_css("background","var(--wa-bg, rgba(0,0,0,.25))")
         | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.14))")
         | detail::raw_css("font-size","14px") | detail::raw_css("width","100%") | detail::raw_css("outline","none")
         | transition("border-color .15s ease, box-shadow .15s ease")
         | on(Focus, detail::raw_css("border-color","var(--wa-primary, #6366f1)"))
         | on(Focus, detail::raw_css("box-shadow","0 0 0 3px color-mix(in srgb, var(--wa-primary, #6366f1) 30%, transparent)"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Status & identity chips
// ─────────────────────────────────────────────────────────────────────────────

/// `badge("new")` — a small pill. Tone picks the colour family.
enum class Tone { neutral, primary, success, warning, danger };
namespace impl {
inline std::pair<const char*, const char*> tone_colors(Tone t){
    switch (t){
    case Tone::primary: return {"var(--wa-primary, #6366f1)", "#fff"};
    case Tone::success: return {"var(--wa-success, #22c55e)", "#04120a"};
    case Tone::warning: return {"var(--wa-warning, #f59e0b)", "#1b1200"};
    case Tone::danger:  return {"var(--wa-danger, #ef4444)",  "#fff"};
    case Tone::neutral: default: return {"var(--wa-raised, rgba(255,255,255,.10))", "var(--wa-text, #e2e8f0)"};
    }
}
} // namespace impl
inline NodeRef badge(std::string label, Tone tone = Tone::neutral){
    auto [bgc, fgc] = impl::tone_colors(tone);
    return text(std::move(label)) | pad_x(9) | pad_y(3) | round(999)
         | detail::raw_css("background", bgc) | detail::raw_css("color", fgc)
         | detail::raw_css("font-size","12px") | semibold | detail::raw_css("white-space","nowrap")
         | detail::raw_css("line-height","1.4");
}

/// `dot(tone)` — a tiny status dot (online/away/error).
inline NodeRef dot(Tone tone = Tone::success){
    auto [c, _] = impl::tone_colors(tone); (void)_;
    return box() | w(8) | h(8) | round(999) | detail::raw_css("background", c) | detail::raw_css("flex","0 0 auto");
}

/// `avatar("AB")` — a circular initials avatar. `avatar_img(url)` for a photo.
inline NodeRef avatar(std::string initials, float d = 36){
    return text(std::move(initials)) | size(d) | round(999) | center
         | bg_primary | fg_on_primary | semibold
         | detail::raw_css("font-size", std::to_string((int)(d * 0.4f)) + "px")
         | detail::raw_css("flex","0 0 auto") | detail::raw_css("text-transform","uppercase");
}
inline NodeRef avatar_img(std::string url, float d = 36){
    return image(std::move(url)) | size(d) | round(999)
         | detail::raw_css("object-fit","cover") | detail::raw_css("flex","0 0 auto");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loading states
// ─────────────────────────────────────────────────────────────────────────────

/// `spinner()` — a rotating ring. Registers its own keyframe via the core asset
/// registry, so it works anywhere with no shell setup.
inline NodeRef spinner(float d = 22, std::uint32_t stroke_col = 0){
    assets().keyframes("wa-ui-spin", "to{transform:rotate(360deg)}");
    std::string col = stroke_col ? [&]{ char b[8]; std::snprintf(b,sizeof(b),"#%06x",stroke_col&0xFFFFFF); return std::string(b); }()
                                 : std::string("var(--wa-primary, #6366f1)");
    return box() | size(d) | round(999)
         | detail::raw_css("border","2.5px solid var(--wa-line, rgba(255,255,255,.15))")
         | detail::raw_css("border-top-color", col)
         | detail::raw_css("animation","wa-ui-spin .7s linear infinite");
}

/// `skeleton(w, h)` — a shimmering placeholder block for loading content.
inline NodeRef skeleton(Len w, Len h){
    assets().keyframes("wa-ui-shimmer", "100%{background-position:-200% 0}");
    auto n = box(); n->style.w = w; n->style.h = h;
    n->style.extra.emplace_back("border-radius","8px");
    n->style.extra.emplace_back("background",
        "linear-gradient(90deg, var(--wa-raised, #1e293b) 25%, var(--wa-line, #334155) 37%, var(--wa-raised, #1e293b) 63%)");
    n->style.extra.emplace_back("background-size","200% 100%");
    n->style.extra.emplace_back("animation","wa-ui-shimmer 1.4s ease infinite");
    finalize(*n); return n;
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation
// ─────────────────────────────────────────────────────────────────────────────

/// `tabs(active, {{id,"Label"}…}, on_select)` — a themed tab bar. `on_select`
/// maps a tab id to a Msg. The active tab is underlined in the primary colour.
template <typename ToMsg>
NodeRef tabs(int active, std::vector<std::pair<int,std::string>> items, ToMsg to_msg){
    std::vector<NodeRef> tab_nodes;
    for (auto& [id, label] : items){
        bool on = (id == active);
        tab_nodes.push_back(
            text(label) | pad_x(14) | pad_y(10) | pointer | semibold
              | detail::raw_css("font-size","14px")
              | (on ? fg_text : fg_muted)
              | detail::raw_css("border-bottom", on ? "2px solid var(--wa-primary, #6366f1)" : "2px solid transparent")
              | transition("color .15s ease, border-color .15s ease")
              | tap(to_msg(id)));
    }
    auto bar = box(); bar->kids = std::move(tab_nodes); bar->style.flow = Flow::row;
    bar->style.extra.emplace_back("border-bottom","1px solid var(--wa-line, rgba(255,255,255,.10))");
    finalize(*bar); return bar;
}

// ─────────────────────────────────────────────────────────────────────
// Floating layers (composed on the core overlay/anchored primitives)
// ─────────────────────────────────────────────────────────────────────────────

/// `popover(open, trigger, panel, place)` — anchored dropdown with frosted panel
/// chrome + auto show/hide. Built on the core `anchored` primitive.
inline NodeRef popover(bool open, NodeRef trigger, NodeRef panel, std::string place="bottom-right"){
    auto styled = open ? (panel | frost(14) | round(12) | pad(6) | elevation(4)
                                | detail::raw_css("min-width","11rem") | pop_in(160))
                       : box();
    return anchored(std::move(trigger), std::move(styled), place);
}

/// `tooltip(trigger, text)` — a hover tooltip. Uses a registered group-hover CSS
/// rule so it needs no state in your Model.
inline NodeRef tooltip(NodeRef trigger, std::string tip, std::string place="top"){
    // one global rule: reveal a [data-wa-tip] child when its wrapper is hovered.
    assets().css(".wa-tip-wrap [data-wa-tip]{opacity:0;transition:opacity .15s ease}"
                 ".wa-tip-wrap:hover [data-wa-tip]{opacity:1}");
    auto bubble = text(std::move(tip))
        | pad_x(9) | pad_y(6) | round(8) | detail::raw_css("font-size","12.5px")
        | detail::raw_css("background","var(--wa-raised, #1e293b)") | fg_text
        | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("box-shadow","0 8px 24px rgba(0,0,0,.4)")
        | detail::raw_css("white-space","nowrap") | detail::raw_css("pointer-events","none")
        | attr("data-wa-tip","");
    return anchored(std::move(trigger), std::move(bubble), place)
         | attr("class","wa-tip-wrap") | detail::raw_css("cursor","default");
}

/// `dialog(open, close_msg, panel_children…)` — a complete modal: dimmed backdrop
/// that closes on click, a stopped panel so content clicks don't close it, plus
/// frosted chrome + a pop-in entrance. Built on the core `overlay` primitive.
template <typename Msg, typename... Cs>
NodeRef dialog(bool open, Msg close_msg, Cs... panel_children){
    if (!open) return box();
    auto panel = col(std::move(panel_children)...)
        | gap(16) | pad(28) | round(20)
        | detail::raw_css("background", "var(--wa-surface, #141b2e)")
        | detail::raw_css("border", "1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("box-shadow", "0 24px 70px rgba(0,0,0,.55), 0 0 0 1px rgba(255,255,255,.04)")
        | detail::raw_css("max-width", "28rem") | detail::raw_css("width", "100%")
        | stop() | pop_in(180);
    return overlay(std::move(panel)) | tap(close_msg);
}

/// `toast_layer(nodes)` — a fixed, non-interactive top-right stack for toasts.
inline NodeRef toast_layer(std::vector<NodeRef> toasts){
    auto n = box(); n->kids = std::move(toasts); n->style.flow = Flow::col;
    auto& s = n->style;
    s.pos = Pos::fixed;
    s.top = {16,Unit::px}; s.right = {16,Unit::px};
    s.has_z = true; s.z = 1100; s.gap = {10,Unit::px};
    s.extra.emplace_back("pointer-events", "none");
    finalize(*n); return n;
}
/// `toast(text, tone)` — a single toast card for the layer above.
inline NodeRef toast(std::string message, Tone tone = Tone::neutral){
    return row(dot(tone), text(std::move(message)) | fg_text | detail::raw_css("font-size","14px"))
        | gap(10) | center | pad_x(16) | pad_y(12) | round(12)
        | detail::raw_css("background","var(--wa-surface, #141b2e)")
        | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("box-shadow","0 10px 30px rgba(0,0,0,.4)")
        | detail::raw_css("pointer-events","auto") | slide_in(220);
}

} // namespace waya::ui
