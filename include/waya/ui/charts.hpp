#pragma once
/// \file ui/charts.hpp
/// Tiny data-visualisation helpers built on the `path` primitive. A chart is
/// just a node — waya's `path` takes points in any coordinate space and fits a
/// viewBox around them, so these map your numbers to points and hand back a
/// scalable SVG that tints with `stroke(...)` / `fg(...)` and sizes with `w`/`h`.
///
///   sparkline(cpu_history) | stroke(0x22d3ee, 2) | w(120) | h(32)
///   line_chart({12, 30, 18, 44, 27}) | stroke_c(0x6366f1)
///   bars({4, 9, 2, 7}) | fg(0x8b5cf6)

#include "../surface/node.hpp"
#include "components.hpp"

#include <algorithm>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

namespace charts_detail {
// map values to points in a 0..W × 0..H box, y flipped (0 at bottom).
inline std::vector<Pt> to_points(const std::vector<float>& ys, float W, float H) {
    std::vector<Pt> pts;
    if (ys.empty()) return pts;
    float lo = *std::min_element(ys.begin(), ys.end());
    float hi = *std::max_element(ys.begin(), ys.end());
    float span = (hi - lo) == 0 ? 1.f : (hi - lo);
    float n = ys.size() > 1 ? (float)(ys.size() - 1) : 1.f;
    for (std::size_t i = 0; i < ys.size(); ++i) {
        float x = W * (float)i / n;
        float y = H - H * (ys[i] - lo) / span;   // flip so bigger = higher
        pts.push_back({x, y});
    }
    return pts;
}
} // namespace charts_detail

/// `line_chart(values)` — an open polyline through the data. Colour with
/// `stroke(hex, width)`; size with `w`/`h`. Default sizing is fluid.
inline NodeRef line_chart(const std::vector<float>& values, float w_ = 240, float h_ = 80) {
    return path(charts_detail::to_points(values, w_, h_), /*closed=*/false)
         | w(w_) | h(h_);
}

/// `sparkline(values)` — a compact inline line chart (no axes), for a table cell
/// or a stat card. Same as line_chart, tuned small.
inline NodeRef sparkline(const std::vector<float>& values, float w_ = 120, float h_ = 32) {
    return line_chart(values, w_, h_) | css("display", "block");
}

/// `area_chart(values)` — a filled line chart. The path is closed down to the
/// baseline; fill it with `fg(hex)` (use a low-alpha via the fill_opacity the
/// path already applies) and outline with `stroke`.
inline NodeRef area_chart(const std::vector<float>& values, float w_ = 240, float h_ = 80) {
    auto pts = charts_detail::to_points(values, w_, h_);
    if (!pts.empty()) {
        // close the shape along the baseline so the fill reads as an area
        pts.push_back({w_, h_});
        pts.push_back({0, h_});
    }
    return path(std::move(pts), /*closed=*/true) | w(w_) | h(h_);
}

/// `bars(values)` — a simple bar chart drawn as SVG via markup (each bar a rect).
/// Colour with `fg(hex)`; size with `w`/`h`. Bars share a baseline and gap.
inline NodeRef bars(const std::vector<float>& values, float w_ = 240, float h_ = 80) {
    std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " +
        std::to_string((int)w_) + " " + std::to_string((int)h_) +
        "' width='100%' height='100%' preserveAspectRatio='none' style='display:block;overflow:visible'>";
    if (!values.empty()) {
        float hi = *std::max_element(values.begin(), values.end());
        if (hi <= 0) hi = 1;
        float n = (float)values.size();
        float slot = w_ / n;
        float bw = slot * 0.66f;
        for (std::size_t i = 0; i < values.size(); ++i) {
            float bh = h_ * (values[i] / hi);
            float x = slot * (float)i + (slot - bw) / 2.f;
            float y = h_ - bh;
            svg += "<rect x='" + std::to_string((int)x) + "' y='" + std::to_string((int)y) +
                   "' width='" + std::to_string((int)bw) + "' height='" + std::to_string((int)bh) +
                   "' rx='2' fill='currentColor'/>";
        }
    }
    svg += "</svg>";
    return markup(std::move(svg)) | w(w_) | h(h_) | css("line-height", "0");
}

} // namespace waya::ui
