#pragma once
/// \file ui/patterns.hpp
/// High-level PATTERNS — the page-shaped building blocks every real app needs,
/// so you write `page_header(...)`, `stat(...)`, `nav_bar(...)`, `hero(...)`
/// instead of hand-assembling forty lines of rows, columns and mods each time.
///
/// Everything here is (like the rest of waya/ui) just a function returning a
/// NodeRef, composed from the public core + the lower components. Read one, copy
/// it, own your variant. They read the theme tokens so they recolour with
/// `theme(t)`, and they're plain nodes so `| ...` any extra mod still applies.
///
///   #include <waya/ui.hpp>
///   using namespace waya::surface;
///   using namespace waya::ui;
///
///   col(
///     page_header("Dashboard", "Everything at a glance", button("New", New{})),
///     row(stat("Revenue", "$48.2k", "+12%", Tone::success),
///         stat("Users",   "1,284",  "+4%",  Tone::primary)) | gap(16) | wrap,
///     section("Recent", list_row(avatar("AK"), "Alex Kim", "2m ago")));

#include "components.hpp"
#include "icons.hpp"
#include "../surface/sugar.hpp"

namespace waya::ui {

using namespace waya::surface;

// ─────────────────────────────────────────────────────────────────────────────
// Page structure
// ─────────────────────────────────────────────────────────────────────────────

/// `page_header("Title", "subtitle", actions…)` — the top of a screen: a big
/// title + muted subtitle on the left, optional action nodes pushed right.
template <typename... Actions>
NodeRef page_header(std::string title, std::string subtitle = "", Actions... actions){
    auto titles = col(
        text(std::move(title)) | fg_text | detail::raw_css("font-size","clamp(22px,3vw,30px)")
            | weight(Weight::black) | detail::raw_css("letter-spacing","-0.02em"),
        when(!subtitle.empty(), [&]{ return text(subtitle) | fg_muted | detail::raw_css("font-size","14px"); })
    ) | gap(4);
    std::vector<NodeRef> acts{ std::move(actions)... };
    return row(std::move(titles), box() | grow(),
               row_(std::move(acts)) | gap(10) | items_center)
        | items_center | gap(20) | wrap | w_full;
}

/// `section("Heading", children…)` — a titled block: a small uppercase heading
/// with a hairline, then the content. The everyday "group of things under a
/// label" layout.
template <typename... Cs>
NodeRef section(std::string heading, Cs... cs){
    return col(
        row(text(std::move(heading)) | fg_muted | detail::raw_css("font-size","12px")
                | weight(Weight::bold) | detail::raw_css("letter-spacing",".1em")
                | detail::raw_css("text-transform","uppercase"),
            box() | grow(), divider()) | gap(14) | items_center,
        col(std::move(cs)...) | gap(12)
    ) | gap(14) | w_full;
}

/// `nav_bar(brand, links…)` — a top navigation bar: a brand node on the left,
/// links/actions pushed to the right. Sticky, hairline underline, blurred.
template <typename... Items>
NodeRef nav_bar(NodeRef brand, Items... items){
    std::vector<NodeRef> right{ std::move(items)... };
    return row(std::move(brand), box() | grow(),
               row_(std::move(right)) | gap(24) | items_center)
        | items_center | w_full | pad_x(24) | pad_y(14)
        | sticky_top(0) | z(50)
        | detail::raw_css("background","color-mix(in srgb, var(--wa-bg, #0b1020) 82%, transparent)")
        | detail::raw_css("backdrop-filter","blur(12px)")
        | detail::raw_css("-webkit-backdrop-filter","blur(12px)")
        | detail::raw_css("border-bottom","1px solid var(--wa-line, rgba(255,255,255,.08))");
}

/// `nav_link("Docs")` — a nav-bar link: muted, brightens on hover. Add `tap`/`href`.
inline NodeRef nav_link(std::string label){
    return text(std::move(label)) | fg_muted | detail::raw_css("font-size","14px") | medium
         | pointer | transition("color .15s ease") | on(Hover, fg_text);
}

// ─────────────────────────────────────────────────────────────────────────────
// Data display
// ─────────────────────────────────────────────────────────────────────────────

/// `stat("Label", "Value")` — a KPI cell: a muted label over a big number, with
/// an optional coloured delta chip (e.g. "+12%"). The dashboard workhorse.
inline NodeRef stat(std::string label, std::string value, std::string delta = "", Tone delta_tone = Tone::success){
    return col(
        row(text(std::move(label)) | fg_muted | detail::raw_css("font-size","13px") | medium,
            when(!delta.empty(), [&]{ return badge(delta, delta_tone); })) | items_center | gap(8),
        text(std::move(value)) | fg_text | detail::raw_css("font-size","30px") | weight(Weight::black)
            | tabular_nums | detail::raw_css("letter-spacing","-0.02em")
    ) | gap(6);
}

/// `metric_card(...)` — `stat` wrapped in a card, optionally with a chart/sparkline
/// node under it. `row(metric_card(...), metric_card(...)) | gap(16) | wrap`.
inline NodeRef metric_card(std::string label, std::string value, std::string delta = "",
                           Tone delta_tone = Tone::success, NodeRef chart = nullptr){
    auto body = col(stat(std::move(label), std::move(value), std::move(delta), delta_tone),
                    when(chart != nullptr, [&]{ return chart; }))
              | gap(14);
    return card(std::move(body)) | grow() | detail::raw_css("flex","1 1 200px") | min_w(200);
}

/// `list_row(leading, title, trailing)` — a single row of a list: an optional
/// leading node (avatar/icon), a title (+ optional subtitle), and an optional
/// trailing node (a time, a badge, a chevron). Hover-highlights. Absent leading/
/// trailing add NO node (and so no phantom gap), not an empty placeholder.
inline NodeRef list_row(NodeRef leading, std::string title, std::string subtitle = "", NodeRef trailing = nullptr){
    auto titles = col(
        text(std::move(title)) | fg_text | detail::raw_css("font-size","14px") | semibold,
        when(!subtitle.empty(), [&]{ return text(subtitle) | fg_muted | detail::raw_css("font-size","13px"); })
    ) | gap(2);
    std::vector<NodeRef> kids;
    if (leading) kids.push_back(std::move(leading));
    kids.push_back(std::move(titles));
    kids.push_back(box() | grow());
    if (trailing) kids.push_back(std::move(trailing));
    return row_(std::move(kids)) | gap(12) | items_center | pad_x(12) | pad_y(10) | round(10)
      | transition("background-color .12s ease")
      | on(Hover, detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.04))"));
}

