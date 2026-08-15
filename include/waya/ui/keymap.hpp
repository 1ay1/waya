#pragma once
/// \file ui/keymap.hpp
/// Keymap<Msg> — keyboard shortcuts as inspectable data.
///
/// `on_shortcut("mod+k", Open{})` wires ONE shortcut on ONE node. But an app's
/// shortcuts are scattered across the view that way, with no single source of
/// truth — so you can't render a "?" help overlay, can't group them, and it's
/// easy for two to collide silently.
///
/// `Keymap` makes the shortcut set one value: you declare bindings once
/// (combo -> label + Msg), wire the whole set with `wire(keymap)`, and render a
/// grouped help sheet with `shortcut_help(keymap)`. The bindings are DATA, so
/// the help view is generated from the same source that drives the behaviour —
/// they can't drift.
///
///   static Keymap<Msg> keys(){
///       return Keymap<Msg>{}
///           .bind("mod+k", "Command palette", OpenPalette{})
///           .bind("mod+/", "Toggle help",     ToggleHelp{})
///           .bind("g h",   "Go home",         Nav{"/"}, "Navigation")
///           .bind("g p",   "Go to profile",   Nav{"/me"}, "Navigation");
///   }
///
///   // view: one mod arms every binding; one call renders the help sheet.
///   app_shell | wire(keys())
///   m.help_open ? modal(shortcut_help(keys()), CloseHelp{}) : nothing()

#include "../surface/node.hpp"
#include "patterns.hpp"        // kbd()

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// One shortcut binding: the key combo, a human label, the Msg it fires, and an
/// optional group heading for the help sheet.
template <typename Msg>
struct Binding {
    std::string combo;      // "mod+k", "g h", "?"
    std::string label;      // "Command palette"
    Msg msg;
    std::string group;      // "" = ungrouped (shown first)
};

/// A set of keyboard bindings — declare once, wire once, document once.
template <typename Msg>
struct Keymap {
    std::vector<Binding<Msg>> bindings;

    /// Add a binding (fluent). `group` buckets it in the help sheet.
    Keymap& bind(std::string combo, std::string label, Msg msg, std::string group = ""){
        bindings.push_back({ std::move(combo), std::move(label), std::move(msg), std::move(group) });
        return *this;
    }
    [[nodiscard]] std::size_t size() const { return bindings.size(); }
    [[nodiscard]] bool empty() const { return bindings.empty(); }
};

/// `wire(keymap)` — a mod that arms EVERY binding as a global shortcut on the
/// node it's applied to (put it on your app shell / a mounted root). One call
/// replaces N scattered `on_shortcut(...)` mods.
template <typename Msg>
inline Mod wire(const Keymap<Msg>& km){
    std::vector<Mod> mods;
    mods.reserve(km.bindings.size());
    for (auto& b : km.bindings) mods.push_back(on_shortcut(b.combo, b.msg));
    return { [mods = std::move(mods)](Node& n){ for (auto& m : mods) m.apply(n); } };
}

namespace keymap_detail {
/// Render one combo string ("mod+shift+k") as a row of kbd caps.
inline NodeRef combo_caps(const std::string& combo){
    std::vector<NodeRef> caps;
    std::string cur;
    auto flush = [&]{ if(!cur.empty()){ caps.push_back(kbd(cur)); cur.clear(); } };
    for (char c : combo){
        if (c=='+' || c==' ') flush();   // '+' = chord, ' ' = sequence; both split
        else cur += c;
    }
    flush();
    return row_(std::move(caps)) | gap(4) | items_center;
}
}

/// `shortcut_help(keymap)` — a grouped, readable cheat-sheet of every binding,
/// generated straight from the keymap. Drop it in a modal on your "?" shortcut.
template <typename Msg>
inline NodeRef shortcut_help(const Keymap<Msg>& km){
    // preserve first-seen group order; "" (ungrouped) renders first.
    std::vector<std::string> order;
    for (auto& b : km.bindings)
        if (std::find(order.begin(), order.end(), b.group) == order.end())
            order.push_back(b.group);

    std::vector<NodeRef> sections;
    for (auto& g : order){
        std::vector<NodeRef> rows;
        for (auto& b : km.bindings){
            if (b.group != g) continue;
            rows.push_back(
                row(text(b.label) | fg_text | text_size(14),
                    box() | grows,
                    keymap_detail::combo_caps(b.combo))
                | items_center | gap(16) | pad_y(6));
        }
        if (!g.empty())
            sections.push_back(text(g) | fg_muted | text_size(12)
                               | uppercase | tracking_em(0.06f) | pad_y(4));
        sections.push_back(col_(std::move(rows)) | w_full);
    }
    return col(
        text("Keyboard shortcuts") | fg_text | font(18) | semibold | pad_y(4),
        col_(std::move(sections)) | w_full | gap(2)
    ) | gap(8) | pad(4) | min_w(320) | role("dialog") | aria_label("Keyboard shortcuts");
}

} // namespace waya::ui
