#pragma once
/// \file ui/pickers.hpp
/// Choosers: `color_field`, `swatch_picker`, `segmented`, and `breadcrumb`.
///
/// The core `color_input` is a native swatch; `date_field`/`time_field` already
/// wrap the native date/time pickers. These fill the remaining chooser gaps —
/// a colour field that pairs the native swatch with a hex text box, a preset
/// palette, a segmented toggle (one-of-N in a pill), and a breadcrumb trail.
/// Each is a pure function of state and reports the chosen value as a Msg.
///
///   struct Model { std::string color = "#6366f1"; int view = 0; };
///   struct SetColor { std::string v; }; struct SetView { int i; };
///
///   color_field("Accent", m.color, [](std::string v){ return SetColor{v}; })
///   swatch_picker(m.color, {"#ef4444","#f59e0b","#10b981","#6366f1"},
///                 [](std::string v){ return SetColor{v}; })
///   segmented(m.view, {"Day","Week","Month"}, [](int i){ return SetView{i}; })
///   breadcrumb({{"Home", Go{"/"}}, {"Docs", Go{"/docs"}}, {"Pickers", {}}})

#include "../surface/node.hpp"
#include "../surface/sugar.hpp"
#include "../surface/forms.hpp"
#include "components.hpp"

#include <optional>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── color_field — native swatch + a hex text box, kept in sync ────────────────
/// `color_field(label, value, to_msg, hint)` — a labelled colour chooser: the
/// native swatch on the left, a `#rrggbb` text box on the right. BOTH report
/// through `to_msg` (a value→Msg mapper), so typing a hex or opening the picker
/// both flow to the same update. The value is echoed as a preview.
template <typename ToMsg>
inline NodeRef color_field(std::string label, std::string value, ToMsg to_msg, std::string hint = "") {
    auto swatch = color_input(value)
        | w(38) | h(38) | round(10)
        | detail::raw_css("padding", "0") | detail::raw_css("border", "none")
        | detail::raw_css("cursor", "pointer") | detail::raw_css("background", "transparent")
        | on_change(to_msg) | aria_label("Colour swatch");
    auto hex = input(value) | input_skin() | grows
        | placeholder("#000000") | attr("maxlength", "7")
        | on_input(to_msg) | detail::raw_css("font-family", "ui-monospace, monospace");
    auto ctrl = row(swatch, hex) | items_center | gap(10) | w_full;
    return field(std::move(label), std::move(ctrl), std::move(hint));
}

