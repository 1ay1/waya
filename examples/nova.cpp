/// examples/nova.cpp — NOVA: a jaw-dropping live analytics landing page. An
/// animated aurora hero over a drifting mesh backdrop, glass metric cards with
/// gradient rings that pulse as their live numbers tick, an interactive area
/// chart with a segmented time-range switch, and staggered fade-up reveals —
/// every effect a one-line mod, the whole thing server-rendered and streaming
/// only deltas. This is what "beautiful by default" looks like when you push
/// waya's design vocabulary all the way.
///
///   cmake --build build --target nova && ./build/nova   # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/component.hpp>
#include <waya/ui.hpp>

#include <array>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::ui;

struct Nova {
    // A metric: a headline number that wanders, and its recent history for the
    // sparkline. `hue`/`hue2` drive the card's gradient ring + glow.
    struct Metric {
        std::string label, unit;
        int value;
        std::vector<float> history;
        std::uint32_t hue, hue2;
    };

    struct Model {
        long tick = 0;
        int range = 1;              // 0=24h 1=7d 2=30d (segmented control)
        bool live = true;
        int visitors = 12480;
        std::vector<Metric> metrics{
            { "Revenue",    "$",  84210, {40,52,48,63,58,71,69,82},  0x8b5cf6, 0x22d3ee },
            { "Active now", "",     1284, {30,42,38,50,47,55,61,58}, 0x22d3ee, 0x34d399 },
            { "Conversion", "%",      34, {20,24,22,28,31,29,35,34}, 0xf472b6, 0x8b5cf6 },
            { "Latency",    "ms",     42, {60,52,55,44,40,43,39,42}, 0x34d399, 0xfbbf24 },
        };
        std::vector<float> series{ 22,30,26,38,34,46,42,55,50,62,58,70,66,78,74,86 };
        long series_ver = 1;        // bumped only when the chart data changes
    };

