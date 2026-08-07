/// examples/life.cpp — LIFE: Conway's Game of Life, live and interactive. Click
/// cells to toggle them, hit play, and the CA evolves on a server-side tick,
/// streaming only the changed cells over the socket. Presets (glider, pulsar,
/// Gosper gun), a speed dial, generation + population counters. The whole
/// simulation is a std::vector<bool> in the Model; the grid is 1800 keyed cells.
///
///   waya run life             # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Life {
    static constexpr int W = 60, H = 34;

    static constexpr std::uint32_t bg    = 0x070a12;
    static constexpr std::uint32_t line  = 0x1a2233;
    static constexpr std::uint32_t ink   = 0xeef2f8;
    static constexpr std::uint32_t body_c= 0x8b98af;
    static constexpr std::uint32_t faint = 0x556074;
    static constexpr std::uint32_t brand = 0x6d7cff;
    static constexpr std::uint32_t live_c= 0x34e0a1;

    struct Model {
        std::vector<char> cells = std::vector<char>(W * H, 0);
        bool running = false;
        long gen = 0;
        int  speed = 6;          // 1..10
        long pop = 0;
    };

    struct Toggle { int i; }; struct Play {}; struct Step {}; struct Clear {};
    struct Rand {}; struct Speed { int d; }; struct Preset { int which; }; struct Tick {};
    using Msg = std::variant<Toggle, Play, Step, Clear, Rand, Speed, Preset, Tick>;

    static Model init() { Model m; stamp(m, 2); return m; }   // start with a pulsar

    static int idx(int x, int y) { return ((y + H) % H) * W + ((x + W) % W); }

    static Model advance(Model m) {
        std::vector<char> nxt(W * H, 0);
        long pop = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dx || dy) n += m.cells[idx(x + dx, y + dy)];
                bool alive = m.cells[idx(x, y)];
                bool next = alive ? (n == 2 || n == 3) : (n == 3);
                nxt[idx(x, y)] = next;
                pop += next;
            }
        m.cells = std::move(nxt); m.gen++; m.pop = pop;
        return m;
    }

    static long count(const std::vector<char>& c) { long p = 0; for (char v : c) p += v; return p; }

    // stamp a preset near the centre. which: 0 glider, 1 gun, 2 pulsar
    static void stamp(Model& m, int which) {
        auto put = [&](int x, int y) { m.cells[idx(x, y)] = 1; };
        int cx = W / 2, cy = H / 2;
        if (which == 0) { // glider (top-left)
            int x = 4, y = 3;
            put(x+1,y); put(x+2,y+1); put(x,y+2); put(x+1,y+2); put(x+2,y+2);
        } else if (which == 1) { // Gosper glider gun
            int ox = 2, oy = 3;
            const char* g[] = {
                "........................O...........",
                "......................O.O...........",
                "............OO......OO............OO",
                "...........O...O....OO............OO",
                "OO........O.....O...OO..............",
                "OO........O...O.OO....O.O...........",
                "..........O.....O.......O...........",
                "...........O...O....................",
                "............OO......................",
            };
            for (int r = 0; r < 9; ++r)
                for (int col = 0; g[r][col]; ++col)
                    if (g[r][col] == 'O') put(ox + col, oy + r);
        } else { // pulsar
            const int pts[][2] = {{2,0},{3,0},{4,0},{0,2},{5,2},{0,3},{5,3},{0,4},{5,4},{2,5},{3,5},{4,5}};
            for (int q = 0; q < 4; ++q)
                for (auto& p : pts) {
                    int sx = (q & 1) ? -1 : 1, sy = (q & 2) ? -1 : 1;
                    int bx = (q & 1) ? -1 : 1, by = (q & 2) ? -1 : 1;
                    put(cx + bx + sx * p[0], cy + by + sy * p[1]);
                }
        }
        m.pop = count(m.cells);
    }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Toggle t) { m.cells[t.i] = !m.cells[t.i]; m.pop = count(m.cells); },
            [&](Play)     { m.running = !m.running; },
            [&](Step)     { m = advance(std::move(m)); },
            [&](Clear)    { std::fill(m.cells.begin(), m.cells.end(), 0); m.gen = 0; m.pop = 0; m.running = false; },
            [&](Rand)     { for (auto& c : m.cells) c = (std::rand() & 7) == 0; m.gen = 0; m.pop = count(m.cells); },
            [&](Speed s)  { m.speed += s.d; if (m.speed < 1) m.speed = 1; if (m.speed > 10) m.speed = 10; },
            [&](Preset p) { std::fill(m.cells.begin(), m.cells.end(), 0); m.gen = 0; stamp(m, p.which); },
            [&](Tick)     { m = advance(std::move(m)); },
        }, msg);
        return m;
    }

    static Sub<Msg> subscribe(const Model& m) {
        if (!m.running) return Sub<Msg>::none();
        int ms = 320 - m.speed * 28;   // speed 1->292ms .. 10->40ms
        return Sub<Msg>::every(std::chrono::milliseconds(ms), Tick{});
    }

    static Mod bord(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }

    // the grid, drawn as one SVG for speed: a rect per LIVE cell only (dead
    // cells are the background), so an empty board ships almost nothing.
    static NodeRef grid(const Model& m) {
        const int CELL = 15;
        std::string svg;
        svg.reserve(m.pop * 40 + 128);
        svg += "<svg viewBox='0 0 " + std::to_string(W*CELL) + " " + std::to_string(H*CELL) +
               "' style='width:100%;height:auto;display:block'>";
        // faint grid dots
        svg += "<rect width='100%' height='100%' fill='#0a0e18'/>";
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (m.cells[idx(x, y)]) {
                    int px = x * CELL, py = y * CELL;
                    svg += "<rect x='" + std::to_string(px+1) + "' y='" + std::to_string(py+1) +
                           "' width='" + std::to_string(CELL-2) + "' height='" + std::to_string(CELL-2) +
                           "' rx='2' fill='#34e0a1'/>";
                }
        svg += "</svg>";

        // an overlay of transparent tap targets, keyed so only toggled cells diff.
        std::vector<NodeRef> cells;
        cells.reserve(W * H);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                int i = idx(x, y);
                cells.push_back(
                    box() | detail::raw_css("width", std::to_string(CELL) + "px")
                          | detail::raw_css("height", std::to_string(CELL) + "px")
                          | pointer | tap(Toggle{ i }) | key(std::to_string(i)));
            }
        auto overlay = box_(std::move(cells))
            | detail::raw_css("position", "absolute") | detail::raw_css("inset", "0")
            | detail::raw_css("display", "grid")
            | detail::raw_css("grid-template-columns", "repeat(" + std::to_string(W) + ",1fr)");

        return stack(markup(std::move(svg)) | detail::raw_css("line-height", "0"), overlay)
            | detail::raw_css("position", "relative")
            | round(12) | detail::raw_css("overflow", "hidden") | bord(0x243049)
            | detail::raw_css("box-shadow", "0 30px 80px -30px rgba(0,0,0,.8)");
    }

    static NodeRef btn(std::string label, Msg msg, bool primary = false) {
        auto n = text(std::move(label)) | font(13) | weight(Weight::semibold)
               | pad_x(14) | pad_y(9) | round(9) | pointer | tap(msg)
               | transition("background-color .15s ease, border-color .15s ease");
        if (primary)
            n = n | fg(0xffffff)
                  | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
                  | detail::raw_css("box-shadow", "0 6px 18px -6px rgba(109,124,255,.7)")
                  | on(Hover, brightness(112));
        else
            n = n | fg(ink) | detail::raw_css("background", "rgba(255,255,255,.04)") | bord(0x2a3446)
                  | on(Hover, detail::raw_css("border-color", "#3a4560"));
        return n;
    }

    static NodeRef stat(std::string k, std::string v, std::uint32_t vc = ink) {
        return col(text(k) | fg(faint) | font(11) | uppercase | tracking_em(0.10f) | weight(Weight::semibold),
                   text(v) | fg(vc) | font(20) | weight(Weight::black) | tabular_nums) | gap(2);
    }

    static NodeRef view(const Model& m) {
        auto title = row(
            box(text("\u25C8") | font(18) | fg(0xffffff)) | square(34) | center | round(9)
                | detail::raw_css("background", "linear-gradient(135deg,#34e0a1,#6d7cff)") | glow(live_c, 14),
            col(text("Life") | fg(ink) | font(20) | weight(Weight::black) | detail::raw_css("letter-spacing","-0.02em"),
                text("Conway's cellular automaton") | fg(faint) | font(12)) | gap(1)
        ) | gap(12) | items_center;

        auto controls = row(
            btn(m.running ? "\u2759\u2759 Pause" : "\u25B6 Play", Play{}, true),
            btn("Step", Step{}),
            btn("Random", Rand{}),
            btn("Clear", Clear{}),
            box() | w(1) | h(22) | detail::raw_css("background", "#243049"),
            btn("Glider", Preset{0}),
            btn("Gun", Preset{1}),
            btn("Pulsar", Preset{2}),
            box() | grow(),
            row(btn("\u2212", Speed{-1}),
                col(text("SPEED") | fg(faint) | font(10) | tracking_em(0.10f),
                    text(std::to_string(m.speed) + "x") | fg(ink) | font(13) | weight(Weight::bold) | tabular_nums) | gap(0) | items_center,
                btn("+", Speed{+1})) | gap(8) | items_center
        ) | gap(9) | items_center | wrap | w_full;

        auto stats = row(
            stat("Generation", std::to_string(m.gen), brand),
            box() | w(1) | h(34) | detail::raw_css("background", "#1c2436"),
            stat("Population", std::to_string(m.pop), live_c),
            box() | w(1) | h(34) | detail::raw_css("background", "#1c2436"),
            stat("Grid", std::to_string(W) + "\u00d7" + std::to_string(H)),
            box() | grow(),
            row(box() | circle(7) | detail::raw_css("background", m.running ? "#34e0a1" : "#556074")
                    | (m.running ? breathe() : noop),
                text(m.running ? "evolving" : "paused") | fg(m.running ? live_c : body_c) | font(13) | weight(Weight::semibold))
                | gap(8) | items_center
        ) | gap(20) | items_center | wrap | pad(18) | round(14)
          | detail::raw_css("background", "linear-gradient(180deg,#0d1322,#0a0f1b)") | bord();

        auto hint = text("Click any cell to toggle it \u00b7 draw a pattern, then Play")
                  | fg(faint) | font(13) | text_center;

        return col(
            row(title, box() | grow()) | w_full,
            controls,
            grid(m),
            stats,
            hint
        ) | gap(16) | pad(28) | max_w(1000) | center_x | min_h(100_vh)
          | detail::raw_css("background",
              "radial-gradient(1100px 560px at 50% -5%, rgba(52,224,161,.10), transparent 55%),"
              "radial-gradient(800px 500px at 85% 20%, rgba(109,124,255,.10), transparent 55%), #070a12")
          | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Life \u00b7 Conway's Game of Life";
        mt.description = "Conway's Game of Life, simulated server-side in C++ and streamed as deltas with waya.";
        return mt;
    }
};

int main() { return live<Life>({ .port = 8080, .page_bg = 0x070a12, .title = "Life" }); }
