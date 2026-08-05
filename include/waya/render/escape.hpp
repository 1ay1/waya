#pragma once
/// \file escape.hpp
/// Context-correct HTML escaping. Phase 1 covers HTML-text and attribute-value
/// contexts; the full escaping-context type system (DESIGN §5) lands with the
/// URL/JS/CSS contexts in a later Phase 1 step.

#include <string>
#include <string_view>

namespace waya::render {

/// Escape text appearing in element content.
inline void escape_text(std::string& out, std::string_view s) {
    for (char c : s) switch (c) {
        case '&': out += "&amp;";  break;
        case '<': out += "&lt;";   break;
        case '>': out += "&gt;";   break;
        default:  out += c;
    }
}

/// Escape a value appearing inside a double-quoted attribute.
inline void escape_attr(std::string& out, std::string_view s) {
    for (char c : s) switch (c) {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&#39;";  break;
        default:   out += c;
    }
}

} // namespace waya::render
