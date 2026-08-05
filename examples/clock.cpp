/// examples/clock.cpp — effects that RUN. This is the counter's grown-up
/// sibling: it proves the surface runtime actually interprets `Cmd` and
/// reconciles `Sub`.
///
///   cmake --build build -j && ./build/clock     # http://localhost:8080
///
/// What it shows, all in pure `update`/`subscribe` — no threads, no timers, no
/// I/O in the app code, only *descriptions* the runtime performs:
///
///   • Sub::every       a live ticking clock (1s), started/stopped by the model
///   • Cmd::after       a self-extinguishing "saved!" flash (debounce shape)
///   • Cmd::navigate    real client-side routing between two screens
///   • Sub::on_route    the URL drives the model back
///
/// Toggle the clock off and the runtime stops the interval; toggle it on and it
/// starts again. That reconciliation is the whole point of subscriptions.

#include <waya/surface/live.hpp>

using namespace waya::surface;

struct Clock {
    struct Model {
        int  seconds = 0;
        bool running = true;
        bool flash   = false;    // "saved!" toast, auto-hidden by Cmd::after
        std::string route = "/"; // current screen, driven by the URL
    };
    using Msg = int;
    enum { Tick, Toggle, Save, ClearFlash, GoHome, GoAbout, Route };

    static Model init() { return {}; }

    // update is pure: it returns the next model AND a description of effects.
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        switch (msg) {
            case Tick:       m.seconds++;                 return { m, Cmd<Msg>::none() };
            case Toggle:     m.running = !m.running;      return { m, Cmd<Msg>::none() };
            case Save:       m.flash = true;
                             // hide the toast after 1.2s — a timed, self-sent Msg
                             return { m, Cmd<Msg>::after(1200ms, ClearFlash) };
            case ClearFlash: m.flash = false;             return { m, Cmd<Msg>::none() };
            case GoHome:     return { m, Cmd<Msg>::navigate("/") };       // route!
            case GoAbout:    return { m, Cmd<Msg>::navigate("/about") };
            case Route:      m.route = value.empty() ? "/" : value; return { m, Cmd<Msg>::none() };
        }
        return { m, Cmd<Msg>::none() };
    }

    // subscribe declares standing event sources as a function of the model.
    // When `running` flips, the runtime starts/stops the interval for us.
    static Sub<Msg> subscribe(const Model& m) {
        return Sub<Msg>::batch({
            m.running ? Sub<Msg>::every(1000ms, Tick) : Sub<Msg>::none(),
            Sub<Msg>::on_route([](std::string path){ return Route; })  // value carries the path
        });
    }

    static NodeRef view(const Model& m) {
        auto pill = [](std::string label, int msg, std::uint32_t c) {
            return text(std::move(label)) | fg(0xffffff) | font(16) | semibold
                 | pad_x(18) | pad_y(10) | bg(c) | round(999)
                 | tap(msg) | transition() | on(Hover, opacity(0.85f));
        };
        auto link = [&](std::string label, int msg, bool active) {
            return text(std::move(label)) | fg(active ? 0x818cf8 : 0x64748b)
                 | font(15) | (active ? semibold : noop) | tap(msg)
                 | pad_x(4) | transition() | on(Hover, fg(0xa5b4fc));
        };

        // A tiny two-screen router: the same Model, a different view per route.
        NodeRef body = (m.route == "/about")
            ? col(
                text("about") | fg(0xe2e8f0) | font(40) | weight(Weight::black),
                text("A live clock built from pure update + effects-as-data. "
                     "The seconds tick over a WebSocket; only the digit that "
                     "changed is sent.") | fg(0x94a3b8) | font(16)
                     | css("max-width", "34rem") | css("line-height", "1.6")
              ) | gap(16) | center
            : col(
                text(hhmmss(m.seconds)) | fg(0x818cf8) | font(76) | weight(Weight::black)
                    | css("font-variant-numeric", "tabular-nums"),
                row(
                    pill(m.running ? "pause" : "start", Toggle, m.running ? 0x334155 : 0x6366f1),
                    pill("save", Save, 0x1e293b)
                ) | gap(12),
                (m.flash ? (text("saved ✓") | fg(0x34d399) | font(14) | semibold)
                         : (text("") | css("height", "1.25rem")))
              ) | gap(24) | center;

        return col(
            row(
                link("clock", GoHome,  m.route != "/about"),
                link("about", GoAbout, m.route == "/about")
            ) | gap(20),
            body
        ) | center | gap(36) | pad(56) | round(24) | bg(0x111827)
          | shadow() | border(1, 0x1f2937);
    }

    static std::string hhmmss(int s) {
        char b[16];
        std::snprintf(b, sizeof(b), "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
        return b;
    }
};

int main() {
    static_assert(SurfaceProgram<Clock>);
    return live<Clock>({.port = 8080});
}