    struct Tick {}; struct Range { int i; }; struct ToggleLive {};
    using Msg = std::variant<Tick, Range, ToggleLive>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model,Cmd<Msg>> {
                m.tick++;
                auto wander = [&](int v, int lo, int hi, int seed){
                    int d = (int)((m.tick*7 + seed*13) % 11) - 5; v += d;
                    return v < lo ? lo : v > hi ? hi : v;
                };
                m.visitors = wander(m.visitors, 9000, 16000, 1);
                int s = 0;
                for (auto& mt : m.metrics) {
                    mt.value = wander(mt.value, 10, mt.label=="Revenue"?99000:2000, s++);
                    mt.history.push_back((float)(mt.value % 100));
                    if (mt.history.size() > 8) mt.history.erase(mt.history.begin());
                }
                // roll the hero chart series
                float nv = m.series.back() + (float)(((int)(m.tick*17) % 24) - 11);
                if (nv < 8) nv = 8; if (nv > 96) nv = 96;
                m.series.push_back(nv);
                if (m.series.size() > 16) m.series.erase(m.series.begin());
                m.series_ver++;
                return { m, Cmd<Msg>::none() };
            },
            [&](Range r) -> std::pair<Model,Cmd<Msg>> { m.range = r.i; return { m, Cmd<Msg>::none() }; },
            [&](ToggleLive) -> std::pair<Model,Cmd<Msg>> { m.live = !m.live; return { m, Cmd<Msg>::none() }; },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.live ? Sub<Msg>::every(1100, Tick{}) : Sub<Msg>::none();
    }

    // ── pieces ───────────────────────────────────────────────────────────
    static std::string commas(long n) {
        std::string s = std::to_string(n < 0 ? -n : n), out;
        int c = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            if (c && c % 3 == 0) out.push_back(',');
            out.push_back(*it); ++c;
        }
        if (n < 0) out.push_back('-');
        return std::string(out.rbegin(), out.rend());
    }

    // a live glass metric card: gradient ring, glow, pulsing sparkline
    static NodeRef metric_card(const Metric& mt, int idx) {
        std::string val = (mt.unit == "$" ? "$" : "") + commas(mt.value)
                        + (mt.unit == "%" || mt.unit == "ms" ? " " + mt.unit : "");
        return col(
            row(
                text(mt.label) | fg(muted) | caption | tracking(1.0f) | semibold,
                push(),
                box() | size(7) | round(999) | bg(mt.hue) | breathe()
            ) | center,
            text(val) | fg(ink) | font_fluid(26, 34) | weight(Weight::black) | tracking_em(-0.02f),
            line_chart(mt.history, 200, 40) | stroke(mt.hue, 2.5f) | round_cap
                | drop_shadow(mt.hue, 14, 0.5f)
        ) | gap(10) | pad(22) | round(20) | grow(1) | min_w(rem(13))
          | frost(12) | gradient_border(mt.hue, mt.hue2, 1)
          | hover_lift(4) | hover_glow(mt.hue, 34)
          | fade_up(600) | delay(120 * idx);
    }

    static NodeRef seg(int cur, int i, std::string label, Msg msg) {
        bool on = cur == i;
        // keyboard-accessible: a real tab role + focus + Enter/Space activation,
        // so the segmented control works for keyboard and screen-reader users.
        auto b = text(std::move(label)) | font(13) | semibold
               | pad_x(16) | pad_y(8) | round(10) | pointer
               | role("tab") | aria("selected", on ? "true" : "false")
               | focusable() | tap(msg) | on_enter(msg) | press();
        return on ? (b | fg(white) | gradient_bg(0x8b5cf6, 0x22d3ee) | glow(0x8b5cf6, 18))
                  : (b | fg(muted) | tint(0xffffff, 0.04f) | hover_lift(1));
    }

    static NodeRef view(const Model& m) {
        // ── HERO ──────────────────────────────────────────────────────
        auto pill = row(
            box() | size(7) | round(999) | bg(m.live ? 0x34d399 : 0x64748b) | breathe(),
            text(m.live ? "live \u00b7 streaming deltas" : "paused")
                | fg(0x9fb3c8) | caption | semibold | tracking(0.5f)
        ) | gap(9) | center | pad_x(15) | pad_y(8) | round(999)
          | frost(10) | pointer
          | role("switch") | aria("checked", m.live ? "true" : "false")
          | aria("label", "toggle live streaming")
          | focusable() | tap(ToggleLive{}) | on_enter(ToggleLive{})
          | interactive() | fade_in(400);

        auto hero = col(
            pill,
            text("Analytics that feel alive") | display | weight(Weight::black)
                | font_fluid(40, 76) | tracking_em(-0.03f)
                | aurora_text(0x818cf8, 0x22d3ee, 0xf472b6, 7) | fade_up(600),
            text("Every number here is server-computed in C++ and streamed to the "
                 "browser as a tiny delta \u2014 no client framework, no rebuild, "
                 "just the changed pixels.")
                | fg(0x94a3b8) | font_fluid(15, 20) | max_w(rem(38))
                | leading(1.6f) | text_center | fade_up(700) | delay(80)
        ) | gap(20) | center | text_center;

        // ── LIVE METRIC CARDS ─────────────────────────────────────────
        std::vector<NodeRef> cards;
        for (std::size_t i = 0; i < m.metrics.size(); ++i)
            cards.push_back(metric_card(m.metrics[i], (int)i));
        auto grid = box_(std::move(cards));
        grid->style.flow = Flow::row; grid->style.wrap = Wrap::wrap;
        grid->style.gap = px(16); finalize(*grid);

        // ── HERO CHART (memoised by series version) ───────────────────
        auto chart = memo(m.series_ver, m.range, [&]{
            auto title = row(
                col(
                    text("Traffic") | fg(ink) | subtitle | weight(Weight::bold),
                    text(commas(m.visitors) + " visitors \u00b7 last "
                         + (m.range==0?"24 hours":m.range==1?"7 days":"30 days"))
                        | fg(muted) | caption
                ) | gap(3),
                push(),
                row(
                    seg(m.range, 0, "24h", Range{0}),
                    seg(m.range, 1, "7d",  Range{1}),
                    seg(m.range, 2, "30d", Range{2})
                ) | gap(6)
            ) | center | wrap | gap(12);

            auto plot = box_({
                area_chart(m.series, 900, 220) | fg(0x8b5cf6)
                    | stroke(0x22d3ee, 2.5f) | round_cap,
                line_chart(m.series, 900, 220) | stroke(0xa78bfa, 3.0f) | round_cap
                    | drop_shadow(0x8b5cf6, 22, 0.55f)
            });
            plot->style.flow = Flow::stack; finalize(*plot);
            plot = plot | w_full | h(px(220));

            return col(title, plot | pad_y(8))
                | gap(14) | pad(24) | round(24)
                | frost(14) | gradient_border(0x8b5cf6, 0x22d3ee, 1)
                | fade_up(800) | delay(160);
        });

        auto footer = row(
            text("\u26a1 real-time") | fg(0x818cf8) | caption | semibold,
            text("\u00b7") | fg(faint),
            text("\U0001f9ec typed C++") | fg(0x22d3ee) | caption | semibold,
            text("\u00b7") | fg(faint),
            text("\U0001f3a8 zero client JS") | fg(0xf472b6) | caption | semibold
        ) | gap(12) | center | wrap | fade_in(1200);

        return page(0x060810,
            centered(66, col(
                hero,
                grid | w_full,
                chart | w_full,
                footer
            ) | gap(40) | center)
        ) | mesh(0x6d28d9, 0x0891b2, 0x060810) | clip_x;
    }
};

int main() {
    static_assert(SurfaceProgram<Nova>);
    return live<Nova>({ .port = 8080, .page_bg = 0x060810, .title = "waya \u00b7 nova" });
}
