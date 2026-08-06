/// examples/pulse.cpp — a live, collaborative ops dashboard. Animated metric
/// bars, a keyed live activity feed, and BROADCAST presence: open two tabs and a
/// ping in one lights up in the other, instantly. Real-time + multiplayer, from
/// pure update. Built from frost()/tint()/hairline()/radial()/hover_glow().
///
///   cmake --build build -j && ./build/pulse     # open TWO tabs on :8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::ui;

struct Pulse {
    struct Event { long id; std::string text; };
    struct Model {
        int cpu = 34, mem = 61, net = 22, ops = 128;
        std::vector<Event> feed;
        long next_id = 1, tick = 0;
    };
    struct Tick {}; struct Ping {}; struct Recv { std::string payload; };
    using Msg = std::variant<Tick, Ping, Recv>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model,Cmd<Msg>> {
                auto wander = [&](int v, int lo, int hi){ int d=(int)((m.tick*7+v*13)%11)-5; v+=d; return v<lo?lo:v>hi?hi:v; };
                m.tick++;
                m.cpu=wander(m.cpu,8,96); m.mem=wander(m.mem,20,92);
                m.net=wander(m.net,2,88);  m.ops=100+(int)((m.tick*17)%90);
                return { m, Cmd<Msg>::none() };
            },
            [&](Ping) -> std::pair<Model,Cmd<Msg>> {
                return { m, Cmd<Msg>::broadcast("pulse", "someone pinged the cluster") };
            },
            [&](const Recv& r) -> std::pair<Model,Cmd<Msg>> {
                m.feed.insert(m.feed.begin(), { m.next_id++, r.payload });
                if (m.feed.size() > 8) m.feed.pop_back();
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::batch({
            Sub<Msg>::every(1000, Tick{}),
            Sub<Msg>::on_topic("pulse", [](std::string p){ return Recv{p}; })
        });
    }

    static NodeRef metric(std::string name, int pct, std::uint32_t c) {
        return col(
            row(
                text(name) | fg(muted) | caption | weight(Weight::bold) | css("text-transform","uppercase") | css("letter-spacing",".05em"),
                text(std::to_string(pct)+"%") | fg(ink) | caption | mono
            ) | css("justify-content","space-between"),
            box(box() | css("width", std::to_string(pct)+"%") | css("height","100%") | round(999)
                      | gradient_bg(c, c, 90) | css("transition","width .8s cubic-bezier(.2,.7,.2,1)")
                      | css("box-shadow","0 0 12px "+detail::rgba_hex(c,0.4f)))
              | css("height","8px") | round(999) | tint(0xffffff, 0.06f)
        ) | gap(8);
    }

    template <typename... Cs>
    static NodeRef card(Cs... cs) {
        return col(std::move(cs)...) | gap(16) | pad(22) | round(18) | frost(10) | elevation(2);
    }

    static NodeRef view(const Model& m) {
        auto header = row(
            row(
                box() | size(px(10)) | round(999) | bg(good) | pulse(1400),
                text("cluster \u00b7 live") | fg(ink) | subtitle | weight(Weight::bold)
            ) | gap(10) | center,
            text("ping cluster") | fg(white) | semibold | font(14)
                | pad_x(18) | pad_y(10) | round(10) | bg(brand)
                | hover_glow(brand, 22) | interactive() | tap(Ping{})
        ) | between | wrap | center | gap(16);

        auto metrics = card(
            text("Metrics") | fg(muted) | label,
            metric("cpu", m.cpu, 0x818cf8),
            metric("memory", m.mem, 0x22d3ee),
            metric("network", m.net, 0x34d399),
            row(
                text(std::to_string(m.ops)) | fg(0xc7d2fe) | display | font(44) | glow_text(0x818cf8, 18)
                    | css("font-variant-numeric","tabular-nums"),
                text("ops/sec") | fg(muted) | caption
            ) | gap(10) | css("align-items","baseline")
        ) | gradient_border(0x6366f1, 0x22d3ee, 1);

        std::vector<NodeRef> rows;
        for (auto& e : m.feed)
            rows.push_back(
                row(box() | size(px(6)) | round(999) | bg(brand2),
                    text(e.text) | fg(ink) | body)
                | key("ev:"+std::to_string(e.id))
                | gap(10) | center | pad_x(14) | pad_y(10) | round(10) | tint(0xffffff,0.03f) | fade_down(400));
        auto feedbox = box_(std::move(rows)); feedbox->style.flow = Flow::col; finalize(*feedbox);
        auto feed = card(
            text("Live activity \u00b7 open a 2nd tab") | fg(muted) | label,
            (m.feed.empty()
                ? (text("no events yet \u2014 press ping") | fg(faint) | caption | pad_y(20) | center)
                : (feedbox | gap(8)))
        ) | grow(1);

        return page(0x080a12,
            centered(62, col(
                header,
                row(metrics | grow(1) | css("min-width","18rem"), feed | grow(1) | css("min-width","18rem"))
                    | gap(20) | wrap | css("align-items","stretch")
            ) | gap(24))
        ) | radial(brand, 80, -10, 0x080a12, 50);
    }
};

int main() {
    static_assert(SurfaceProgram<Pulse>);
    return live<Pulse>({ .port = 8080, .page_bg = 0x080a12, .title = "waya \u2014 pulse" });
}
