// examples/pixels.cpp — a live collaborative pixel canvas ("r/place" in one file).
//
// Every visitor paints on ONE shared grid. Because the canvas lives on the
// server, a new tab sees the existing art instantly (real SSR first paint), and
// every stroke fans out to all connected sessions over a WebSocket — so two
// tabs side by side paint on the same board in real time. The whole thing is a
// pure update + view; the only "backend" is a mutex-guarded array of colours.
//
//   waya run pixels           (or: WAYA_PORT=8080 ./build/pixels)
//
// It shows off what waya is uniquely good at: real-time, multiplayer, and
// server-rendered — with no client JS, no database, and no HTML/CSS in sight.

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <array>
#include <mutex>
#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;

// ── The shared board (process-global, the ONE source of truth) ──────────────
static constexpr int W = 32, H = 32, N = W * H;

// A 16-colour palette (indices 0..15; 0 = the empty/background cell).
static constexpr std::array<std::uint32_t, 16> kPalette = {
    0x1b1f2a, 0xffffff, 0xe4e4e4, 0x888888, 0x222222, 0xe53935, 0xff7043, 0xffb300,
    0xfdd835, 0x66bb6a, 0x26a69a, 0x29b6f6, 0x3f51b5, 0x7e57c2, 0xec407a, 0x8d6e63,
};

struct Board {
    std::mutex m;
    std::array<std::uint8_t, N> px{};   // colour index per cell (0 = empty)
    std::uint64_t version = 0;          // bumped on every paint
    long long strokes = 0;              // total pixels painted (a shared stat)
};
static Board& board() { static Board b; return b; }

// Snapshot the shared board into a plain value the pure view can read.
struct Snapshot { std::array<std::uint8_t, N> px; std::uint64_t version; long long strokes; };
static Snapshot snapshot() {
    auto& b = board(); std::lock_guard<std::mutex> l(b.m);
    return { b.px, b.version, b.strokes };
}

// ── App ─────────────────────────────────────────────────────────────────────
struct Model {
    int      color = 5;    // the palette index this user is painting with
    Snapshot board_ = snapshot();   // this session's view of the shared board
};

struct Pick   { int color; };        // choose a palette colour
struct Paint  { int cell; };         // paint a cell with the current colour
struct Synced { };                   // a broadcast told us the board changed
struct Clear  { };                   // wipe the board (everyone)

using Msg = std::variant<Pick, Paint, Synced, Clear>;

struct App {
    using Model = ::Model;
    using Msg   = ::Msg;

    static Model init() { return {}; }   // reads the current shared board via Snapshot's default

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Pick p) -> std::pair<Model, Cmd<Msg>> {
                m.color = p.color;
                return { m, Cmd<Msg>::none() };
            },
            [&](Paint p) -> std::pair<Model, Cmd<Msg>> {
                if (p.cell >= 0 && p.cell < N) {
                    auto& b = board();
                    { std::lock_guard<std::mutex> l(b.m);
                      b.px[(std::size_t)p.cell] = (std::uint8_t)m.color;
                      ++b.version; ++b.strokes; }
                    // Tell EVERY session (including this one) to re-read the board.
                    return { m, Cmd<Msg>::broadcast("canvas", "paint") };
                }
                return { m, Cmd<Msg>::none() };
            },
            [&](Clear) -> std::pair<Model, Cmd<Msg>> {
                auto& b = board();
                { std::lock_guard<std::mutex> l(b.m); b.px.fill(0); ++b.version; }
                return { m, Cmd<Msg>::broadcast("canvas", "clear") };
            },
            [&](Synced) -> std::pair<Model, Cmd<Msg>> {
                m.board_ = snapshot();   // pull the latest shared state
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    // Subscribe to the shared "canvas" topic: any paint anywhere -> Synced -> repaint.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_topic("canvas", [](std::string) { return Msg{Synced{}}; });
    }

    static NodeRef view(const Model& m) {
        // The board: one keyed cell per pixel. Keyed so the diff only ships the
        // cells that actually changed (usually one), not the whole 1024-cell grid.
        auto cells = each_i(m.board_.px, [&](std::uint8_t idx, std::size_t i) {
            std::uint32_t c = kPalette[idx < kPalette.size() ? idx : 0];
            return box()
                | bg(c) | aspect(1.0f) | pointer
                | tap(Paint{ (int)i })
                | round(2)
                | detail::raw_css("transition", "background .08s")
                | key("p" + std::to_string(i));
        });
        auto canvas = box(); canvas->kids = std::move(cells); finalize(*canvas);
        canvas = canvas | grid_cols(W) | gap(1) | w_full | max_w(560);

        // The palette: a swatch per colour; the active one gets a ring.
        auto swatches = each_i(std::vector<std::uint32_t>(kPalette.begin() + 1, kPalette.end()),
            [&](std::uint32_t c, std::size_t k) {
                int idx = (int)k + 1;
                auto sw = box() | bg(c) | w(30) | h(30) | round(8) | pointer | tap(Pick{ idx });
                if (idx == m.color) sw = sw | ring(0xffffff, 3);
                return sw;
            });
        auto palette = box(); palette->kids = std::move(swatches); finalize(*palette);
        palette = palette | horizontal | detail::raw_css("flex-wrap", "wrap") | gap(8) | items_center;

        return col(
            row(text("\xf0\x9f\x8e\xa8  waya/place") | heading,
                box() | grows,
                text(std::to_string(m.board_.strokes) + " strokes")
                    | fg_muted | detail::raw_css("font-size", "13px"))
                | items_center | mb(4),
            text("Paint on a shared canvas \u2014 open a second tab and watch it sync live.")
                | fg_muted | detail::raw_css("font-size", "14px") | mb(16),
            canvas,
            row(palette, box() | grows, button("Clear", Clear{}))
                | items_center | gap(16) | mt(16))
            | max_w(600) | mx_auto | pad(28) | gap(0);
    }

    static Meta meta(const Model&) {
        return { .title = "waya/place \u2014 collaborative pixel canvas",
                 .description = "A live, multiplayer pixel-art board rendered on the server with waya." };
    }
    // Analytics-ready; no <head> injection needed for the demo.
};

int main() { return live<App>({ .port = 8080, .title = "waya/place" }); }
