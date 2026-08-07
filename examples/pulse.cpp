/// examples/pulse.cpp — PULSE: a real-time analytics dashboard. Four KPI cards
/// with live sparklines, an area chart with a segmented range switch, a traffic
/// bar chart, and a rolling activity feed — all driven by one server-side clock
/// and streamed as deltas. A pause/resume control shows how a Sub is just a
/// function of the Model.
///
///   waya run pulse            # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <array>
#include <deque>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Pulse {
    struct Metric {
        std::string        label, unit;
        long               value;
        std::uint32_t      hue;
        std::vector<float> spark;
    };

    struct Model {
        long   tick = 0;
        bool   live = true;
        int    range = 1;                // 0=1H 1=24H 2=7D
        std::array<Metric, 4> kpis = {{
            { "Active users", "",   1284, 0x22d3ee, {12,18,15,22,19,26,24,31,28,34} },
            { "Requests/s",   "",    847, 0xa78bfa, {40,38,44,41,52,48,55,50,58,54} },
            { "Revenue",      "$",  9320, 0x34d399, {20,24,22,28,30,27,33,36,34,40} },
            { "Error rate",   "%",     7, 0xf472b6, { 9, 8,11, 7, 6, 8, 5, 6, 4, 5} },
        }};
        std::vector<float> traffic = { 32, 48, 40, 66, 58, 72, 61, 80, 74, 90, 68, 84 };
        std::vector<float> chart   = { 30, 42, 38, 55, 49, 63, 58, 71, 66, 78, 74, 86, 82, 94 };
        std::deque<std::string> feed = {
            "deploy \u2192 prod succeeded",
            "user alex.k upgraded to Pro",
            "cache warmed \u00b7 42ms p95",
        };
    };

    struct Tick {}; struct ToggleLive {}; struct Range { int i; };
    using Msg = std::variant<Tick, ToggleLive, Range>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model, Cmd<Msg>> {
                m.tick++;
                auto wander = [&](long v, long lo, long hi, int seed) {
                    long d = (long)((m.tick * 7 + seed * 13) % 11) - 5;
                    v += d; if (v < lo) v = lo; if (v > hi) v = hi; return v;
                };
                m.kpis[0].value = wander(m.kpis[0].value, 900, 2000, 1);
                m.kpis[1].value = wander(m.kpis[1].value, 500, 1200, 2);
                m.kpis[2].value = wander(m.kpis[2].value, 6000, 14000, 3);
                m.kpis[3].value = wander(m.kpis[3].value, 1, 20, 4);
                for (auto& k : m.kpis) {
                    k.spark.push_back((float)k.value);
                    if (k.spark.size() > 10) k.spark.erase(k.spark.begin());
                }
                m.chart.push_back((float)m.kpis[0].value / 15.f);
                if (m.chart.size() > 14) m.chart.erase(m.chart.begin());
                for (auto& t : m.traffic)
                    t = std::max(10.f, std::min(100.f, t + ((m.tick + (long)t) % 7) - 3));

                if (m.tick % 5 == 0) {
                    static const char* evt[] = {
                        "new signup \u00b7 berlin", "job queue drained",
                        "invoice #4821 paid", "webhook delivered \u00b7 200",
                        "user session \u2192 mobile",
                    };
                    m.feed.push_front(evt[(m.tick / 5) % 5]);
                    if (m.feed.size() > 6) m.feed.pop_back();
                }
                return { m, Cmd<Msg>::none() };
            },
            [&](ToggleLive) -> std::pair<Model, Cmd<Msg>> {
                m.live = !m.live; return { m, Cmd<Msg>::none() };
            },
            [&](Range r) -> std::pair<Model, Cmd<Msg>> {
                m.range = r.i; return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.live ? Sub<Msg>::every(std::chrono::milliseconds(900), Tick{})
                      : Sub<Msg>::none();
    }

    // ── little building blocks ──────────────────────────────────────────────
    static NodeRef panel_(NodeRef inner) {
        return inner | pad(22) | round(20)
             | detail::raw_css("background",
                 "linear-gradient(180deg, rgba(255,255,255,.05), rgba(255,255,255,.02))")
             | hairline(0xffffff, 0.09f) | elevation(3);
    }

    static NodeRef kpi_card(const Metric& k) {
        std::string big = (k.unit == "$" ? "$" : "") +
                          std::to_string(k.value) + (k.unit == "%" ? "%" : "");
        return panel_(col(
            row(text(k.label) | fg(muted) | font(13) | semibold | tracking_em(0.02f),
                box() | grow(),
                box() | circle(8) | bg(k.hue) | glow(k.hue, 12) | breathe()) | items_center,
            text(big) | fg(ink) | font(34) | weight(Weight::black) | tabular_nums,
            sparkline(k.spark) | stroke(k.hue, 2) | w_full | h(34)
        ) | gap(12)) | grow();
    }

    static NodeRef seg(const Model& m, int i, std::string label) {
        bool on = m.range == i;
        auto n = text(label) | font(13) | semibold
               | pad_x(14) | pad_y(7) | round(9)
               | fg(on ? ink : muted)
               | interactive() | tap(Range{ i });
        if (on) n = n | tint(0xffffff, 0.12f) | ring(0xffffff, 1);
        return n;
    }

    static NodeRef view(const Model& m) {
        // top bar
        auto header = row(
            row(box(text("\u25C8") | font(20) | fg(0xffffff))
                    | square(40) | center | round(11)
                    | gradient(0x8b5cf6, 0x6366f1, 135) | glow(0x8b5cf6, 18),
                col(text("Pulse") | fg(ink) | font(20) | weight(Weight::black) | leading(1.f),
                    text("analytics") | fg(faint) | font(12) | tracking_em(0.14f))
                    | gap(1)) | gap(12) | items_center,
            box() | grow(),
            row(box() | circle(8) | bg(m.live ? 0x34d399 : 0x64748b)
                    | (m.live ? breathe() : noop),
                text(m.live ? "Live" : "Paused") | fg(m.live ? 0x34d399 : muted)
                    | font(13) | semibold,
                box(text(m.live ? "Pause" : "Resume") | font(13) | semibold | fg(ink))
                    | pad_x(14) | pad_y(8) | round(10)
                    | frost(12) | hairline(0xffffff, 0.14f)
                    | interactive() | tap(ToggleLive{})
            ) | gap(12) | items_center
        ) | items_center | w_full;

        // KPI row
        auto kpis = row(
            kpi_card(m.kpis[0]), kpi_card(m.kpis[1]),
            kpi_card(m.kpis[2]), kpi_card(m.kpis[3])
        ) | gap(16) | wrap;

        // main chart panel
        auto chart_panel = panel_(col(
            row(col(text("Throughput") | fg(ink) | font(17) | semibold,
                    text("requests over time") | fg(faint) | font(12)) | gap(2),
                box() | grow(),
                row(seg(m, 0, "1H"), seg(m, 1, "24H"), seg(m, 2, "7D"))
                    | gap(4) | pad(4) | round(11) | tint(0xffffff, 0.05f)) | items_center,
            area_chart(m.chart) | fg(0x6366f1) | stroke(0x818cf8, 2) | w_full | h(200)
        ) | gap(16)) | grow();

        // traffic bars
        auto traffic_panel = panel_(col(
            text("Traffic by hour") | fg(ink) | font(17) | semibold,
            bars(m.traffic) | fg(0x22d3ee) | w_full | h(200)
        ) | gap(16)) | w(300);

        auto charts_row = row(chart_panel, traffic_panel) | gap(16) | wrap;

        // activity feed
        std::vector<NodeRef> feed_items;
        {
            int i = 0;
            for (auto& line : m.feed) {
                if (i > 0)
                    feed_items.push_back(box() | h(1) | w_full | tint(0xffffff, 0.06f));
                feed_items.push_back(
                    row(box() | circle(6) | bg(i == 0 ? 0x34d399 : 0x475569),
                        text(line) | fg(i == 0 ? ink : muted) | font(14),
                        box() | grow(),
                        text(i == 0 ? "now" : std::to_string(i * 5) + "s")
                            | fg(faint) | font(12) | tabular_nums)
                    | gap(12) | items_center | pad_y(10)
                    | key(std::to_string(m.tick - i)) | fade_up(300));
                i++;
            }
        }
        auto feed_panel = panel_(col(
            row(text("Activity") | fg(ink) | font(17) | semibold,
                box() | grow(),
                badge("live", Tone::success)) | items_center,
            col_(std::move(feed_items))
        ) | gap(8)) | w_full;

        return col(header, kpis, charts_row, feed_panel)
             | gap(20) | pad(28) | max_w(1180) | center_x
             | min_h(100_vh)
             | radial(0x141b2e, 20, -20, 0x0b1020)
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Pulse";
        mt.description = "A real-time analytics dashboard, server-rendered in C++ with waya.";
        return mt;
    }
};

int main() {
    return live<Pulse>({ .port = 8080, .title = "waya \u00b7 pulse" });
}
