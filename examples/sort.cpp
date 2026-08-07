/// examples/sort.cpp — SORT: watch sorting algorithms run, step by step, as
/// animated bars. Pick bubble / insertion / selection / quicksort, shuffle, and
/// hit play \u2014 the algorithm's state machine advances on a server tick, colouring
/// the bars it's comparing (amber) and swapping (rose). Comparison + swap
/// counters keep score. The whole thing is a std::vector<int> plus a tiny
/// program-counter, evolved in pure C++.
///
///   waya run sort             # then open the printed URL

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

struct Sort {
    static constexpr int N = 48;

    static constexpr std::uint32_t ink   = 0xeef2f8;
    static constexpr std::uint32_t body_c= 0x8b98af;
    static constexpr std::uint32_t faint = 0x556074;
    static constexpr std::uint32_t line  = 0x1c2436;
    static constexpr std::uint32_t brand = 0x6d7cff;
    static constexpr std::uint32_t cmp_c = 0xf6b74a;   // comparing
    static constexpr std::uint32_t swp_c = 0xff6b81;   // swapping
    static constexpr std::uint32_t done_c= 0x34e0a1;   // sorted

    enum Algo { Bubble, Insertion, Selection, Quick };

    struct Model {
        std::vector<int> a;
        int algo = Bubble;
        bool running = false;
        long cmps = 0, swaps = 0;
        int  speed = 7;
        bool done = false;
        // algorithm state (a resumable step machine)
        int i = 0, j = 0, k = 0, minidx = 0;
        std::vector<std::pair<int,int>> stack;   // quicksort ranges to process
        int a_hi = -1, b_hi = -1;                // highlighted indices this frame
    };

    struct Play {}; struct Shuffle {}; struct Pick { int algo; }; struct Speed { int d; }; struct Tick {}; struct Reset {};
    using Msg = std::variant<Play, Shuffle, Pick, Speed, Tick, Reset>;

    static void shuffle(Model& m) {
        m.a.resize(N);
        for (int i = 0; i < N; ++i) m.a[i] = i + 1;
        for (int i = N - 1; i > 0; --i) std::swap(m.a[i], m.a[std::rand() % (i + 1)]);
        m.cmps = m.swaps = 0; m.done = false; m.running = false;
        m.i = 0; m.j = 0; m.k = 0; m.minidx = 0; m.a_hi = m.b_hi = -1;
        m.stack.clear(); m.stack.push_back({0, N - 1});
    }

    static Model init() { Model m; shuffle(m); return m; }

