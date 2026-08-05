/// examples/app.cpp — a waya app.
///
///   cmake --build build -j && ./build/app        # http://localhost:8080
///
/// This is how you build a waya app. You describe a surface with a tiny
/// vocabulary — box / text / image / path + chaining attrs + tap(msg) — and
/// waya renders it, streams only the delta on each tap, and keeps the browser
/// in sync. Not one line mentions HTML, CSS, a div, flex, onclick, or a canvas;
/// those are waya's business, not yours.

#include <waya/surface/live.hpp>

#include <cmath>
#include <vector>

using namespace waya::surface;

struct Dashboard {
    struct Model {
        int requests = 0;
        std::vector<float> history{20, 30, 25, 40, 35, 50, 45};
    };
    using Msg = int;
    enum { AddRequest, Reset };

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        if (msg == AddRequest) {
            m.requests++;
            m.history.push_back(20 + (float)(m.requests * 7 % 60));
            if (m.history.size() > 24) m.history.erase(m.history.begin());
        } else if (msg == Reset) {
            m = Model{};
        }
        return m;
    }

    static NodeRef view(const Model& m) {
        // build the chart path from the history
        std::vector<Pt> chart;
        for (std::size_t i = 0; i < m.history.size(); ++i)
            chart.push_back({(float)i * 16.f, 80.f - m.history[i]});

        auto card = [](NodeRef body) {
            return body | pad(20) | bg(0x1e293b) | round_(16);
        };

        return col({
            text("Live dashboard") | fg(0x818cf8) | size(32) | bold,
            text("Every tap streams a tiny delta — no HTML in the app code.")
                | fg(0x94a3b8) | size(15),

            card(col({
                text("Total requests") | fg(0x94a3b8) | size(13),
                text(m.requests) | fg(0xe2e8f0) | size(48) | bold,
            }) | gap(4)),

            card(col({
                text("Traffic") | fg(0x94a3b8) | size(13),
                path(chart) | fg(0x22d3ee) | size(24),      // one primitive = the chart
            }) | gap(8)),

            row({
                text("+ request") | fg(0xffffff) | pad(12) | bg(0x6366f1) | round_(10) | tap(AddRequest),
                text("reset")     | fg(0xe2e8f0) | pad(12) | bg(0x334155) | round_(10) | tap(Reset),
            }) | gap(12),
        }) | gap(20) | pad(40) | bg(0x0b1020);
    }
};

int main() {
    static_assert(SurfaceProgram<Dashboard>);
    return live<Dashboard>({.port = 8080});
}
