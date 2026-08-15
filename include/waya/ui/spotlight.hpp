#pragma once
/// \file ui/spotlight.hpp
/// spotlight — a generic Cmd+K launcher over ARBITRARY items (not just a Keymap).
///
/// `command_palette` is the launcher over a `Keymap`. But often the searchable
/// set isn't keyboard shortcuts — it's files, people, settings pages, recent
/// docs. `spotlight` is the same fuzzy launcher over any list of `SpotItem`s:
/// each has a label, an optional category (grouping/sublabel), an optional icon,
/// and extra keyword text folded into the match. It reuses the SAME
/// `fuzzy_score` as the command palette, so ranking feels identical.
///
///   struct Model { bool open=false; std::string q; int sel=0; };
///   struct Query{std::string v;}; struct Move{int d;}; struct Run{std::string id;}; struct Close{};
///
///   std::vector<SpotItem> items = {
///       { "Profile settings", "Settings", "settings", "account preferences", "s-profile" },
///       { "Invite teammate",  "People",   "plus",     "add member",          "invite"    },
///   };
///
///   m.open ? spotlight(items, m.q, m.sel,
///               [](std::string v){ return Query{v}; },
///               [](std::string id){ return Run{id}; },   // the chosen item's id
///               Close{})
///          : nothing()
///
/// `onRun(id)` carries the chosen item's `id` (your stable handle), so update()
/// dispatches by id. Selection is clamped/wrapped; ↑/↓ + Enter are wired.

#include "../surface/node.hpp"
#include "command_palette.hpp"   // reuse palette_detail::fuzzy_score + lower
#include "components.hpp"
#include "icons.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

// ── item ──────────────────────────────────────────────────────────────────────
/// A searchable entry. `label` is shown + matched; `category` is a muted
/// sublabel/group; `icon` is an optional glyph name; `keywords` is extra text
/// folded into the fuzzy match but not shown (aliases, synonyms); `id` is the
/// stable handle `onRun` reports.
struct SpotItem {
    std::string label;
    std::string category;   // "" => none
    std::string icon;       // "" => none (an icon name from ui/icons)
    std::string keywords;   // "" => none; searched, not shown
    std::string id;         // the handle passed to onRun
    bool operator==(const SpotItem&) const = default;
};

// ── ranking (pure, testable) ──────────────────────────────────────────────────
/// The indices of `items` that match `query`, best first. Matches against
/// "label category keywords" so an alias or group name also finds an item.
/// Reuses the command palette's fuzzy scorer for identical feel.
inline std::vector<int> spotlight_matches(const std::vector<SpotItem>& items, const std::string& query){
    std::string q = palette_detail::lower(query);
    std::vector<std::pair<int,int>> scored;   // {score, index}
    for (int i = 0; i < (int)items.size(); ++i){
        const auto& it = items[i];
        std::string hay = palette_detail::lower(it.label + " " + it.category + " " + it.keywords);
        int s = palette_detail::fuzzy_score(hay, q);
        if (s >= 0) scored.push_back({ s, i });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](auto& a, auto& b){ return a.first > b.first; });
    std::vector<int> out;
    out.reserve(scored.size());
    for (auto& [_, i] : scored) out.push_back(i);
    return out;
}

// ── view ──────────────────────────────────────────────────────────────────────
/// `spotlight(items, query, selected, onQuery, onRun, onClose)` — the launcher.
/// `query`/`selected` are your model state; `onQuery` maps the search text to a
/// Msg; `onRun(id)` maps the chosen item's id to your Msg; `onClose` fires on
/// Escape / backdrop click. Selection wraps and highlights.
template <typename OnQuery, typename OnRun, typename OnClose>
inline NodeRef spotlight(const std::vector<SpotItem>& items, const std::string& query, int selected,
                         OnQuery onQuery, OnRun onRun, OnClose onClose){
    auto ranked = spotlight_matches(items, query);
    int n = (int)ranked.size();
    if (n > 0) selected = ((selected % n) + n) % n;

    std::vector<NodeRef> rows;
    rows.reserve(ranked.size());
    for (int i = 0; i < n; ++i){
        const auto& it = items[ranked[i]];
        bool on = (i == selected);

        std::vector<NodeRef> line;
        if (!it.icon.empty())
            line.push_back(box(icon(it.icon, 17)) | (on ? fg_text : fg_muted) | detail::raw_css("line-height","0"));
        line.push_back(
            col(text(it.label) | (on ? fg_text : fg_muted) | detail::raw_css("font-size","14px"),
                it.category.empty() ? nothing()
                    : (text(it.category) | fg_muted | detail::raw_css("font-size","11.5px") | detail::raw_css("opacity","0.75")))
            | gap(1));
        line.push_back(box() | grows);

        auto rowc = box(); rowc->kids = std::move(line); rowc->style.flow = Flow::row; finalize(*rowc);
        rows.push_back(rowc
            | items_center | gap(11) | pad_x(12) | pad_y(8) | round(8) | w_full
            | (on ? bg(0x2a3350) : Mod{})
            | pointer | role("option") | aria_selected(on)
            | key("spot-" + std::to_string(ranked[i]))
            | tap(onRun(it.id)));
    }

    auto search = input(query)
        | placeholder("Search\xe2\x80\xa6")
        | anchor("spotlight") | autofocus()
        | w_full | pad_x(14) | pad_y(12) | detail::raw_css("font-size","15px")
        | detail::raw_css("background","transparent")
        | detail::raw_css("border","none") | detail::raw_css("outline","none")
        | fg_text | on_input(onQuery)
        | on_escape(onClose)
        | aria_label("Search");

    auto panel = col(
        search,
        box() | h(1) | w_full | detail::raw_css("background","var(--wa-line, rgba(255,255,255,.08))"),
        (n > 0
            ? (col_(std::move(rows)) | w_full | gap(2) | pad(8)
               | detail::raw_css("max-height","340px") | detail::raw_css("overflow-y","auto"))
            : (text("No results") | fg_muted | pad(20) | text_align(Justify::center) | w_full))
    ) | w_full | max_w(560) | round(14)
      | detail::raw_css("background","var(--wa-surface, #141b2e)")
      | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
      | detail::raw_css("box-shadow","0 24px 70px -20px rgba(0,0,0,.7)")
      | role("listbox") | aria_label("Search results")
      | stop();

    return box(panel)
        | surface::dialog() | pin() | absolute()
        | detail::raw_css("display","flex")
        | detail::raw_css("justify-content","center")
        | detail::raw_css("align-items","flex-start")
        | detail::raw_css("padding-top","12vh")
        | veil(.55f) | backdrop_blur(2)
        | tap(onClose);
}

} // namespace waya::ui
