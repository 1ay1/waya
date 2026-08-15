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
                        | pre);

    // the in-progress line + blinking cursor
    auto cursor = box()
        | w(8) | h(15)
        | bg(green) | phosphor(green, 6)
        | detail::raw_css("animation", "mtx-blink 1s step-end infinite")   // custom @keyframes
        | detail::raw_css("display", "inline-block")
        | margin_left(2);
    auto typing_row = row(
        text(m.typing) | fg(green) | font(13) | term_font | phosphor(green, 5) | pre,
        cursor
    ) | items_center;
    lines.push_back(typing_row);

    // header bar: "traffic lights" + title
    auto dot = [](std::uint32_t c){ return box() | size(10) | round(999) | bg(c); };
    auto header = row(dot(red), dot(amber), dot(green), box() | grow(),
                      text("root@mainframe:~#") | fg(dim) | font(11) | term_font)
        | gap(7) | items_center | w_full | pad_x(4) | pad_y(2);

    return col(header,
               box() | h(1) | bg(line) | w_full,
               col_(lines) | gap(3) | w_full)
        | gap(10) | pad(16) | round(6) | w_full
        | gradient(rgba(0x020a06, .97f), rgba(0x000000, .97f), 180)
        | border(1, rgba(green, .23f))
        | pane(green);
}

} // namespace mtx