// ── swatch_picker — a palette of preset colours ───────────────────────────────
namespace pick_detail {
/// Keep only a valid CSS colour shape (#rgb / #rrggbb / a bare css keyword of
/// [a-z]) so a palette string can NEVER break out of the `background:` value.
inline std::string safe_color(const std::string& s) {
    if (!s.empty() && s[0] == '#') {
        std::string out = "#";
        for (char c : s.substr(1))
            if ((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')) out += c;
        if (out.size()==4 || out.size()==7) return out;
        return "transparent";
    }
    for (char c : s) if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z'))) return "transparent";
    return s.empty() ? "transparent" : s;
}
} // namespace pick_detail

/// `swatch_picker(value, palette, to_msg, size)` — a grid of colour chips; the
/// chosen one gets a ring. Clicking a chip delivers `to_msg(hex)`. Values are
/// applied as a SANITISED background so a bad string simply shows nothing.
template <typename ToMsg>
inline NodeRef swatch_picker(const std::string& value, std::vector<std::string> palette,
                             ToMsg to_msg, float size = 28) {
    std::vector<NodeRef> chips;
    chips.reserve(palette.size());
    for (auto& hex : palette) {
        bool on = hex == value;
        auto chip = box()
            | w(size) | h(size) | round(999)
            | detail::raw_css("background", pick_detail::safe_color(hex))
            | pointer | tap(to_msg(hex))
            | detail::raw_css("transition", "transform .1s ease")
            | hover_lift(2)
            | aria_label("Colour " + hex);
        if (on) chip = chip
            | detail::raw_css("box-shadow", "0 0 0 2px var(--wa-bg,#0b1020), 0 0 0 4px var(--wa-primary,#6366f1)")
            | aria("pressed", "true");
        chips.push_back(std::move(chip));
    }
    auto rowc = box(); rowc->kids = std::move(chips); rowc->style.flow = Flow::row;
    rowc->style.wrap = Wrap::wrap; finalize(*rowc);
    return rowc | items_center | gap(10) | role("radiogroup") | aria_label("Colour palette");
}

// ── segmented — a one-of-N pill (a tab strip's compact cousin) ────────────────
/// `segmented(active, labels, to_msg)` — a pill split into N segments; the
/// active one is filled. Clicking segment i delivers `to_msg(i)`. Great for
/// small view switches (Day/Week/Month, List/Grid) where full tabs are heavy.
template <typename ToMsg>
inline NodeRef segmented(int active, std::vector<std::string> labels, ToMsg to_msg) {
    std::vector<NodeRef> segs;
    segs.reserve(labels.size());
    for (int i = 0; i < (int)labels.size(); ++i) {
        bool on = i == active;
        auto seg = box(text(labels[i]) | detail::raw_css("font-size", "13.5px") | medium)
            | pad_x(14) | pad_y(7) | round(8) | center | items_center
            | detail::raw_css("transition", "background .12s, color .12s")
            | pointer | tap(to_msg(i))
            | role("tab") | aria("selected", on ? "true" : "false");
        if (on) seg = seg | fg_text
            | detail::raw_css("background", "var(--wa-raised, rgba(255,255,255,.10))")
            | detail::raw_css("box-shadow", "0 1px 3px rgba(0,0,0,.35)") | semibold;
        else seg = seg | fg_muted | hover_bg(0xffffff, 0.05f);
        segs.push_back(std::move(seg));
    }
    auto rowc = box(); rowc->kids = std::move(segs); rowc->style.flow = Flow::row; finalize(*rowc);
    return rowc | items_center | gap(4) | pad(4) | round(11) | role("tablist")
        | detail::raw_css("background", "var(--wa-bg, rgba(0,0,0,.25))")
        | detail::raw_css("display", "inline-flex");
}

// ── breadcrumb — a navigation trail ───────────────────────────────────────────
/// One crumb: a label and an OPTIONAL Msg (the last/current crumb usually has
/// none — it's not a link). Build with `crumb("Docs", Go{"/docs"})` or
/// `crumb("Current")` for the trailing, non-clickable one.
struct Crumb {
    std::string label;
    int msg = -1;   // wire token; -1 = not a link
};
/// `crumb("Label", Msg{})` — a clickable crumb. `crumb("Label")` — the current
/// (non-clickable) crumb.
template <typename Msg>
inline Crumb crumb(std::string label, Msg m) {
    return { std::move(label), detail::register_msg<Msg>(std::move(m)) };
}
inline Crumb crumb(std::string label) { return { std::move(label), -1 }; }

/// `breadcrumb({crumb("Home", Go{"/"}), crumb("Docs", Go{"/docs"}), crumb("Here")})`
/// — a "Home › Docs › Here" trail. Linked crumbs are tappable and highlight on
/// hover; the final crumb is muted plain text. Separators are non-selectable.
inline NodeRef breadcrumb(std::vector<Crumb> crumbs) {
    std::vector<NodeRef> parts;
    parts.reserve(crumbs.size() * 2);
    for (std::size_t i = 0; i < crumbs.size(); ++i) {
        if (i) parts.push_back(text("\xe2\x80\xba")   // › (U+203A)
            | fg_muted | detail::raw_css("user-select", "none")
            | detail::raw_css("font-size", "13px") | detail::raw_css("opacity", "0.6"));
        auto& c = crumbs[i];
        bool linked = c.msg >= 0;
        auto node = text(c.label) | detail::raw_css("font-size", "13.5px");
        if (linked) {
            node->on_tap = c.msg;
            node = node | fg_muted | pointer | role("link")
                 | detail::raw_css("transition", "color .12s")
                 | on(Hover, fg_text);
        } else {
            node = node | fg_text | medium | aria("current", "page");
        }
        parts.push_back(std::move(node));
    }
    auto rowc = box(); rowc->kids = std::move(parts); rowc->style.flow = Flow::row; finalize(*rowc);
    return rowc | items_center | gap(8) | role("navigation") | aria_label("Breadcrumb");
}

} // namespace waya::ui