    // Advance the chosen algorithm by ONE comparison/swap step. Each returns
    // with a_hi/b_hi set to the touched indices, so the view can colour them.
    static void step(Model& m) {
        if (m.done) return;
        m.a_hi = m.b_hi = -1;
        switch (m.algo) {
            case Bubble: {
                if (m.i >= N - 1) { m.done = true; m.running = false; return; }
                m.a_hi = m.j; m.b_hi = m.j + 1; m.cmps++;
                if (m.a[m.j] > m.a[m.j + 1]) { std::swap(m.a[m.j], m.a[m.j + 1]); m.swaps++; }
                if (++m.j >= N - 1 - m.i) { m.j = 0; m.i++; }
                break;
            }
            case Insertion: {
                if (m.i >= N) { m.done = true; m.running = false; return; }
                if (m.i == 0) { m.i = 1; m.j = 1; }
                if (m.j > 0) { m.a_hi = m.j; m.b_hi = m.j - 1; m.cmps++;
                    if (m.a[m.j] < m.a[m.j - 1]) { std::swap(m.a[m.j], m.a[m.j - 1]); m.swaps++; m.j--; }
                    else { m.i++; m.j = m.i; }
                } else { m.i++; m.j = m.i; }
                if (m.i >= N) { m.done = true; m.running = false; }
                break;
            }
            case Selection: {
                if (m.i >= N - 1) { m.done = true; m.running = false; return; }
                if (m.j == 0 && m.k == 0) { m.minidx = m.i; m.j = m.i + 1; }
                if (m.j < N) { m.a_hi = m.j; m.b_hi = m.minidx; m.cmps++;
                    if (m.a[m.j] < m.a[m.minidx]) m.minidx = m.j;
                    m.j++;
                } else {
                    if (m.minidx != m.i) { std::swap(m.a[m.i], m.a[m.minidx]); m.swaps++; }
                    m.i++; m.j = 0; m.k = 0;
                }
                break;
            }
            case Quick: {
                // iterative quicksort with a partition state machine
                if (m.k == 0) {                        // need a new range
                    while (!m.stack.empty()) {
                        auto [lo, hi] = m.stack.back(); m.stack.pop_back();
                        if (lo < hi) { m.i = lo; m.j = lo; m.k = 1; m.minidx = hi; // pivot = a[hi]
                                       m.stack.push_back({lo, hi}); break; }
                    }
                    if (m.k == 0) { m.done = true; m.running = false; return; }
                }
                // partition step: m.i = scan, m.j = store, range on stack top
                auto [lo, hi] = m.stack.back();
                if (m.i < hi) {
                    m.a_hi = m.i; m.b_hi = hi; m.cmps++;
                    if (m.a[m.i] < m.a[hi]) { std::swap(m.a[m.i], m.a[m.j]); if (m.i != m.j) m.swaps++; m.j++; }
                    m.i++;
                } else {
                    std::swap(m.a[m.j], m.a[hi]); m.swaps++;   // place pivot
                    int p = m.j;
                    m.stack.pop_back();
                    m.stack.push_back({lo, p - 1});
                    m.stack.push_back({p + 1, hi});
                    m.k = 0;
                }
                break;
            }
        }
    }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Play)    { if (!m.done) m.running = !m.running; },
            [&](Shuffle) { shuffle(m); },
            [&](Reset)   { shuffle(m); },
            [&](Pick p)  { m.algo = p.algo; shuffle(m); },
            [&](Speed s) { m.speed += s.d; if (m.speed < 1) m.speed = 1; if (m.speed > 12) m.speed = 12; },
            [&](Tick)    { for (int s = 0; s < 1 + m.speed / 3; ++s) step(m); },  // batch steps for speed
        }, msg);
        return m;
    }

    static Sub<Msg> subscribe(const Model& m) {
        return (m.running && !m.done) ? Sub<Msg>::every(std::chrono::milliseconds(24), Tick{}) : Sub<Msg>::none();
    }

    static const char* algo_name(int a) {
        static const char* n[] = { "Bubble", "Insertion", "Selection", "Quicksort" }; return n[a];
    }

    static Mod bord(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }

    static NodeRef bars(const Model& m) {
        std::string svg;
        int W = 960, H = 360;
        float bw = (float)W / N;
        svg.reserve(N * 90 + 64);
        svg += "<svg viewBox='0 0 " + std::to_string(W) + " " + std::to_string(H) +
               "' preserveAspectRatio='none' style='width:100%;height:320px;display:block'>";
        for (int i = 0; i < N; ++i) {
            float h = (float)m.a[i] / N * (H - 6);
            float x = i * bw, y = H - h;
            const char* fill = "#3a4a6a";
            if (m.done) fill = "#34e0a1";
            else if (i == m.a_hi) fill = "#ff6b81";
            else if (i == m.b_hi) fill = "#f6b74a";
            svg += "<rect x='" + std::to_string((int)(x + 1)) + "' y='" + std::to_string((int)y) +
                   "' width='" + std::to_string((int)(bw - 2)) + "' height='" + std::to_string((int)h) +
                   "' rx='2' fill='" + fill + "'/>";
        }
        svg += "</svg>";
        return box(markup(std::move(svg)) | detail::raw_css("line-height", "0"))
            | w_full | round(14) | detail::raw_css("overflow", "hidden") | bord(0x243049)
            | detail::raw_css("background", "linear-gradient(180deg,#0c111d,#0a0e18)")
            | detail::raw_css("box-shadow", "0 30px 80px -30px rgba(0,0,0,.8)")
            | pad(6);
    }

    static NodeRef chip(const Model& m, int a) {
        bool sel = m.algo == a;
        auto n = text(algo_name(a)) | font(13) | weight(Weight::semibold)
               | pad_x(14) | pad_y(9) | round(9) | pointer | tap(Pick{ a })
               | transition("background-color .15s ease, border-color .15s ease");
        if (sel) n = n | fg(0xffffff)
                    | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
                    | detail::raw_css("box-shadow", "0 6px 16px -6px rgba(109,124,255,.7)");
        else n = n | fg(ink) | detail::raw_css("background", "rgba(255,255,255,.04)") | bord(0x2a3446)
                  | on(Hover, detail::raw_css("border-color", "#3a4560"));
        return n;
    }

    static NodeRef btn(std::string label, Msg msg) {
        return text(std::move(label)) | font(13) | weight(Weight::semibold) | fg(ink)
             | pad_x(14) | pad_y(9) | round(9) | pointer | tap(msg)
             | detail::raw_css("background", "rgba(255,255,255,.04)") | bord(0x2a3446)
             | on(Hover, detail::raw_css("border-color", "#3a4560"))
             | transition("border-color .15s ease");
    }

    static NodeRef stat(std::string k, std::string v, std::uint32_t c) {
        return col(text(k) | fg(faint) | font(11) | uppercase | tracking_em(0.10f) | weight(Weight::semibold),
                   text(v) | fg(c) | font(22) | weight(Weight::black) | tabular_nums) | gap(2);
    }

    static NodeRef view(const Model& m) {
        auto title = row(
            box(text("\u25C8") | font(18) | fg(0xffffff)) | square(34) | center | round(9)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)") | glow(brand, 14),
            col(text("Sort") | fg(ink) | font(20) | weight(Weight::black) | detail::raw_css("letter-spacing","-0.02em"),
                text("sorting algorithm visualiser") | fg(faint) | font(12)) | gap(1)
        ) | gap(12) | items_center;

        auto algos = row(chip(m, Bubble), chip(m, Insertion), chip(m, Selection), chip(m, Quick))
                   | gap(8) | wrap;

        auto controls = row(
            text(m.running ? "\u2759\u2759 Pause" : (m.done ? "\u21BB Replay" : "\u25B6 Play"))
                | font(13) | weight(Weight::semibold) | fg(0xffffff)
                | pad_x(16) | pad_y(9) | round(9) | pointer | tap(m.done ? Msg{Shuffle{}} : Msg{Play{}})
                | detail::raw_css("background", "linear-gradient(135deg,#34e0a1,#22b98a)")
                | on(Hover, brightness(110)),
            btn("Shuffle", Shuffle{}),
            box() | grow(),
            row(btn("\u2212", Speed{-1}),
                col(text("SPEED") | fg(faint) | font(10) | tracking_em(0.10f) | weight(Weight::semibold),
                    text(std::to_string(m.speed) + "x") | fg(ink) | font(13) | weight(Weight::bold) | tabular_nums) | gap(0) | items_center,
                btn("+", Speed{+1})) | gap(8) | items_center
        ) | gap(9) | items_center | wrap | w_full;

        auto stats = row(
            stat("Algorithm", algo_name(m.algo), brand),
            box() | w(1) | h(34) | detail::raw_css("background", "#1c2436"),
            stat("Comparisons", std::to_string(m.cmps), cmp_c),
            box() | w(1) | h(34) | detail::raw_css("background", "#1c2436"),
            stat("Swaps", std::to_string(m.swaps), swp_c),
            box() | grow(),
            row(box() | circle(7) | detail::raw_css("background", m.done ? "#34e0a1" : (m.running ? "#f6b74a" : "#556074"))
                    | ((m.running && !m.done) ? breathe() : noop),
                text(m.done ? "sorted" : (m.running ? "running" : "ready"))
                    | fg(m.done ? done_c : (m.running ? cmp_c : body_c)) | font(13) | weight(Weight::semibold))
                | gap(8) | items_center
        ) | gap(20) | items_center | wrap | pad(18) | round(14)
          | detail::raw_css("background", "linear-gradient(180deg,#0d1322,#0a0f1b)") | bord();

        auto legend = row(
            legend_dot(0xf6b74a, "comparing"), legend_dot(0xff6b81, "swapping"),
            legend_dot(0x34e0a1, "sorted"), legend_dot(0x3a4a6a, "unsorted")
        ) | gap(18) | wrap | justify_center;

        return col(
            row(title, box() | grow(), algos) | items_center | gap(16) | wrap | w_full,
            controls, bars(m), stats, legend
        ) | gap(16) | pad(28) | max_w(1040) | center_x | min_h(100_vh)
          | detail::raw_css("background",
              "radial-gradient(1100px 560px at 50% -5%, rgba(109,124,255,.12), transparent 55%),"
              "radial-gradient(800px 500px at 85% 20%, rgba(52,224,161,.08), transparent 55%), #070a12")
          | as_main;
    }

    static NodeRef legend_dot(std::uint32_t c, std::string label) {
        return row(box() | square(11) | round(3) | detail::raw_css("background", detail::hexstr(c)),
                   text(std::move(label)) | fg(body_c) | font(12)) | gap(7) | items_center;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Sort \u00b7 algorithm visualiser";
        mt.description = "Watch sorting algorithms run step by step, animated server-side in C++ with waya.";
        return mt;
    }
};

int main() { return live<Sort>({ .port = 8080, .page_bg = 0x070a12, .title = "Sort" }); }