/// `key_value("Label", "Value")` — a label:value row (a details/definition list).
inline NodeRef key_value(std::string k, std::string v){
    return row(text(std::move(k)) | fg_muted | detail::raw_css("font-size","14px"),
               box() | grow(),
               text(std::move(v)) | fg_text | detail::raw_css("font-size","14px") | medium | tabular_nums)
        | items_center | gap(16) | pad_y(8);
}

// ─────────────────────────────────────────────────────────────────────────────
// Small chrome
// ─────────────────────────────────────────────────────────────────────────────

/// `tag("design")` — a subtle outlined label chip (categories, filters). Lighter
/// than a `badge`. Add `tap` to make it a filter.
inline NodeRef tag(std::string label){
    return text(std::move(label)) | fg_muted | detail::raw_css("font-size","12.5px") | medium
         | pad_x(10) | pad_y(4) | round(8)
         | detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.04))")
         | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
         | detail::raw_css("white-space","nowrap");
}

/// `kbd("Cmd")` — a keyboard-key cap (shortcut hints). `row(kbd("Cmd"), kbd("K"))`.
inline NodeRef kbd(std::string key){
    return text(std::move(key)) | fg_text | detail::raw_css("font-size","12px") | semibold
         | pad_x(7) | pad_y(3) | round(6) | mono
         | detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.06))")
         | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.14))")
         | detail::raw_css("box-shadow","0 1px 0 var(--wa-line, rgba(255,255,255,.10))");
}

/// `banner("message", tone)` — a full-width inline alert bar with an icon and a
/// tinted background. For success/warning/error/info notices.
inline NodeRef banner(std::string message, Tone tone = Tone::primary){
    auto [accent, _] = impl::tone_colors(tone); (void)_;
    const char* ico = tone == Tone::danger ? "alert" : tone == Tone::warning ? "alert"
                    : tone == Tone::success ? "check" : "info";
    return row(icon(ico, 18) | detail::raw_css("color", accent),
               text(std::move(message)) | fg_text | detail::raw_css("font-size","14px") | leading(1.5f))
        | gap(12) | items_center | pad_x(16) | pad_y(12) | round(12) | w_full
        | detail::raw_css("background","color-mix(in srgb," + std::string(accent) + " 12%, transparent)")
        | detail::raw_css("border","1px solid color-mix(in srgb," + std::string(accent) + " 35%, transparent)");
}

