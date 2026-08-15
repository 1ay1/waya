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

// A 16-colour palette (indices 0..15; 0 = the empty/background cell). Warm,
// saturated hues that pop on a dark canvas — this is example POLISH, not core.
static constexpr std::array<std::uint32_t, 16> kPalette = {
    0x151a26, 0xffffff, 0xc7d0e0, 0x8a94a6, 0x3a4152, 0xff4d6d, 0xff8c42, 0xffd23f,
    0x8ce99a, 0x37d67a, 0x22c3e6, 0x4d9dff, 0x6c5ce7, 0xb17cff, 0xff6bcb, 0xa9744f,
};

struct Board {
    std::mutex m;
    std::array<std::uint8_t, N> px{};   // colour index per cell (0 = empty)
    std::uint64_t version = 0;          // bumped on every paint
    long long strokes = 0;              // total pixels painted (a shared stat)
    Board(){ seed(); }
    // A little default heart so the canvas greets you with art, not a void.
    void seed(){
        static const char* art[] = {
            "................................",
            "................................",
            "................................",
            ".......XX.........XX.............",
            "......XXXX.......XXXX............",
            ".....XXXXXX.....XXXXXX...........",
            ".....XXXXXXX...XXXXXXX...........",
            ".....XXXXXXXX.XXXXXXXX...........",
            ".....XXXXXXXXXXXXXXXXX...........",
            "......XXXXXXXXXXXXXXX............",
            ".......XXXXXXXXXXXXX.............",
            "........XXXXXXXXXXX..............",
            ".........XXXXXXXXX...............",
            "..........XXXXXXX...............",
            "...........XXXXX................",
            "............XXX.................",
            ".............X..................",
        };
        int rows = (int)(sizeof(art)/sizeof(art[0]));
        for (int y = 0; y < rows && y < H; ++y)
            for (int x = 0; x < W && art[y][x]; ++x)
                if (art[y][x] == 'X') px[(std::size_t)(y * W + x)] = 5;   // pink heart
    }
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
        // Each cell. Empty cells get a faint raised fill + hairline so the grid
        // reads as a crafted surface; painted cells get the full colour with a
        // soft top highlight so they look like placed tiles. (All example flair.)
        auto cells = each_i(m.board_.px, [&](std::uint8_t idx, std::size_t i) {
            bool empty = (idx == 0);
            std::uint32_t c = kPalette[idx < kPalette.size() ? idx : 0];
            auto cell = box() | aspect(1.0f) | pointer | round(3)
                | tap(Paint{ (int)i })
                | detail::raw_css("transition", "background .1s, transform .06s")
                | key("p" + std::to_string(i));
            if (empty)
                cell = cell
                    | bg(rgba(0xffffff, 0.03f))
                    | border(1, rgba(0xffffff, 0.045f))
                    | on(Hover, bg(rgba(0xffffff, 0.10f)));
            else
                cell = cell | bg(c) | inset_light(0.28f) | on(Hover, scale(1.12f));
            return cell;
        });
        auto canvas = box(); canvas->kids = std::move(cells); finalize(*canvas);
        canvas = canvas | grid_cols(W) | gap(2) | w_full
            | pad(12) | round(16)
            | bg(rgba(0x0a0d16, 0.9f))
            | border(1, rgba(0xffffff, 0.06f))
            | elevation(4);

        // Palette: rounded chips that lift on hover; the active one glows.
        auto swatches = each_i(std::vector<std::uint32_t>(kPalette.begin() + 1, kPalette.end()),
            [&](std::uint32_t c, std::size_t k) {
                int idx = (int)k + 1;
                bool on = (idx == m.color);
                auto sw = box() | bg(c) | w(30) | h(30) | round(9) | pointer
                    | inset_light(0.25f)
                    | detail::raw_css("transition", "transform .12s")
                    | tap(Pick{ idx })
                    | hover_lift(2);
                if (on) sw = sw | ring(rgba(0xffffff, 0.95f), 2.5f) | scale(1.12f);
                return sw;
            });
        auto palette = box(); palette->kids = std::move(swatches); finalize(*palette);
        palette = palette | horizontal | detail::raw_css("flex-wrap", "wrap") | gap(9) | items_center;

        auto stat = text(std::to_string(m.board_.strokes) + " strokes")
            | fg(rgba(0xffffff, 0.85f)) | detail::raw_css("font-size", "12.5px") | semibold
            | pad_x(11) | pad_y(6) | round(999)
            | bg(rgba(0xffffff, 0.06f)) | border(1, rgba(0xffffff, 0.08f));

        auto clear = button("Clear", Clear{});

        return col(
            row(text("\xf0\x9f\x8e\xa8  waya/place")
                    | heading | gradient_text(0xb17cff, 0x4d9dff, 100)
                    | detail::raw_css("font-size", "22px") | bold,
                box() | grows,
                stat)
                | items_center | mb(2),
            text("Paint on a shared canvas \xe2\x80\x94 open a second tab and watch it sync live.")
                | fg(rgba(0xffffff, 0.5f)) | detail::raw_css("font-size", "14px") | mb(5),
            canvas,
            row(palette, box() | grows, clear)
                | items_center | gap(16) | mt(5),
            text("Server-rendered \xc2\xb7 real-time multiplayer \xc2\xb7 no client code \xc2\xb7 built with waya")
                | fg(rgba(0xffffff, 0.32f)) | detail::raw_css("font-size", "12px")
                | text_align(Justify::center) | mt(6) | w_full)
            | max_w(600) | mx_auto | pad(32)
            | detail::raw_css("min-height", "100vh")
            | detail::raw_css("justify-content", "center")
            // a soft radial glow behind everything — example flair, not core policy.
            | detail::raw_css("background",
                "radial-gradient(1200px 600px at 50% -10%, rgba(108,92,231,.16), transparent 60%),"
                "radial-gradient(900px 500px at 90% 110%, rgba(77,157,255,.12), transparent 55%)")
            | w_full;
    }

    static Meta meta(const Model&) {
        return { .title = "waya/place \u2014 collaborative pixel canvas",
                 .description = "A live, multiplayer pixel-art board rendered on the server with waya." };
    }
    // Analytics-ready; no <head> injection needed for the demo.
};

int main() { return live<App>({ .port = 8080, .page_bg = 0x080a12, .title = "waya/place" }); }
