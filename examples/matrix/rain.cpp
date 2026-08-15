/// matrix/rain.cpp — render the rain columns to SVG <text> glyphs. Each column
/// draws its trail from head upward, brightest at the head fading to dim green.
#include "rain.hpp"
#include "theme.hpp"

#include <string>

namespace mtx {

namespace {
constexpr int kCols = 48;
constexpr int kRows = 30;
constexpr int CW = 14;   // cell width  (svg units)
constexpr int CH = 20;   // cell height

// katakana-ish + digits glyph set (deterministic per column+row via seed)
char glyph_for(std::uint32_t seed, int row) {
    static const char set[] = "01<>[]{}#$%&*+=/\\|ABCDEFGHKLMNPRSTUVXYZ";
    std::uint32_t h = seed ^ (std::uint32_t(row) * 2654435761u);
    h ^= h >> 15; h *= 0x2c1b3c6dU; h ^= h >> 12;
    return set[h % (sizeof(set) - 1)];
}
} // namespace

waya::surface::NodeRef rain_canvas(const Model& m) {
    using namespace waya::surface;
    const int W = kCols * CW, H = kRows * CH;

    std::string svg;
    svg.reserve(64 * 1024);
    svg += "<svg viewBox='0 0 " + std::to_string(W) + " " + std::to_string(H) +
           "' preserveAspectRatio='xMidYMid slice' "
           "style='position:absolute;inset:0;width:100%;height:100%;display:block'>";
    svg += "<rect width='" + std::to_string(W) + "' height='" + std::to_string(H) + "' fill='#000'/>";
    svg += "<g font-family='monospace' font-size='16' text-anchor='middle'>";

    for (int ci = 0; ci < (int)m.cols.size() && ci < kCols; ++ci) {
        const auto& c = m.cols[ci];
        int hx_ = ci * CW + CW / 2;
        int head_row = (int)c.head;
        for (int k = 0; k < c.len; ++k) {
            int row = head_row - k;
            if (row < 0 || row >= kRows) continue;
            int y = row * CH + CH - 4;
            char g = glyph_for(c.seed + row, row);
            std::string ch(1, g);
            if (ch == "<") ch = "&lt;"; else if (ch == ">") ch = "&gt;";
            else if (ch == "&") ch = "&amp;";
            if (k == 0) {
                // bright head — the only near-white glyph, small glow
                svg += "<text x='" + std::to_string(hx_) + "' y='" + std::to_string(y) +
                       "' fill='" + hx(bright) + "' opacity='0.95'"
                       " style='filter:drop-shadow(0 0 5px " + hx(green) + ")'>" + ch + "</text>";
            } else {
                // trail: a sharp falloff so only the top few glyphs are visible
                // and the rest fades to near-black — reads as a distinct stream,
                // not a wall of noise. Squared falloff + a low ceiling.
                float t = 1.0f - float(k) / c.len;
                float op = 0.55f * t * t;           // squared => quick fade
                if (op < 0.02f) continue;           // drop the tail entirely when too faint
                char ob[8]; std::snprintf(ob, sizeof ob, "%.2f", op);
                svg += "<text x='" + std::to_string(hx_) + "' y='" + std::to_string(y) +
                       "' fill='" + hx(green) + "' opacity='" + ob + "'>" + ch + "</text>";
            }
        }
    }
    svg += "</g></svg>";

    // The rain sits BEHIND everything at reduced opacity so the terminal + HUD
    // read clearly on top — the rain is atmosphere, not the subject.
    return box(markup(std::move(svg)))
        | absolute() | pin()
        | z(0)
        | opacity(.55f)
        | clip;
}

} // namespace mtx
