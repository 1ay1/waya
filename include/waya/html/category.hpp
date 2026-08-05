#pragma once
/// \file category.hpp
/// HTML5 content categories as a bitmask.
///
/// https://html.spec.whatwg.org/multipage/dom.html#kinds-of-content
///
/// The content model is the heart of waya's "invalid HTML doesn't compile"
/// guarantee. Each element declares which categories it *belongs to* and which
/// it *permits as children*; a single bitmask-and gates every nesting.

#include <cstdint>
#include <string_view>

namespace waya::html {

enum class Cat : std::uint32_t {
    None        = 0,

    // Standard WHATWG content categories.
    Metadata    = 1u << 0,   ///< <title>, <meta>, <link>, <style>, <base>
    Flow        = 1u << 1,   ///< most things that go in <body>
    Sectioning  = 1u << 2,   ///< <article>, <section>, <nav>, <aside>
    Heading     = 1u << 3,   ///< <h1>–<h6>, <hgroup>
    Phrasing    = 1u << 4,   ///< text-level: <span>, <a>, <em>, <img>, text
    Embedded    = 1u << 5,   ///< <img>, <video>, <canvas>, <iframe>
    Interactive = 1u << 6,   ///< <a>, <button>, <input>, <select>, <label>
    Palpable    = 1u << 7,   ///< has content (not empty) — for lint, not gating
    ScriptSupp  = 1u << 8,   ///< <script>, <template>

    // Structural pseudo-categories for elements with strict parent/child rules
    // that the standard categories do not capture on their own.
    HtmlSect    = 1u << 16,  ///< <head>, <body> — only inside <html>
    ListItem    = 1u << 17,  ///< <li>
    DListItem   = 1u << 18,  ///< <dt>, <dd>
    TableSect   = 1u << 19,  ///< <thead>, <tbody>, <tfoot>, <caption>, <colgroup>
    TableRow    = 1u << 20,  ///< <tr>
    TableCell   = 1u << 21,  ///< <td>, <th>
    OptionLike  = 1u << 22,  ///< <option>, <optgroup>
    SummaryLike = 1u << 23,  ///< <summary> (first child of <details>)

    Any         = 0xFFFFFFFFu,
};

constexpr Cat operator|(Cat a, Cat b) {
    return static_cast<Cat>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
constexpr Cat operator&(Cat a, Cat b) {
    return static_cast<Cat>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

/// True when any bit is set.
constexpr bool any(Cat c) { return static_cast<std::uint32_t>(c) != 0; }

/// A short human description of a permitted-content set, for diagnostics.
constexpr std::string_view describe(Cat c) {
    if (any(c & Cat::Phrasing))    return "phrasing content (text, <span>, <a>, <em>, <img>, …)";
    if (any(c & Cat::Flow))        return "flow content (<div>, <p>, <ul>, <table>, text, …)";
    if (any(c & Cat::Metadata))    return "metadata content (<title>, <meta>, <link>, <style>)";
    if (any(c & Cat::TableCell))   return "<td> and <th>";
    if (any(c & Cat::TableRow))    return "<tr>";
    if (any(c & Cat::TableSect))   return "<thead>, <tbody>, <tfoot>, <caption>, <colgroup>, <tr>";
    if (any(c & Cat::ListItem))    return "<li>";
    if (any(c & Cat::DListItem))   return "<dt> and <dd>";
    if (any(c & Cat::OptionLike))  return "<option> and <optgroup>";
    if (any(c & Cat::HtmlSect))    return "<head> and <body>";
    return "no children (this is a void or empty element)";
}

} // namespace waya::html
