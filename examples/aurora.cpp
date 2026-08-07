/// examples/aurora.cpp — AURORA: a modern SaaS landing page. Clean type scale,
/// crisp inline-SVG icons, restrained accents, a subtle gradient field, a real
/// nav bar, a hero with a working "star" pill, a feature grid, and a stats
/// strip. Designed to look like a shipped product, not a demo.
///
///   waya run aurora           # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Aurora {
    // ── a tight, deliberate palette ─────────────────────────────────────────
    static constexpr std::uint32_t bg      = 0x080b14;   // near-black page
    static constexpr std::uint32_t panel   = 0x0e1320;   // raised surface
    static constexpr std::uint32_t line    = 0x1e2636;   // hairline border
    static constexpr std::uint32_t ink      = 0xf2f5fa;   // headings
    static constexpr std::uint32_t body_c   = 0x9aa7bd;   // body text
    static constexpr std::uint32_t faint    = 0x5b667d;
    static constexpr std::uint32_t brand    = 0x6d7cff;   // single accent (indigo)
    static constexpr std::uint32_t brand_2  = 0x00d4ff;

    struct Model { long stars = 2417; bool starred = false; };
    struct Star {};
    using Msg = std::variant<Star>;

    static Model init() { return {}; }
    static Model update(Model m, Msg) {
        m.starred = !m.starred;
        m.stars  += m.starred ? 1 : -1;
        return m;
    }

    // hairline border helper
    static Mod stroke_border(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }
    static Mod card_bg() {
        return detail::raw_css("background",
            "linear-gradient(180deg,#0f1524,#0b101c)");
    }

    // a soft label above sections
    static NodeRef kicker(std::string t) {
        return text(std::move(t)) | fg(brand_2) | font(12) | weight(Weight::bold)
             | tracking_em(0.16f) | uppercase;
    }

    static NodeRef nav() {
        auto link = [](std::string t) {
            return text(std::move(t)) | fg(body_c) | font(14) | weight(Weight::medium)
                 | pointer | on(Hover, fg(ink)) | transition("color .15s ease");
        };
        auto logo = row(
            box(icon("home", 18) | fg(0xffffff))
                | square(30) | center | round(8)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)"),
            text("waya") | fg(ink) | font(17) | weight(Weight::bold) | tracking_em(-0.01f)
        ) | gap(10) | items_center;

        auto right = row(
            link("Features"), link("Docs"), link("Pricing"),
            row(text("Sign in") | fg(ink) | font(14) | weight(Weight::semibold))
                | pad_x(16) | pad_y(9) | round(9)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
                | pointer | on(Hover, brightness(110))
                | transition("filter .15s ease")
        ) | gap(28) | items_center;

        return row(logo, box() | grow(), right)
             | items_center | w_full | max_w(1120) | center_x
             | pad_x(28) | pad_y(20);
    }

    static NodeRef feature(std::string ico, std::string title_, std::string body_) {
        return col(
            box(icon(ico, 20) | fg(brand_2))
                | square(44) | center | round(11)
                | detail::raw_css("background", "rgba(109,124,255,.10)")
                | stroke_border(0x263050),
            text(title_) | fg(ink) | font(17) | weight(Weight::semibold),
            text(body_)  | fg(body_c) | font(14) | leading(1.65f)
        ) | gap(14) | pad(26) | round(16) | grow()
          | card_bg() | stroke_border()
          | on(Hover, detail::raw_css("border-color", "#2f3b57"))
          | transition("border-color .18s ease");
    }

    static NodeRef stat(std::string big, std::string label) {
        return col(
            text(std::move(big)) | fg(ink) | font(30) | weight(Weight::black)
                | tracking_em(-0.02f),
            text(std::move(label)) | fg(faint) | font(13) | weight(Weight::medium)
        ) | gap(4) | items_center;
    }

    static NodeRef view(const Model& m) {
        // ── hero ────────────────────────────────────────────────────────────
        auto badge_row = row(
            box() | circle(6) | detail::raw_css("background", "#00d4ff"),
            text("v1.0 is live") | fg(body_c) | font(13) | weight(Weight::medium),
            box() | w(1) | h(12) | detail::raw_css("background", "#2a3348"),
            row(text("changelog") | fg(brand_2) | font(13) | weight(Weight::semibold),
                icon("arrow-right", 13) | fg(brand_2)) | gap(5) | items_center
        ) | gap(12) | items_center | pad_x(14) | pad_y(8) | pill
          | detail::raw_css("background", "rgba(255,255,255,.03)")
          | stroke_border(0x232c40);

        auto title_ = col(
            text("The web framework") | fg(ink),
            row(text("that speaks ")  | fg(ink),
                text("C++")
                    | detail::raw_css("background", "linear-gradient(120deg,#8b96ff,#00d4ff)")
                    | detail::raw_css("-webkit-background-clip", "text")
                    | detail::raw_css("background-clip", "text")
                    | detail::raw_css("color", "transparent")) | items_baseline
        ) | gap(4) | items_center
          | font_fluid(40, 72) | weight(Weight::black)
          | detail::raw_css("letter-spacing", "-0.03em")
          | detail::raw_css("line-height", "1.05")
          | text_center;

        auto sub = text("Render UI on the server, stream only the diff. No JavaScript "
                        "to write, no build step, no client state to sync \u2014 just a "
                        "Model, a Msg, and a view function.")
                 | fg(body_c) | font_fluid(16, 19) | leading(1.7f)
                 | max_w(600) | text_center;

        auto primary_cta = row(
            text("Start building") | fg(0xffffff) | font(15) | weight(Weight::semibold),
            icon("arrow-right", 16) | fg(0xffffff)
        ) | gap(9) | items_center | pad_x(22) | pad_y(13) | round(11)
          | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
          | detail::raw_css("box-shadow", "0 8px 24px -8px rgba(109,124,255,.7)")
          | pointer | on(Hover, detail::raw_css("transform", "translateY(-1px)"))
          | transition("transform .15s ease, box-shadow .15s ease");

        auto star_cta = row(
            icon("star", 16) | fg(m.starred ? 0xffd24a : body_c),
            text("Star") | fg(ink) | font(15) | weight(Weight::semibold),
            box() | w(1) | h(16) | detail::raw_css("background", "#2a3348"),
            text(std::to_string(m.stars)) | fg(body_c) | font(14) | weight(Weight::semibold)
                | tabular_nums
        ) | gap(11) | items_center | pad_x(18) | pad_y(13) | round(11)
          | detail::raw_css("background", "rgba(255,255,255,.03)")
          | stroke_border(0x2a3348)
          | pointer | on(Hover, detail::raw_css("border-color", "#3a4560"))
          | tap(Star{}) | transition("border-color .15s ease");

        auto hero = col(
            badge_row, title_, sub,
            row(primary_cta, star_cta) | gap(14) | items_center | wrap | justify_center
        ) | gap(28) | items_center | pad_x(28)
          | detail::raw_css("padding-top", "40px")
          | detail::raw_css("padding-bottom", "64px")
          | max_w(820) | center_x;

        // ── feature grid ────────────────────────────────────────────────────
        auto features = col(
            col(kicker("Why waya"),
                text("Everything you need, nothing you don't")
                    | fg(ink) | font_fluid(26, 36) | weight(Weight::bold)
                    | detail::raw_css("letter-spacing", "-0.02em") | text_center)
                | gap(12) | items_center,
            row(
                feature("bell",     "Real-time by default",
                        "State lives on the server. Every change streams to the client as a minimal patch over one socket."),
                feature("settings", "Typed end to end",
                        "Your whole app is a Model, a Msg variant, and pure functions. If it compiles, the wiring is correct."),
                feature("heart",    "Beautiful, composably",
                        "A vocabulary of layout and style mods. Gradients, glass and motion are each a single word.")
            ) | gap(18) | wrap
        ) | gap(40) | items_center | pad_x(28) | pad_y(56) | max_w(1120) | center_x;

        // ── stats strip ─────────────────────────────────────────────────────
        auto stats = row(
            stat("0kb", "JS shipped"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("<1ms", "diff time"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("100%", "server-rendered"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("C++26", "one language")
        ) | gap(36) | items_center | justify_center | wrap
          | pad(32) | round(18) | max_w(880) | center_x
          | card_bg() | stroke_border();

        auto footer = row(
            text("\u00a9 2025 waya") | fg(faint) | font(13),
            box() | grow(),
            text("server-rendered \u00b7 no JS \u00b7 C++26") | fg(faint) | font(13)
        ) | items_center | w_full | max_w(1120) | center_x | pad_x(28) | pad_y(28);

        return col(
            nav(),
            hero,
            features,
            box(stats) | pad_x(28) | w_full,
            box() | h(48),
            box() | h(1) | w_full | detail::raw_css("background", "#141b29"),
            footer
        ) | min_h(100_vh)
          | detail::raw_css("background",
              "radial-gradient(1200px 600px at 50% -10%, rgba(109,124,255,.18), transparent 60%),"
              "radial-gradient(900px 500px at 85% 20%, rgba(0,212,255,.08), transparent 55%), #080b14")
          | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "waya \u00b7 the C++ web framework";
        mt.description = "Render UI on the server, stream only the diff. No JavaScript, no build step.";
        return mt;
    }
};

int main() {
    return live<Aurora>({ .port = 8080, .page_bg = 0x080b14, .title = "waya" });
}
