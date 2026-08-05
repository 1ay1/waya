#pragma once
/// \file tag.hpp
/// The set of HTML elements waya knows, as an enum. One entry per element.
///
/// Phase 1 covers the ~55 elements that make up the overwhelming majority of
/// real documents. The remaining long-tail elements (and per-element attribute
/// tables) are generated from the WHATWG spec by tools/gen_elements.py — see
/// PLAN.md Phase 1. Adding an element is: one enum entry + one WAYA_ELEMENT row
/// in traits.hpp.

#include <cstdint>
#include <string_view>

namespace waya::html {

enum class Tag : std::uint16_t {
    // Document structure
    html, head, body, title, meta, link, style, base, script, noscript,

    // Sections
    header, footer, main, nav, section, article, aside, hgroup,
    h1, h2, h3, h4, h5, h6,

    // Grouping / flow
    div, p, hr, pre, blockquote, figure, figcaption,
    ul, ol, li, dl, dt, dd,

    // Text-level (phrasing)
    a, span, em, strong, small, s, cite, code, sub, sup, i, b, u, mark,
    br, wbr, time, abbr, kbd, samp, var_,

    // Embedded
    img, picture, source, video, audio, iframe, canvas, svg,

    // Tabular
    table, caption, colgroup, col, thead, tbody, tfoot, tr, td, th,

    // Forms
    form, label, input, button, select, option, optgroup, textarea,
    fieldset, legend, datalist, output, progress, meter,

    // Interactive
    details, summary, dialog,

    Count_  ///< sentinel — number of tags
};

} // namespace waya::html
