/// examples/aurora.cpp — AURORA: a living hero landing page. A drifting aurora
/// backdrop, a gradient headline whose hue rotates, floating glass feature
/// cards, a live "stars" counter that ticks up on its own, and a call-to-action
/// that glows on hover. Every effect is a single named mod; the whole page is
/// server-rendered and only the counter's delta streams over the wire.
///
///   waya run aurora           # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Aurora {
    struct Model {
        long  tick    = 0;
        long  stars   = 24713;   // the live "GitHub stars" vanity metric
        bool  starred = false;
    };

    struct Tick {}; struct Star {};
    using Msg = std::variant<Tick, Star>;

    static Model init() { return {}; }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model, Cmd<Msg>> {
                m.tick++;
                // a gentle organic drift upward, ~ every 1.2s
                if (m.tick % 2 == 0) m.stars += 1 + (m.tick * 7) % 3;
                return { m, Cmd<Msg>::none() };
            },
            [&](Star) -> std::pair<Model, Cmd<Msg>> {
                m.starred = !m.starred;
                m.stars  += m.starred ? 1 : -1;
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::every(std::chrono::milliseconds(600), Tick{});
    }

    // A small "pill" chip used above the headline.
    static NodeRef eyebrow() {
        return row(
            box() | circle(7) | bg(0x34d399) | breathe(),
            text("now shipping v1.0") | fg(ink) | font(13) | semibold | tracking_em(0.02f)
        ) | gap(9) | items_center | pad_x(14) | pad_y(8) | pill
          | frost(10) | hairline(0xffffff, 0.14f);
    }

    // A frosted feature card that lifts and glows on hover.
    static NodeRef feature(std::string glyph, std::string title_, std::string body_,
                           std::uint32_t hue) {
        return col(
            box(text(glyph) | font(26))
                | square(52) | round(15) | center
                | gradient(hue, 0x0b1020, 145) | ring(hue, 1) | glow(hue, 18),
            text(title_) | fg(ink) | font(18) | semibold,
            text(body_)  | fg(muted) | body
        ) | gap(14) | pad(24) | round(20) | w(300)
          | frost(16) | hairline(0xffffff, 0.10f)
          | hover_lift(4) | hover_glow(hue, 40)
          | transition("transform .2s cubic-bezier(.2,.7,.2,1), box-shadow .2s ease")
          | fade_up();
    }

    static NodeRef view(const Model& m) {
        auto headline = col(
            text("Build living interfaces") | font_fluid(34, 76) | weight(Weight::black)
                | aurora_text(0xa78bfa, 0x22d3ee, 0xf472b6) | leading(1.02f)
                | text_center,
            text("in pure C++") | font_fluid(34, 76) | weight(Weight::black)
                | fg(ink) | leading(1.02f) | text_center
        ) | gap(2) | items_center;

        auto subhead =
            text("waya renders your UI on the server and streams only what "
                 "changed — no JavaScript to write, no build step, no client "
                 "state to sync. Just a Model, a Msg, and a view.")
            | fg(muted) | font_fluid(15, 19) | leading(1.7f)
            | max_w(560) | text_center;

        auto star_btn =
            row(
                text(m.starred ? "\u2605" : "\u2606") | font(18)
                    | fg(m.starred ? 0xfbbf24 : ink),
                text("Star") | semibold,
                box(text(std::to_string(m.stars)) | tabular_nums | semibold)
                    | pad_x(10) | pad_y(3) | pill | tint(0xffffff, 0.10f) | fg(ink)
            ) | gap(10) | items_center
              | pad_x(20) | pad_y(13) | round(12)
              | frost(14) | hairline(0xffffff, 0.16f) | fg(ink)
              | interactive() | tap(Star{});

        auto cta =
            row(
                text("Get started") | semibold,
                text("\u2192") | font(17)
            ) | gap(9) | items_center
              | pad_x(24) | pad_y(13) | round(12)
              | gradient(0x8b5cf6, 0x6366f1, 135) | fg(0xffffff)
              | glow(0x8b5cf6, 30) | interactive() | hover_glow(0x8b5cf6, 46)
              | tap(Star{});   // demo: any tap works

        auto actions = row(cta, star_btn) | gap(14) | items_center | wrap
                     | justify_center | fade_up(600);

        auto features = row(
            feature("\u26A1", "Instant",   "Deltas over a socket. A keystroke round-trips in a frame.", 0x22d3ee),
            feature("\u25C8", "Typed",     "Model, Msg, view — the whole app is one std::variant.",     0xa78bfa),
            feature("\u2726", "Beautiful", "Gradients, glass, motion — every effect is one word.",       0xf472b6)
        ) | gap(20) | wrap | justify_center | max_w(960);

        auto hero = col(
            eyebrow(),
            headline,
            subhead,
            actions,
            features
        ) | gap(30) | items_center | pad_x(24) | pad_y(80) | max_w(1040) | center_x;

        auto footer = row(
            text("waya") | fg(ink) | semibold | tracking_em(0.06f),
            box() | grow(),
            text("server-rendered \u00b7 no JS \u00b7 C++26") | fg(faint) | font(13)
        ) | items_center | w_full | max_w(1040) | center_x | pad_x(24) | pad_y(24);

        return col(hero, box() | grow(), footer)
             | min_h(100_vh)
             | aurora(0x1e1b4b, 0x0b1020, 0x0e2a3a, 26)
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "waya \u00b7 Aurora";
        mt.description = "A living hero landing page, server-rendered in pure C++ with waya.";
        return mt;
    }
};

int main() {
    return live<Aurora>({ .port = 8080, .title = "waya \u00b7 aurora" });
}
