/// matrix/rain.cpp — render the rain columns to SVG <text> glyphs. Each column
/// draws its trail from head upward, brightest at the head fading to dim green.
#include "rain.hpp"
#include "theme.hpp"

#include <waya/ui/scene.hpp>
#include <string>
#include <vector>

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
    using namespace waya::ui;
    const int W = kCols * CW, H = kRows * CH;

    // Every glyph is a typed vtext Shape — no string concatenation, no manual
    // entity escaping (the scene escapes '<'/'&' for us), no hand-formatted
    // opacity. The rain is now DATA the backend draws, like everything else.
    std::vector<Shape> glyphs;
    glyphs.push_back(vrect(0, 0, W, H).fill(black));
    for (int ci = 0; ci < (int)m.cols.size() && ci < kCols; ++ci) {
        const auto& c = m.cols[ci];
        float cx = ci * CW + CW / 2.f;
        int head_row = (int)c.head;
        for (int k = 0; k < c.len; ++k) {
            int row = head_row - k;
            if (row < 0 || row >= kRows) continue;
            float y = row * CH + CH - 4.f;
            std::string ch(1, glyph_for(c.seed + row, row));
            if (k == 0) {
                // bright head — the only near-white glyph
                glyphs.push_back(vtext(cx, y, ch).fill(bright).opacity(0.95f)
                                 .font_px(16).anchor_mid().mono());
            } else {
                // trail: squared falloff so only the top few glyphs read as a
                // distinct stream; drop the tail once it's too faint.
                float t = 1.0f - float(k) / c.len;
                float op = 0.55f * t * t;
                if (op < 0.02f) continue;
                glyphs.push_back(vtext(cx, y, ch).fill(green).opacity(op)
                                 .font_px(16).anchor_mid().mono());
            }
        }
    }

    // The rain sits BEHIND everything at reduced opacity so the terminal + HUD
    // read clearly on top — the rain is atmosphere, not the subject.
    return box(scene((float)W, (float)H, std::move(glyphs)) | w_full | h_full)
        | absolute() | pin()
        | z(0)
        | opacity(.55f)
        | clip;
}

} // namespace mtx
