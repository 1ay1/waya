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
#include "../surface/layout.hpp"

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
         | text_size(14) | detail::raw_css("line-height","1")
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
        return bg_transparent() | fg_text
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
/// above any input node. `hint` shows a helper line below when non-empty.
inline NodeRef field(std::string label, NodeRef control, std::string hint = ""){
    auto lab = text(std::move(label)) | fg_muted | text_size(12.5f) | semibold
             | detail::raw_css("letter-spacing",".02em");
    // Wrap the label + control in a <label> element: clicking the label text
    // focuses the control (implicit association), and screen readers announce
    // them together — without needing to thread a matching id/for through.
    auto col_ = col(std::move(lab), std::move(control)) | gap(6) | as("label")
              | detail::raw_css("display","flex") | pointer;
    if (!hint.empty())
        col_ = col(std::move(col_), text(std::move(hint)) | fg_muted | text_size(12)) | gap(6);
    return col_;
}

/// `field_invalid("Email", control, "That email is taken")` — the ERROR state of
/// a field: the same labelled control, but the message renders in the danger
/// colour with an alert role (announced by screen readers), and the control is
/// wrapped so it reads as invalid. Drive it from your Model — a validation error
/// is just state — so `f.error.empty() ? field(…) : field_invalid(…, f.error)`.
inline NodeRef field_invalid(std::string label, NodeRef control, std::string error){
    auto lab = text(std::move(label)) | fg_muted | text_size(12.5f) | semibold
             | detail::raw_css("letter-spacing",".02em");
    // a red focus ring / border on the control so the invalid field stands out.
    control = std::move(control)
        | detail::raw_css("border-color", "#ef4444")
        | detail::raw_css("box-shadow", "0 0 0 3px rgba(239,68,68,.18)")
        | attr("aria-invalid", "true");
    auto msg = text(std::move(error)) | detail::raw_css("color","#ef4444")
             | text_size(12) | role("alert");
    return col(std::move(lab), std::move(control), std::move(msg)) | gap(6) | as("label")
         | detail::raw_css("display","flex") | pointer;
}

/// `styled_input(input_node)` — apply the library's input chrome to a raw core
/// input()/textarea()/select() so all your fields match. Themed, focus-ringed.
inline Mod input_skin(){
    return pad_x(12) | pad_y(10) | round(10) | fg_text
         | detail::raw_css("background","var(--wa-bg, rgba(0,0,0,.25))")
         | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.14))")
         | text_size(14) | detail::raw_css("width","100%") | detail::raw_css("outline","none")
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
         | text_size(12) | semibold | detail::raw_css("white-space","nowrap")
         | detail::raw_css("line-height","1.4");
}

/// `dot(tone)` — a tiny status dot (online/away/error).
inline NodeRef dot(Tone tone = Tone::success){
    auto [c, _] = impl::tone_colors(tone); (void)_;
    return box() | w(8) | h(8) | round(999) | detail::raw_css("background", c) | no_shrink;
}

/// `avatar("AB")` — a circular initials avatar. `avatar_img(url)` for a photo.
inline NodeRef avatar(std::string initials, float d = 36){
    return text(std::move(initials)) | size(d) | round(999) | center
         | bg_primary | fg_on_primary | semibold
         | detail::raw_css("font-size", std::to_string((int)(d * 0.4f)) + "px")
         | no_shrink | detail::raw_css("text-transform","uppercase");
}
inline NodeRef avatar_img(std::string url, float d = 36){
    return image(std::move(url)) | size(d) | round(999)
         | detail::raw_css("object-fit","cover") | no_shrink;
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
              | text_size(14)
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
        | pad_x(9) | pad_y(6) | round(8) | text_size(12.5f)
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
    if (!open) return nothing();
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
    return row(dot(tone), text(std::move(message)) | fg_text | text_size(14))
        | gap(10) | center | pad_x(16) | pad_y(12) | round(12)
        | detail::raw_css("background","var(--wa-surface, #141b2e)")
        | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
        | detail::raw_css("box-shadow","0 10px 30px rgba(0,0,0,.4)")
        | detail::raw_css("pointer-events","auto") | slide_in(220);
}

// ── dashboard primitives ────────────────────────────────────────────
// The pieces you reach for when building an instrument/monitoring UI: a
// titled panel with an accent rail, a labelled slider row, and a media
// scope that fills its panel. All token-themed (fg_text / fg_muted /
// bg_surface / --wa-line), so `theme(t)` on the root recolours them.

/// `panel(title, subtitle, body)` — a card with a titled header: an accent
/// rail, a title + subtitle stack, and the body below. Pass `accent` to tint
/// the rail; pass `header_extra` for a top-right control (an expand button, a
/// live badge). The body GROWS to fill the card height, so panels sharing a
/// grid row line up and content like a bar chart soaks up the space.
///
///   panel("CPU", "last 60s", spark) | h_full
///   panel("Map", "live", scope(svg), 0x22d3ee, expand_btn)
inline NodeRef panel(std::string title, std::string subtitle, NodeRef body,
                     std::uint32_t accent = 0x6d7cff, NodeRef header_extra = nullptr){
    auto rail = box() | detail::raw_css("width","3px") | detail::raw_css("height","18px")
        | round(2) | detail::raw_css("background", detail::hexstr(accent))
        | glow(accent, 10);
    auto titles = col(
        text(std::move(title)) | fg_text | text_size(14)
            | weight(Weight::bold) | detail::raw_css("letter-spacing","-0.01em"),
        when(!subtitle.empty(), [&]{
            return text(std::move(subtitle)) | fg_muted | text_size(11); })
    ) | gap(2);
    auto head = row(rail, titles, box() | grows,
                    when(header_extra != nullptr, [&]{ return header_extra; }))
        | gap(10) | items_center | w_full;
    return col(head, box(std::move(body)) | grows | min_h(0) | w_full)
        | gap(14) | pad(18) | round(16) | h_full
        | detail::raw_css("background","var(--wa-surface, #0b1120)")
        | border_token()
        | detail::raw_css("box-shadow","0 24px 60px -30px rgba(0,0,0,.7)");
}

/// `slider_row(label, control, value_text)` — the canonical dial: a label on
/// the left, a live value on the right, and the control (a `range_input`,
/// usually) full-width beneath. Wire the control yourself so the Msg stays
/// yours:
///
///   slider_row("Gain", range_input(v,0,100) | on_input(...) , v + " dB")
inline NodeRef slider_row(std::string label, NodeRef control, std::string value_text = "",
                          std::uint32_t value_color = 0x22d3ee){
    return col(
        row(text(std::move(label)) | fg_muted | text_size(12) | medium,
            box() | grows,
            when(!value_text.empty(), [&]{
                return text(std::move(value_text))
                    | detail::raw_css("color", detail::hexstr(value_color))
                    | text_size(12) | weight(Weight::bold) | tabular_nums; }))
            | items_center | w_full,
        std::move(control) | w_full
    ) | gap(6) | w_full;
}

/// `scope(content)` — a framed media viewport for an SVG / canvas / <img> that
/// should FILL its panel. Clips, rounds, sizes to the panel, and gives the
/// child `width/height:100%`. The everyday "put this visualisation in a card"
/// container. `min_px` sets a height floor so it never collapses.
inline NodeRef scope(NodeRef content, int min_px = 180){
    return box(std::move(content) | w_full | h_full | detail::raw_css("line-height","0"))
        | w_full | grows | round(12) | clip_content
        | detail::raw_css("min-height", std::to_string(min_px) + "px")
        | detail::raw_css("background","var(--wa-bg, #060a14)")
        | border_token();
}

} // namespace waya::ui