/// `empty_state("No results", "Try a different search", action?)` — the friendly
/// placeholder for an empty list/search: a centred icon, title, hint, and an
/// optional call-to-action node.
inline NodeRef empty_state(std::string title, std::string hint = "", NodeRef action = nullptr,
                           std::string ico = "search"){
    return col(
        box(icon(ico, 26) | fg_muted) | size(56) | center | round(999)
            | detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.04))"),
        text(std::move(title)) | fg_text | detail::raw_css("font-size","17px") | semibold,
        when(!hint.empty(), [&]{ return text(hint) | fg_muted | detail::raw_css("font-size","14px")
                                        | leading(1.6f) | text_center | max_w(360); }),
        when(action != nullptr, [&]{ return action; })
    ) | gap(14) | items_center | pad(48) | w_full;
}

/// `code_block("...", "cpp")` — a monospace code panel with a language tag.
inline NodeRef code_block(std::string code, std::string lang = ""){
    auto bar = when(!lang.empty(), [&]{
        return row(text(lang) | fg_muted | detail::raw_css("font-size","12px") | mono, box() | grow())
             | pad_x(14) | pad_y(8)
             | detail::raw_css("border-bottom","1px solid var(--wa-line, rgba(255,255,255,.08))");
    });
    return col(bar,
        text(std::move(code)) | fg_text | mono | detail::raw_css("font-size","13px")
            | leading(1.6f) | pre_wrap | pad(16))
        | round(12) | detail::raw_css("overflow","hidden")
        | detail::raw_css("background","var(--wa-bg, rgba(0,0,0,.3))")
        | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))");
}

/// `feature_card(icon, "Title", "body")` — a marketing feature cell: an accented icon
/// tile over a title and a paragraph. `auto_grid(280)` a row of them.
inline NodeRef feature_card(std::string ico, std::string title, std::string body, Tone tone = Tone::primary){
    auto [accent, _] = impl::tone_colors(tone); (void)_;
    return col(
        box(icon(ico, 20) | detail::raw_css("color", accent)) | size(46) | center | round(12)
            | detail::raw_css("background","color-mix(in srgb," + std::string(accent) + " 14%, transparent)"),
        text(std::move(title)) | fg_text | detail::raw_css("font-size","17px") | semibold,
        text(std::move(body)) | fg_muted | detail::raw_css("font-size","14px") | leading(1.65f)
    ) | gap(14) | pad(24) | round(16) | grow()
      | bg_surface | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.08))");
}

// ───────────────────────────────────────────────────────────────────────
// Forms — a labelled control in ONE call. Each maps its live value to your Msg
// (a mapper fn `std::string -> Msg`, or a bare Msg for a tap/toggle) so you
// never wire on_input by hand. All wear the shared input_skin so a form matches.
// ───────────────────────────────────────────────────────────────────────

