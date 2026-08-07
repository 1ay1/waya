/// examples/pulse.cpp — PULSE: a product-grade analytics dashboard. A fixed
/// sidebar, a top bar with live status, four refined KPI cards with real
/// sparklines and trend deltas, a large area chart with a range switch, a
/// traffic bar chart, and a live activity feed. One server clock drives it; a
/// pause control shows a Sub is just a function of the Model.
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
using namespace waya::surface::literals;
using namespace waya::ui;

struct Pulse {
    // ── palette ─────────────────────────────────────────────────────────────
    static constexpr std::uint32_t bg     = 0x0a0d16;
    static constexpr std::uint32_t side   = 0x0c1019;
    static constexpr std::uint32_t card   = 0x0f1420;
    static constexpr std::uint32_t line   = 0x1c2434;
    static constexpr std::uint32_t ink    = 0xeef2f8;
    static constexpr std::uint32_t body_c = 0x8b98af;
    static constexpr std::uint32_t faint  = 0x566579;
    static constexpr std::uint32_t brand  = 0x6d7cff;
    static constexpr std::uint32_t good   = 0x2ee6a6;
    static constexpr std::uint32_t bad    = 0xff6b81;

    struct Metric {
        std::string label; std::string prefix; std::string suffix;
        long value; float delta; std::uint32_t hue;
        std::vector<float> spark;
    };

    struct Model {
        long tick = 0; bool live = true; int range = 1;
        std::array<Metric,4> kpis = {{
            { "Active users",  "",  "",  8642, +12.4f, 0x6d7cff, {30,34,32,40,38,46,44,52,50,58} },
            { "Requests / s",  "",  "",  1284, +4.1f,  0x2ee6a6, {40,38,44,41,52,48,55,50,58,54} },
            { "Revenue",       "$", "",  9320, +8.7f,  0x00d4ff, {20,24,22,28,30,27,33,36,34,40} },
            { "Error rate",    "",  "%",   4,  -2.3f,  0xff6b81, { 9, 8,11, 7, 6, 8, 5, 6, 4, 4} },
        }};
        std::vector<float> traffic = {32,48,40,66,58,72,61,80,74,90,68,84};
        std::vector<float> chart   = {30,42,38,55,49,63,58,71,66,78,74,86,82,94};
        std::deque<std::string> feed = {
            "Deploy to production succeeded",
            "alex.k upgraded to the Pro plan",
            "Cache warmed \u00b7 42ms p95 latency",
        };
    };

