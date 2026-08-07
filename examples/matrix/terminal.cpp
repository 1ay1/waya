/// matrix/terminal.cpp — the terminal pane. Revealed log lines + the line being
/// typed with a blinking block cursor. Alert-tinted line for the [!] warnings.
#include "terminal.hpp"
#include "theme.hpp"

#include <waya/surface/sugar.hpp>

namespace mtx {

using namespace waya::surface;

namespace {
std::uint32_t line_color(const std::string& s) {
    if (s.find("[!]") != std::string::npos) return amber;
    if (s.find("GRANTED") != std::string::npos || s.find("root@") != std::string::npos) return bright;
    if (s.find("the matrix has you") != std::string::npos) return bright;
    return green;
}
} // namespace

NodeRef terminal_pane(const Model& m) {
    std::vector<NodeRef> lines;
    for (const auto& l : m.log)
        lines.push_back(text(l) | fg(line_color(l)) | font(13) | term_font | phosphor(line_color(l), 5)
                        | detail::raw_css("white-space", "pre"));

    // the in-progress line + blinking cursor
    auto cursor = box()
        | detail::raw_css("width", "8px") | detail::raw_css("height", "15px")
        | detail::raw_css("background", hx(green)) | phosphor(green, 6)
        | detail::raw_css("animation", "mtx-blink 1s step-end infinite")
        | detail::raw_css("display", "inline-block")
        | detail::raw_css("margin-left", "2px");
    auto typing_row = row(
        text(m.typing) | fg(green) | font(13) | term_font | phosphor(green, 5) | detail::raw_css("white-space","pre"),
        cursor
    ) | items_center;
    lines.push_back(typing_row);

    // header bar: "traffic lights" + title
    auto dot = [](std::uint32_t c){ return box() | detail::raw_css("width","10px")
        | detail::raw_css("height","10px") | round(999) | detail::raw_css("background", hx(c)); };
    auto header = row(dot(red), dot(amber), dot(green), box() | grow(),
                      text("root@mainframe:~#") | fg(dim) | font(11) | term_font)
        | gap(7) | items_center | w_full | pad_x(4) | pad_y(2);

    return col(header,
               box() | detail::raw_css("height","1px") | detail::raw_css("background", hx(line)) | w_full,
               col_(lines) | gap(3) | w_full)
        | gap(10) | pad(16) | round(6) | w_full
        | detail::raw_css("background", "linear-gradient(180deg, rgba(2,10,6,.97), rgba(0,0,0,.97))")
        | detail::raw_css("border", "1px solid " + hxa(green, "3a"))
        | pane(green);
}

} // namespace mtx
