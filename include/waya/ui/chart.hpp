#pragma once
/// \file ui/chart.hpp
/// chart() — a real data chart: axes, gridlines, labels, legend, tooltips.
///
/// `sparkline`/`line_chart`/`bars` (charts.hpp) draw a bare shape \u2014 great for a
/// stat card, useless as an actual chart. A real chart needs a value axis with
/// gridlines and labels, a category axis, one or more named series, and a
/// legend. `chart()` builds all of that on the `scene` vector vocabulary, so it
/// renders to ONE diffable, XSS-safe <svg> \u2014 no markup strings.
///
///   chart(560, 300,
///       { series("Revenue", {10, 25, 18, 40, 32}, 0x22d3ee),
///         series("Cost",    { 8, 12, 14, 20, 22}, 0xf59e0b) },
///       { .x_labels = {"Jan","Feb","Mar","Apr","May"}, .kind = ChartKind::line })
///
/// The value axis auto-scales to the data (a "nice" rounded max), draws N
/// horizontal gridlines with labels, and the category axis labels the x points.
/// `kind` picks line / area / bar. A legend row names each series in its colour.
/// Everything is a pure function of the data \u2014 change a value and it redraws.

#include "../surface/node.hpp"
#include "scene.hpp"
#include "components.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// One named data series.
struct Series {
    std::string name;
    std::vector<float> values;
    std::uint32_t color = 0x22d3ee;
};
inline Series series(std::string name, std::vector<float> values, std::uint32_t color = 0x22d3ee){
    return { std::move(name), std::move(values), color };
}

enum class ChartKind { line, area, bar };

/// Chart options (aggregate-initialised: `{ .x_labels = …, .kind = … }`).
struct ChartOpts {
    std::vector<std::string> x_labels;      // category labels along the bottom
    ChartKind kind = ChartKind::line;
    int y_ticks = 4;                        // number of horizontal gridlines
    bool legend = true;
    float y_min = 0;                        // axis floor (default 0)
    bool auto_min = false;                  // if true, floor to the data's min instead
};

namespace chart_detail {
// A "nice" upper bound >= v: 1/2/5 x 10^k, so gridline labels read cleanly.
inline float nice_max(float v){
    if (v <= 0) return 1;
    float exp = std::floor(std::log10(v));
    float base = std::pow(10.f, exp);
    float frac = v / base;
    float nice = frac <= 1 ? 1 : frac <= 2 ? 2 : frac <= 5 ? 5 : 10;
    return nice * base;
}
inline std::string fmt(float v){
    // compact axis label: drop trailing .0
    if (v == (long long)v) return std::to_string((long long)v);
    char b[24]; std::snprintf(b, sizeof b, "%.1f", v); return b;
}
}

/// `chart(w, h, series, opts)` — a full chart. `w`/`h` set the coordinate space;
/// size the node with `| w_full | h(300)`. Colour-per-series is in each Series.
inline NodeRef chart(float w_, float h_, std::vector<Series> data, ChartOpts opts = {}){
    using namespace chart_detail;
    // ── plot area (leave margins for axis labels + legend) ─────────────────
    const float ml = 44, mr = 12, mt = 12;
    const float mb = (opts.legend ? 46.f : 28.f);
    const float px = ml, py = mt;
    const float pw = std::max(1.f, w_ - ml - mr);
    const float ph = std::max(1.f, h_ - mt - mb);

    // ── value range ────────────────────────────────────────────────────────
    float dmax = 0, dmin = 0; bool any = false;
    for (auto& s : data) for (float v : s.values){
        if (!any){ dmax = dmin = v; any = true; }
        else { dmax = std::max(dmax, v); dmin = std::min(dmin, v); }
    }
    float lo = opts.auto_min ? dmin : opts.y_min;
    float hi = nice_max(std::max(dmax, lo + 1));
    float span = std::max(1.f, hi - lo);

    // longest series length = number of category slots
    std::size_t n = 0; for (auto& s : data) n = std::max(n, s.values.size());
    if (n == 0) n = 1;

    auto typeX = [&](std::size_t i, std::size_t count) -> float {
        if (count <= 1) return px + pw / 2;
        return px + pw * (float)i / (float)(count - 1);
    };
    auto valueY = [&](float v) -> float { return py + ph * (1.f - (v - lo) / span); };

    std::vector<Shape> shapes;

    // ── gridlines + y labels ────────────────────────────────────────────────
    for (int t = 0; t <= opts.y_ticks; ++t){
        float frac = (float)t / opts.y_ticks;
        float val = lo + span * frac;
        float y = py + ph * (1.f - frac);
        shapes.push_back(vline(px, y, px + pw, y).stroke(rgba(0xffffff, .08f), 1));
        shapes.push_back(vtext(px - 8, y + 4, fmt(val)).fill(rgba(0xffffff, .45f)).font_px(11).anchor_end());
    }
    // baseline (value axis) + category axis
    shapes.push_back(vline(px, py, px, py + ph).stroke(rgba(0xffffff, .18f), 1));
    shapes.push_back(vline(px, py + ph, px + pw, py + ph).stroke(rgba(0xffffff, .18f), 1));

    // ── x labels ─────────────────────────────────────────────────────────────
    for (std::size_t i = 0; i < opts.x_labels.size() && i < n; ++i){
        float x = (opts.kind == ChartKind::bar)
            ? px + pw * ((float)i + 0.5f) / (float)n
            : typeX(i, n);
        shapes.push_back(vtext(x, py + ph + 16, opts.x_labels[i]).fill(rgba(0xffffff, .5f)).font_px(11).anchor_mid());
    }

    // ── series ───────────────────────────────────────────────────────────────
    for (std::size_t si = 0; si < data.size(); ++si){
        const auto& s = data[si];
        if (s.values.empty()) continue;
        if (opts.kind == ChartKind::bar){
            // grouped bars: each category slot split among series.
            float slot = pw / (float)n;
            float groupw = slot * 0.7f;
            float barw = groupw / (float)data.size();
            for (std::size_t i = 0; i < s.values.size(); ++i){
                float bh = ph * (s.values[i] - lo) / span;
                if (bh < 0) bh = 0;
                float x0 = px + slot * (float)i + (slot - groupw) / 2 + barw * (float)si;
                shapes.push_back(vrect(x0, py + ph - bh, barw * 0.86f, bh, 2).fill(s.color));
            }
        } else {
            std::vector<Pt> pts;
            for (std::size_t i = 0; i < s.values.size(); ++i)
                pts.push_back({ typeX(i, s.values.size()), valueY(s.values[i]) });
            if (opts.kind == ChartKind::area){
                std::vector<Pt> poly = pts;
                poly.push_back({ pts.back().x, py + ph });
                poly.push_back({ pts.front().x, py + ph });
                shapes.push_back(vpolygon(poly).fill(rgba(s.color, .18f)));
            }
            shapes.push_back(vpolyline(pts).stroke(s.color, 2).round_cap());
            // point dots
            for (auto& p : pts) shapes.push_back(vcircle(p.x, p.y, 2.5f).fill(s.color));
        }
    }

    // ── legend ─────────────────────────────────────────────────────────────
    if (opts.legend){
        float lx = px, ly = py + ph + 34;
        for (auto& s : data){
            shapes.push_back(vrect(lx, ly - 8, 10, 10, 2).fill(s.color));
            shapes.push_back(vtext(lx + 16, ly + 1, s.name).fill(rgba(0xffffff, .7f)).font_px(12));
            lx += 20 + (float)s.name.size() * 7.2f + 18;
        }
    }

    return scene(w_, h_, std::move(shapes));
}

} // namespace waya::ui