    struct Tick {}; struct ToggleLive {}; struct Range { int i; };
    using Msg = std::variant<Tick, ToggleLive, Range>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model,Cmd<Msg>> {
                m.tick++;
                auto wander = [&](long v, long lo, long hi, int seed){
                    long d = (long)((m.tick*7 + seed*13) % 11) - 5;
                    v += d; return v < lo ? lo : v > hi ? hi : v;
                };
                m.kpis[0].value = wander(m.kpis[0].value, 6000, 12000, 1);
                m.kpis[1].value = wander(m.kpis[1].value, 800, 2000, 2);
                m.kpis[2].value = wander(m.kpis[2].value, 6000, 14000, 3);
                m.kpis[3].value = wander(m.kpis[3].value, 1, 20, 4);
                for (auto& k : m.kpis) {
                    k.spark.push_back((float)k.value);
                    if (k.spark.size() > 10) k.spark.erase(k.spark.begin());
                }
                m.chart.push_back((float)m.kpis[0].value/130.f);
                if (m.chart.size() > 14) m.chart.erase(m.chart.begin());
                for (auto& t : m.traffic)
                    t = std::max(10.f, std::min(100.f, t + ((m.tick+(long)t)%7) - 3));
                if (m.tick % 5 == 0) {
                    static const char* evt[] = {
                        "New signup from Berlin", "Background job queue drained",
                        "Invoice #4821 was paid", "Webhook delivered \u00b7 200 OK",
                        "Session continued on mobile" };
                    m.feed.push_front(evt[(m.tick/5)%5]);
                    if (m.feed.size() > 6) m.feed.pop_back();
                }
                return { m, Cmd<Msg>::none() };
            },
            [&](ToggleLive) -> std::pair<Model,Cmd<Msg>> { m.live=!m.live; return {m,Cmd<Msg>::none()}; },
            [&](Range r)    -> std::pair<Model,Cmd<Msg>> { m.range=r.i; return {m,Cmd<Msg>::none()}; },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model& m) {
        return m.live ? Sub<Msg>::every(std::chrono::milliseconds(1000), Tick{}) : Sub<Msg>::none();
    }

    // ── helpers ─────────────────────────────────────────────────────────────
    static Mod border(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }
    static Mod surface() {
        return detail::raw_css("background", "#0f1420") ;
    }
    static NodeRef panel(NodeRef inner) {
        return inner | pad(20) | round(14) | surface() | border();
    }

    static NodeRef nav_item(std::string ico, std::string label, bool active) {
        auto n = row(icon(ico, 18) | fg(active ? ink : body_c),
                     text(label) | fg(active ? ink : body_c) | font(14)
                        | weight(active ? Weight::semibold : Weight::medium))
               | gap(12) | items_center | pad_x(12) | pad_y(10) | round(9) | pointer
               | transition("background-color .15s ease, color .15s ease");
        if (active) n = n | detail::raw_css("background", "rgba(109,124,255,.12)");
        else n = n | on(Hover, detail::raw_css("background", "rgba(255,255,255,.04)"));
        return n;
    }

    static NodeRef sidebar() {
        auto logo = row(
            box(icon("home", 17) | fg(0xffffff)) | square(30) | center | round(8)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)"),
            text("Pulse") | fg(ink) | font(17) | weight(Weight::bold)
        ) | gap(10) | items_center;

        return col(
            logo,
            box() | h(8),
            nav_item("home", "Overview", true),
            nav_item("user", "Audience", false),
            nav_item("bell", "Alerts", false),
            nav_item("settings", "Settings", false),
            box() | grow(),
            row(box(text("AK") | fg(0xffffff) | font(12) | weight(Weight::bold))
                    | square(32) | center | circle(16)
                    | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#a78bfa)"),
                col(text("Alex Kim") | fg(ink) | font(13) | weight(Weight::semibold),
                    text("Pro plan") | fg(faint) | font(12)) | gap(1))
                | gap(10) | items_center | pad(10) | round(10) | border()
        ) | gap(6) | pad(18) | w(232)
          | detail::raw_css("height", "100vh") | sticky_top(0)
          | detail::raw_css("background", "#0c1019")
          | detail::raw_css("border-right", "1px solid #161d2b");
    }

    static NodeRef kpi(const Metric& k) {
        bool up = k.delta >= 0;
        std::string big = k.prefix + std::to_string(k.value) + k.suffix;
        char d[16]; std::snprintf(d, sizeof d, "%+.1f%%", k.delta);
        return panel(col(
            row(text(k.label) | fg(body_c) | font(13) | weight(Weight::medium),
                box() | grow(),
                row(icon(up?"chevron-up":"chevron-down", 13) | fg(up?good:bad),
                    text(d) | fg(up?good:bad) | font(12) | weight(Weight::semibold) | tabular_nums)
                    | gap(2) | items_center) | items_center,
            text(big) | fg(ink) | font(30) | weight(Weight::black) | tabular_nums
                | detail::raw_css("letter-spacing", "-0.02em"),
            box(sparkline(k.spark, 260, 40) | stroke(k.hue, 2.5f) | w_full | h_full)
                | w_full | h(40)
        ) | gap(12)) | grow() | detail::raw_css("flex", "1 1 200px") | min_w(200);
    }

    static NodeRef seg(const Model& m, int i, std::string label) {
        bool on = m.range == i;
        auto n = text(label) | font(13) | weight(Weight::semibold)
               | pad_x(14) | pad_y(7) | round(8) | pointer | tap(Range{i})
               | fg(on ? ink : body_c);
        if (on) n = n | detail::raw_css("background", "#1a2131")
                     | detail::raw_css("box-shadow", "0 1px 2px rgba(0,0,0,.3)");
        return n;
    }

    static NodeRef view(const Model& m) {
        auto topbar = row(
            col(text("Overview") | fg(ink) | font(22) | weight(Weight::bold)
                    | detail::raw_css("letter-spacing","-0.02em"),
                text("Last updated just now") | fg(faint) | font(13)) | gap(2),
            box() | grow(),
            row(box() | circle(7) | detail::raw_css("background", m.live?"#2ee6a6":"#566579")
                    | (m.live ? breathe() : noop),
                text(m.live ? "Live" : "Paused") | fg(m.live?good:body_c) | font(13) | weight(Weight::semibold),
                row(icon(m.live?"loader":"chevron-right", 14) | fg(ink),
                    text(m.live ? "Pause" : "Resume") | fg(ink) | font(13) | weight(Weight::semibold))
                    | gap(7) | items_center | pad_x(14) | pad_y(9) | round(9) | border()
                    | pointer | on(Hover, detail::raw_css("border-color","#2a3446"))
                    | tap(ToggleLive{}) | transition("border-color .15s ease")
            ) | gap(14) | items_center
        ) | items_center | w_full;

        auto kpis = row(kpi(m.kpis[0]), kpi(m.kpis[1]), kpi(m.kpis[2]), kpi(m.kpis[3]))
                  | gap(16) | wrap;

        auto chart_panel = panel(col(
            row(col(text("Throughput") | fg(ink) | font(16) | weight(Weight::semibold),
                    text("Requests over the selected window") | fg(faint) | font(12)) | gap(3),
                box() | grow(),
                row(seg(m,0,"1H"), seg(m,1,"24H"), seg(m,2,"7D"))
                    | gap(2) | pad(3) | round(10) | detail::raw_css("background","#0b0f1a")
                    | border()) | items_center,
            box(area_chart(m.chart, 640, 210) | fg(brand) | stroke(0x8b96ff, 2.5f) | w_full | h_full)
                | w_full | h(210)
        ) | gap(18)) | grow() | min_w(340);

        auto traffic = panel(col(
            text("Traffic by hour") | fg(ink) | font(16) | weight(Weight::semibold),
            box(bars(m.traffic, 300, 210) | fg(0x00d4ff) | w_full | h_full)
                | w_full | h(210)
        ) | gap(18)) | w(300);

        // feed
        std::vector<NodeRef> feed_items;
        {
            int i = 0;
            for (auto& s : m.feed) {
                if (i > 0) feed_items.push_back(box() | h(1) | w_full | detail::raw_css("background","#161d2b"));
                feed_items.push_back(
                    row(box() | circle(7) | detail::raw_css("background", i==0?"#2ee6a6":"#3a465e")
                            | (i==0 ? glow(good, 8) : noop),
                        text(s) | fg(i==0?ink:body_c) | font(14),
                        box() | grow(),
                        text(i==0?"now":std::to_string(i*5)+"s") | fg(faint) | font(12) | tabular_nums)
                    | gap(12) | items_center | pad_y(11)
                    | key(std::to_string(m.tick - i)) | fade_up(280));
                i++;
            }
        }
        auto feed = panel(col(
            row(text("Activity") | fg(ink) | font(16) | weight(Weight::semibold),
                box() | grow(),
                row(box() | circle(6) | detail::raw_css("background","#2ee6a6") | breathe(),
                    text("live") | fg(good) | font(12) | weight(Weight::semibold))
                    | gap(6) | items_center) | items_center,
            col_(std::move(feed_items))
        ) | gap(6)) | w_full;

        auto content = col(topbar, kpis, row(chart_panel, traffic) | gap(16) | wrap, feed)
                     | gap(20) | pad(28) | grow() | min_w(0);

        return row(sidebar(), content)
             | items_stretch | min_h(100_vh) | detail::raw_css("background", "#0a0d16")
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Pulse \u00b7 analytics"; 
        mt.description = "A real-time analytics dashboard, server-rendered in C++ with waya.";
        return mt;
    }
};

int main() { return live<Pulse>({ .port = 8080, .page_bg = 0x0a0d16, .title = "Pulse" }); }