/// `text_field("Email", value, to_msg, "placeholder", "hint")` — a labelled text
/// input. `to_msg` maps the typed text to a Msg: `[](std::string v){ return SetEmail{v}; }`.
template <typename ToMsg>
NodeRef text_field(std::string label, std::string value, ToMsg to_msg,
                   std::string placeholder_ = "", std::string hint = "", std::string kind = "text"){
    auto ctrl = input(std::move(value)) | type(std::move(kind)) | input_skin() | on_input(to_msg);
    if (!placeholder_.empty()) ctrl = ctrl | placeholder(std::move(placeholder_));
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

/// `email_field` / `password_field` — text_field with the right input type
/// (correct mobile keyboard, masking, autofill).
template <typename ToMsg>
NodeRef email_field(std::string label, std::string value, ToMsg to_msg, std::string ph = "", std::string hint = ""){
    return text_field(std::move(label), std::move(value), to_msg, std::move(ph), std::move(hint), "email");
}
template <typename ToMsg>
NodeRef password_field(std::string label, std::string value, ToMsg to_msg, std::string ph = "", std::string hint = ""){
    return text_field(std::move(label), std::move(value), to_msg, std::move(ph), std::move(hint), "password");
}

/// `textarea_field("Bio", value, to_msg)` — a labelled multi-line field.
template <typename ToMsg>
NodeRef textarea_field(std::string label, std::string value, ToMsg to_msg,
                       std::string placeholder_ = "", std::string hint = ""){
    auto ctrl = textarea(std::move(value)) | input_skin()
              | detail::raw_css("min-height","96px") | detail::raw_css("resize","vertical") | on_input(to_msg);
    if (!placeholder_.empty()) ctrl = ctrl | placeholder(std::move(placeholder_));
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

/// `select_field("Plan", options, chosen, to_msg)` — a labelled dropdown.
template <typename ToMsg>
NodeRef select_field(std::string label, std::vector<Opt> options, std::string chosen,
                     ToMsg to_msg, std::string hint = ""){
    auto ctrl = select(std::move(options), std::move(chosen)) | input_skin() | on_change(to_msg);
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

/// `switch_field("Notifications", "email + push", on, Msg{})` — a settings row:
/// title + description on the left, a toggle on the right. Tapping the row (or
/// the toggle) fires the message.
template <typename Msg>
NodeRef switch_field(std::string title, std::string desc, bool on, Msg msg){
    auto titles = col(
        text(std::move(title)) | fg_text | detail::raw_css("font-size","14px") | semibold,
        when(!desc.empty(), [&]{ return text(desc) | fg_muted | detail::raw_css("font-size","13px"); })
    ) | gap(2);
    return row(std::move(titles), box() | grow(), toggle(on, msg))
        | items_center | gap(16) | pad_y(12);
}

/// `checkbox_field("I agree", on, Msg{})` — a checkbox + inline label; clicking
/// either toggles it. The message is wired via the checkbox's change event only
/// (NOT a tap on both the box and the <label>, which native label-forwarding
/// would fire twice — cancelling the toggle).
template <typename Msg>
NodeRef checkbox_field(std::string label, bool on, Msg msg){
    return row(checkbox(on) | on_change([msg](std::string){ return msg; }),
               text(std::move(label)) | fg_text | detail::raw_css("font-size","14px"))
        | gap(10) | items_center | as("label") | detail::raw_css("cursor","pointer");
}

/// `form_actions(buttons…)` — a right-aligned button bar for a form footer.
template <typename... Buttons>
NodeRef form_actions(Buttons... buttons){
    std::vector<NodeRef> b{ std::move(buttons)... };
    return row(box() | grow(), row_(std::move(b)) | gap(10) | items_center)
        | items_center | w_full | detail::raw_css("padding-top", "16px");
}

/// `hero_section("Big headline", "Supporting copy", actions…)` — a centred hero block:
/// a fluid headline, a max-width subhead, and a row of CTA nodes. The top of a
/// landing page in one call.
template <typename... Actions>
NodeRef hero_section(std::string headline, std::string subhead = "", Actions... actions){
    std::vector<NodeRef> acts{ std::move(actions)... };
    return col(
        text(std::move(headline)) | fg_text | detail::raw_css("font-size","clamp(34px,6vw,64px)")
            | weight(Weight::black) | detail::raw_css("letter-spacing","-0.03em")
            | detail::raw_css("line-height","1.05") | text_center,
        when(!subhead.empty(), [&]{ return text(subhead) | fg_muted
            | detail::raw_css("font-size","clamp(16px,2.2vw,19px)") | leading(1.7f)
            | max_w(600) | text_center; }),
        when(!acts.empty(), [&]{ return row_(std::move(acts)) | gap(14) | items_center | wrap | justify_center; })
    ) | gap(24) | items_center | pad_y(48) | w_full;
}

// ───────────────────────────────────────────────────────────────────────
// App shells — the sidebar/dashboard layout, in one call. The sidebar collapses
// on phones (only_desktop) so the content goes full-width; the content column
// scrolls independently of the pinned rail.
// ───────────────────────────────────────────────────────────────────────

/// `sidebar_item(icon, "Label", active, msg)` — a nav row for a dashboard rail:
/// icon + label, highlighted when active, keyboard-reachable.
template <typename Msg>
NodeRef sidebar_item(std::string ico, std::string label, bool active, Msg msg){
    auto n = row(icon(ico, 18) | (active ? fg_text : fg_muted),
                 text(std::move(label)) | (active ? fg_text : fg_muted)
                     | detail::raw_css("font-size","14px") | (active ? semibold : medium))
        | gap(12) | items_center | pad_x(12) | pad_y(10) | round(9) | pointer
        | role("button") | tab_index(0) | tap(msg)
        | transition("background-color .15s ease, color .15s ease");
    if (active) n = n | detail::raw_css("background","color-mix(in srgb, var(--wa-primary,#6d7cff) 14%, transparent)");
    else n = n | on(Hover, detail::raw_css("background","var(--wa-raised, rgba(255,255,255,.04))"));
    return n;
}

/// `sidebar_shell(brand, nav_items, content)` — a full dashboard layout: a
/// fixed, sticky sidebar (brand on top, nav below) beside a scrolling content
/// column. On phones the sidebar is replaced by a sticky TOP BAR (brand + the
/// same nav items as a horizontal scrolling strip), so navigation is always
/// reachable — no dead-end full-width content. Wrap in your page root
/// (`| theme(t) | bg_page`).
inline NodeRef sidebar_shell(NodeRef brand, std::vector<NodeRef> nav_items, NodeRef content){
    // Desktop rail: brand + nav stacked, sticky, hidden on phones.
    std::vector<NodeRef> rail; rail.push_back(brand);
    rail.push_back(box() | detail::raw_css("height","8px"));
    for (auto& it : nav_items) rail.push_back(it);
    auto sidebar = col_(rail) | gap(4) | pad(18) | w(240)
        | detail::raw_css("height","100vh") | sticky_top(0)
        | detail::raw_css("background","var(--wa-surface, #0c1019)")
        | detail::raw_css("border-right","1px solid var(--wa-line, rgba(255,255,255,.08))")
        | only_desktop();

    // Mobile top bar: brand on the left, nav items as a horizontal strip that
    // scrolls if they overflow. Sticky at the top. Shown ONLY on phones.
    std::vector<NodeRef> strip;
    for (auto& it : nav_items) strip.push_back(it);
    auto mobile_nav = col(
        brand,
        row_(strip) | gap(6)
            | detail::raw_css("overflow-x","auto")
            | detail::raw_css("flex-wrap","nowrap")
            | detail::raw_css("-webkit-overflow-scrolling","touch")
    ) | gap(12) | pad(14) | w_full | sticky_top(0)
        | detail::raw_css("z-index","40")
        | detail::raw_css("background","var(--wa-surface, #0c1019)")
        | detail::raw_css("border-bottom","1px solid var(--wa-line, rgba(255,255,255,.08))")
        | only_phone();

    auto main_ = box(std::move(content)) | grow() | min_w(0)
        | pad_fluid(16, 32) | detail::raw_css("max-width","1400px");
    // Row on desktop (rail | content); on phone the rail collapses (only_desktop)
    // and the mobile_nav sits above content — so we stack the shell in a column
    // wrapper and put the desktop row inside.
    auto content_col = col(mobile_nav, row(sidebar, std::move(main_)) | items_stretch | grows)
        | detail::raw_css("min-height","100vh") | w_full;
    return content_col;
}

// ───────────────────────────────────────────────────────────────────────
// Dialogs — higher-level on top of the core `dialog()`.
// ───────────────────────────────────────────────────────────────────────

/// `confirm_dialog(open, "Title", "message", "Confirm", on_confirm, on_cancel)` —
/// a ready-made yes/no modal: title, body, and a Cancel + primary (or danger)
/// action bar. Nothing when closed.
template <typename ConfirmMsg, typename CancelMsg>
NodeRef confirm_dialog(bool open, std::string title, std::string message,
                       std::string confirm_label, ConfirmMsg on_confirm, CancelMsg on_cancel,
                       Variant confirm_variant = Variant::primary){
    return dialog(open, on_cancel,
        text(std::move(title)) | fg_text | detail::raw_css("font-size","18px") | weight(Weight::bold),
        text(std::move(message)) | fg_muted | detail::raw_css("font-size","14px") | leading(1.6f),
        row(box() | grow(),
            button("Cancel", on_cancel, Variant::secondary),
            button(std::move(confirm_label), on_confirm, confirm_variant)) | gap(10) | items_center
            | detail::raw_css("padding-top","8px"));
}

} // namespace waya::ui
