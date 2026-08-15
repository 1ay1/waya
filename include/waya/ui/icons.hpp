#pragma once
/// \file ui/icons.hpp
/// A small, tree-shakeable inline-SVG icon set. Icons are just nodes: each is a
/// `markup(<svg>…</svg>)` sized and coloured by the surrounding mods, so an icon
/// composes exactly like any other node — `icon("check") | fg_primary | size(20)`.
///
/// Only the icons you actually name end up in your binary (each is an inline
/// function returning a string literal — the linker drops the rest). The set is
/// stroke-based (currentColor), so `fg(…)` tints them and `size(n)` scales them.
///
///   #include <waya/ui.hpp>
///   using namespace waya::ui;
///   row(icon("search"), text("Search")) | gap(8)
///   icon_button_i("trash", Delete{}, Variant::danger)   // icon in a button

#include "../surface/node.hpp"
#include "components.hpp"

#include <string>

namespace waya::ui {

using namespace waya::surface;

namespace icons_detail {
// Each icon is a 24×24 stroke path (Feather-style geometry), currentColor,
// round caps. Returned as the inner SVG body so we can wrap it once.
inline const char* body(std::string_view name) {
    if (name == "check")     return "<polyline points='20 6 9 17 4 12'/>";
    if (name == "x")         return "<line x1='18' y1='6' x2='6' y2='18'/><line x1='6' y1='6' x2='18' y2='18'/>";
    if (name == "plus")      return "<line x1='12' y1='5' x2='12' y2='19'/><line x1='5' y1='12' x2='19' y2='12'/>";
    if (name == "minus")     return "<line x1='5' y1='12' x2='19' y2='12'/>";
    if (name == "search")    return "<circle cx='11' cy='11' r='8'/><line x1='21' y1='21' x2='16.65' y2='16.65'/>";
    if (name == "menu")      return "<line x1='3' y1='12' x2='21' y2='12'/><line x1='3' y1='6' x2='21' y2='6'/><line x1='3' y1='18' x2='21' y2='18'/>";
    if (name == "chevron-down")  return "<polyline points='6 9 12 15 18 9'/>";
    if (name == "chevron-up")    return "<polyline points='18 15 12 9 6 15'/>";
    if (name == "chevron-right") return "<polyline points='9 18 15 12 9 6'/>";
    if (name == "chevron-left")  return "<polyline points='15 18 9 12 15 6'/>";
    if (name == "arrow-right") return "<line x1='5' y1='12' x2='19' y2='12'/><polyline points='12 5 19 12 12 19'/>";
    if (name == "arrow-left")  return "<line x1='19' y1='12' x2='5' y2='12'/><polyline points='12 19 5 12 12 5'/>";
    if (name == "user")      return "<path d='M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2'/><circle cx='12' cy='7' r='4'/>";
    if (name == "settings")  return "<circle cx='12' cy='12' r='3'/><path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z'/>";
    if (name == "trash")     return "<polyline points='3 6 5 6 21 6'/><path d='M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2'/>";
    if (name == "edit")      return "<path d='M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7'/><path d='M18.5 2.5a2.12 2.12 0 0 1 3 3L12 15l-4 1 1-4z'/>";
    if (name == "heart")     return "<path d='M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 1 0-7.78 7.78L12 21.23l8.84-8.84a5.5 5.5 0 0 0 0-7.78z'/>";
    if (name == "star")      return "<polygon points='12 2 15.09 8.26 22 9.27 17 14.14 18.18 21.02 12 17.77 5.82 21.02 7 14.14 2 9.27 8.91 8.26 12 2'/>";
    if (name == "bell")      return "<path d='M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9'/><path d='M13.73 21a2 2 0 0 1-3.46 0'/>";
    if (name == "home")      return "<path d='M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'/><polyline points='9 22 9 12 15 12 15 22'/>";
    if (name == "mail")      return "<path d='M4 4h16a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z'/><polyline points='22 6 12 13 2 6'/>";
    if (name == "external")  return "<path d='M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6'/><polyline points='15 3 21 3 21 9'/><line x1='10' y1='14' x2='21' y2='3'/>";
    if (name == "info")      return "<circle cx='12' cy='12' r='10'/><line x1='12' y1='16' x2='12' y2='12'/><line x1='12' y1='8' x2='12.01' y2='8'/>";
    if (name == "alert")     return "<path d='M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z'/><line x1='12' y1='9' x2='12' y2='13'/><line x1='12' y1='17' x2='12.01' y2='17'/>";
    if (name == "loader")    return "<line x1='12' y1='2' x2='12' y2='6'/><line x1='12' y1='18' x2='12' y2='22'/><line x1='4.93' y1='4.93' x2='7.76' y2='7.76'/><line x1='16.24' y1='16.24' x2='19.07' y2='19.07'/><line x1='2' y1='12' x2='6' y2='12'/><line x1='18' y1='12' x2='22' y2='12'/><line x1='4.93' y1='19.07' x2='7.76' y2='16.24'/><line x1='16.24' y1='7.76' x2='19.07' y2='4.93'/>";
    if (name == "folder")    return "<path d='M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z'/>";
    if (name == "folder-open") return "<path d='M6 14l1.5-2.5A2 2 0 0 1 9.24 10H22a2 2 0 0 1 1.94 2.5L22 20H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h5a2 2 0 0 1 2 2v2'/>";
    if (name == "file")      return "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/><polyline points='14 2 14 8 20 8'/>";
    if (name == "file-code") return "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/><polyline points='14 2 14 8 20 8'/><polyline points='10 13 8 15 10 17'/><polyline points='14 13 16 15 14 17'/>";
    if (name == "file-image") return "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/><polyline points='14 2 14 8 20 8'/><circle cx='9.5' cy='13.5' r='1.5'/><path d='M20 17l-3.5-3.5L11 19'/>";
    return "";  // unknown → empty (renders nothing)
}
} // namespace icons_detail

/// `icon("check")` — an inline SVG icon. Tint with `fg(…)`, size with `size(n)`
/// (default 24). Uses currentColor, so it inherits text colour by default.
inline NodeRef icon(std::string_view name, float d = 24) {
    std::string sz = std::to_string((int)d);
    std::string svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='" + sz + "' height='" + sz +
        "' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' "
        "stroke-linecap='round' stroke-linejoin='round' style='display:block'>" +
        std::string(icons_detail::body(name)) + "</svg>";
    // A markup node is a <div> wrapper; make it shrink to the icon and inherit
    // colour. currentColor picks up the surrounding `fg(...)`.
    return markup(std::move(svg)) | detail::raw_css("display", "inline-flex")
         | no_shrink | detail::raw_css("line-height", "0");
}

/// `icon_button_i("trash", Delete{})` — an icon inside a themed square button.
template <typename Msg>
NodeRef icon_button_i(std::string_view name, Msg msg, Variant v = Variant::ghost, float d = 18) {
    return button_node(icon(name, d), msg, v);
}

} // namespace waya::ui
