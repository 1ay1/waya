#pragma once
/// \file ui/command_palette.hpp
/// command_palette — the Cmd+K fuzzy launcher, built on Keymap.
///
/// A `Keymap` already holds every action as (label, Msg) data. A command
/// palette is the natural second view of that same data: type to fuzzy-filter
/// the actions by label, arrow to select, Enter to run. Because it reads the
/// SAME keymap that arms the shortcuts, the palette and the keyboard can never
/// list different commands.
///
/// The palette is pure: you own its little bit of state (query + selected
/// index) in your model, and `command_palette(...)` renders it from that plus
/// the keymap. It emits YOUR messages — one to update the query, one per run.
///
///   struct Model { bool palette=false; std::string q; int sel=0; };
///
///   // update:
///   [&](OpenPalette)   { m.palette=true; m.q=""; m.sel=0; return {m, Cmd::focus("cmdk")}; }
///   [&](PaletteQuery e){ m.q=e.value; m.sel=0;             return {m, Cmd::none()}; }
///   [&](PaletteMove d) { m.sel += d.delta;                 return {m, Cmd::none()}; }
///   [&](RunCommand c)  { m.palette=false; return {m, /* dispatch c.msg */}; }
///
///   // view:
///   m.palette
///     ? command_palette(keys(), m.q, m.sel,
///           PaletteQuery{}, [](Msg cmd){ return RunCommand{cmd}; }, ClosePalette{})
///     : nothing()
///
/// The fuzzy match is a subsequence test (chars of the query appear in order in
/// the label) with a simple score, so "usr" finds "Go to user settings" — the
/// familiar launcher feel, no dependency.

#include "../surface/node.hpp"
#include "keymap.hpp"
#include "components.hpp"
#include "patterns.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

namespace palette_detail {
inline std::string lower(std::string s){ for(char& c : s) if(c>='A'&&c<='Z') c += 32; return s; }

/// Subsequence fuzzy match: every char of `q` appears in `text` in order.
/// Returns a score (higher = better) or -1 for no match. Empty query matches
/// everything with score 0. Scoring rewards, in order of weight: matches at a
/// WORD BOUNDARY (start, or after a space/`-`/`_`/`/`), CONTIGUOUS runs, and
/// EARLY position; it penalises gaps between matched chars. So "usr" ranks
/// "User settings" above "Go to user list", and "gts" finds "Go To Settings".
inline int fuzzy_score(const std::string& text_lower, const std::string& q_lower){
    if (q_lower.empty()) return 0;
    int score = 0, run = 0;
    std::size_t ti = 0;
    bool first = true;
    for (char qc : q_lower){
        bool found = false;
        std::size_t gap = 0;
        for (; ti < text_lower.size(); ++ti){
            if (text_lower[ti] == qc){
                bool boundary = (ti == 0) ||
                    text_lower[ti-1]==' ' || text_lower[ti-1]=='-' ||
                    text_lower[ti-1]=='_' || text_lower[ti-1]=='/';
                run = (gap == 0 && !first) ? run + 1 : 0;   // reset run on a gap
                score += 1 + run * 3;                        // contiguous runs win
                if (boundary)   score += 10;                 // word-start is a strong signal
                if (ti < 4)     score += 2;                  // early in the string
                score -= (int)std::min<std::size_t>(gap, 4); // penalise scanned-over chars
                ++ti; found = true; break;
            }
            ++gap;
        }
        if (!found) return -1;                              // a query char never appeared
        first = false;
    }
    return score;
}
} // namespace palette_detail

/// A scored, ranked view of a keymap for a query. Exposed so an app can drive
/// selection bounds (clamp `sel` to `[0, ranked.size())`).
template <typename Msg>
inline std::vector<const Binding<Msg>*> palette_matches(const Keymap<Msg>& km, const std::string& query){
    std::string q = palette_detail::lower(query);
    std::vector<std::pair<int, const Binding<Msg>*>> scored;
    for (auto& b : km.bindings){
        int s = palette_detail::fuzzy_score(palette_detail::lower(b.label), q);
        if (s >= 0) scored.push_back({ s, &b });
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](auto& a, auto& b){ return a.first > b.first; });
    std::vector<const Binding<Msg>*> out;
    out.reserve(scored.size());
    for (auto& [_, b] : scored) out.push_back(b);
    return out;
}

/// `command_palette(keymap, query, selected, onQuery, onRun, onClose)` — the
/// full launcher. `query`/`selected` are your model state; `onQuery` maps the
/// search box's text to a Msg; `onRun(cmd)` maps a chosen command's Msg to YOUR
/// Msg (usually a wrapper that closes the palette and re-dispatches); `onClose`
/// fires on Escape / backdrop click. Selection is clamped and highlighted.
template <typename Msg, typename OnQuery, typename OnRun, typename OnClose>
inline NodeRef command_palette(const Keymap<Msg>& km, const std::string& query, int selected,
                               OnQuery onQuery, OnRun onRun, OnClose onClose){
    auto ranked = palette_matches(km, query);
    int n = (int)ranked.size();
    if (n > 0){ selected = ((selected % n) + n) % n; }   // wrap + clamp

    std::vector<NodeRef> rows;
    rows.reserve(ranked.size());
    for (int i = 0; i < n; ++i){
        const auto& b = *ranked[i];
        bool on = (i == selected);
        auto caps = keymap_detail::combo_caps(b.combo);
        rows.push_back(
            row(text(b.label) | (on ? fg_text : fg_muted) | detail::raw_css("font-size","14px"),
                box() | grows,
                caps)
            | items_center | gap(12) | pad_x(12) | pad_y(9) | round(8) | w_full
            | (on ? bg(0x2a3350) : Mod{})
            | pointer | role("option") | aria_selected(on)
            | key("cmd-" + std::to_string(i))
            | tap(onRun(b.msg)));
    }

    auto search = input(query)
        | placeholder("Type a command\xe2\x80\xa6")           // …
        | anchor("cmdk") | autofocus()
        | w_full | pad_x(14) | pad_y(12) | detail::raw_css("font-size","15px")
        | detail::raw_css("background","transparent")
        | detail::raw_css("border","none") | detail::raw_css("outline","none")
        | fg_text | on_input(onQuery)
        | on_escape(onClose)
        | aria_label("Command search");

    auto panel = col(
        search,
        box() | h(1) | w_full | detail::raw_css("background","var(--wa-line, rgba(255,255,255,.08))"),
        (n > 0
            ? (col_(std::move(rows)) | w_full | gap(2) | pad(8)
               | detail::raw_css("max-height","320px") | detail::raw_css("overflow-y","auto"))
            : (text("No matching commands") | fg_muted | pad(20) | text_align(Justify::center) | w_full))
    ) | w_full | max_w(560) | round(14)
      | detail::raw_css("background","var(--wa-surface, #141b2e)")
      | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.10))")
      | detail::raw_css("box-shadow","0 24px 70px -20px rgba(0,0,0,.7)")
      | role("listbox") | aria_label("Commands")
      | stop();                                            // clicks inside don't close

    // a dimmed, click-to-close backdrop with the panel near the top.
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
