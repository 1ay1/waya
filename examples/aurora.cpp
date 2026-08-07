/// examples/aurora.cpp — AURORA: a premium SaaS landing page. A living aurora
/// backdrop that drifts server-side, a hero with a real code-window mockup,
/// staggered fade-up entrances, a gradient-bordered CTA, a logo cloud, and a
/// feature grid with depth. Every effect is a mod; the drift streams as deltas.
///
///   waya run aurora           # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <cmath>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Aurora {
    // ── palette ─────────────────────────────────────────────────────────────
    static constexpr std::uint32_t bg      = 0x060912;
    static constexpr std::uint32_t line    = 0x1b2233;
    static constexpr std::uint32_t ink     = 0xf4f7fc;
    static constexpr std::uint32_t body_c  = 0x9aa7bd;
    static constexpr std::uint32_t faint   = 0x596479;
    static constexpr std::uint32_t brand   = 0x6d7cff;
    static constexpr std::uint32_t brand2  = 0x00d4ff;

    struct Model { long tick = 0; long stars = 2417; bool starred = false; };
    struct Tick {}; struct Star {};
    using Msg = std::variant<Tick, Star>;

    static Model init() { return {}; }
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Tick) -> std::pair<Model,Cmd<Msg>> { m.tick++; return { m, Cmd<Msg>::none() }; },
            [&](Star) -> std::pair<Model,Cmd<Msg>> {
                m.starred = !m.starred; m.stars += m.starred ? 1 : -1;
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }
    // Slow drift for the aurora backdrop — a gentle 2s cadence keeps it alive
    // without churning the wire.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::every(std::chrono::milliseconds(2000), Tick{});
    }

    static Mod bord(std::uint32_t c = line) {
        return detail::raw_css("border", "1px solid " + detail::hexstr(c));
    }
    static Mod card() {
        return detail::raw_css("background", "linear-gradient(180deg,#0d1322,#0a0f1b)");
    }
    static NodeRef kicker(std::string t) {
        return text(std::move(t)) | fg(brand2) | font(12) | weight(Weight::bold)
             | tracking_em(0.18f) | uppercase;
    }

    // ── the drifting aurora backdrop: three soft radial blobs whose centres
    // wander on the tick. Rendered as one absolutely-positioned layer behind the
    // content, so it never affects layout.
    static NodeRef backdrop(const Model& m) {
        auto num = [](float f){ char b[16]; std::snprintf(b, sizeof b, "%.1f", f); return std::string(b); };
        float t = m.tick * 0.5f;
        auto blob = [&](float cx0, float cy0, float ax, float ay, const char* rgba, float r) {
            float x = cx0 + ax * std::cos(t * 0.6f + cx0);
            float y = cy0 + ay * std::sin(t * 0.5f + cy0);
            return "radial-gradient(" + num(r) + "px " + num(r*0.7f) + "px at " +
                   num(x) + "% " + num(y) + "%, " + rgba + ", transparent 60%)";
        };
        std::string g =
            blob(30, 10, 12, 8, "rgba(109,124,255,.22)", 900) + "," +
            blob(78, 22, 10, 10, "rgba(0,212,255,.14)", 800) + "," +
            blob(55, 80, 14, 8, "rgba(160,110,255,.12)", 850);
        return box() | detail::raw_css("position", "absolute")
             | detail::raw_css("inset", "0")
             | detail::raw_css("background", g)
             | detail::raw_css("transition", "background 1.8s ease-in-out")
             | detail::raw_css("pointer-events", "none")
             | detail::raw_css("z-index", "0");
    }

    static NodeRef nav() {
        auto link = [](std::string t) {
            return text(std::move(t)) | fg(body_c) | font(14) | weight(Weight::medium)
                 | pointer | on(Hover, fg(ink)) | transition("color .15s ease");
        };
        auto logo = row(
            box(text("\u25C8") | font(16) | fg(0xffffff))
                | square(30) | center | round(9)
                | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)")
                | glow(brand, 14),
            text("waya") | fg(ink) | font(17) | weight(Weight::bold) | tracking_em(-0.01f)
        ) | gap(10) | items_center;
        auto signin = row(text("Sign in") | fg(0xffffff) | font(14) | weight(Weight::semibold))
            | pad_x(16) | pad_y(9) | round(9)
            | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
            | pointer | on(Hover, brightness(112)) | transition("filter .15s ease");
        return row(logo, box() | grow(),
                   row(link("Features"), link("Docs"), link("Pricing"), signin)
                       | gap(28) | items_center)
             | items_center | w_full | max_w(1140) | center_x | pad_x(28) | pad_y(20);
    }

    // A tiny syntax-coloured code window — the "show don't tell" hero prop.
    static NodeRef code_window() {
        auto ln = [](std::vector<NodeRef> parts) {
            return row_(std::move(parts)) | detail::raw_css("white-space", "pre");
        };
        auto kw  = [](std::string s){ return text(std::move(s)) | fg(0xc4a7ff); };
        auto fn  = [](std::string s){ return text(std::move(s)) | fg(0x7dd3fc); };
        auto str = [](std::string s){ return text(std::move(s)) | fg(0x86efac); };
        auto pl  = [](std::string s){ return text(std::move(s)) | fg(0x93a4bd); };
        auto dot = [](std::uint32_t c){ return box() | square(11) | circle(6)
                     | detail::raw_css("background", detail::hexstr(c)); };

        auto titlebar = row(dot(0xff5f57), dot(0xfebc2e), dot(0x28c840),
                            box() | grow(),
                            text("view.cpp") | fg(faint) | font(12))
            | gap(8) | items_center | pad_x(14) | pad_y(11)
            | detail::raw_css("border-bottom", "1px solid #1b2233");

        auto code = col(
            ln({ kw("col"), pl("(") }),
            ln({ pl("  "), fn("text"), pl("("), str("\"Hello\""), pl(") | "), fn("font"), pl("(48) | bold,") }),
            ln({ pl("  "), fn("button"), pl("("), str("\"Star\""), pl(", Star{})") }),
            ln({ pl("    | "), fn("bg"), pl("(brand) | "), fn("glow"), pl("(brand)") }),
            ln({ pl(") | "), fn("gap"), pl("(16) | "), fn("center"), pl(";") })
        ) | gap(6) | pad(18) | font(13.5f)
          | detail::raw_css("font-family", "ui-monospace,SFMono-Regular,Menlo,monospace")
          | leading(1.5f);

        return col(titlebar, code)
             | w_full | max_w(440) | round(14) | detail::raw_css("overflow", "hidden")
             | card() | bord(0x243049)
             | detail::raw_css("box-shadow", "0 30px 80px -30px rgba(0,0,0,.8)")
             | float_() | fade_up(700);
    }

    static NodeRef feature(std::string glyph, std::uint32_t hue, std::string title_,
                           std::string body_, int delay_ms) {
        return col(
            box(text(std::move(glyph)) | font(20) | fg(hue))
                | square(46) | center | round(12)
                | detail::raw_css("background",
                    "radial-gradient(circle at 30% 30%," + detail::hexstr(hue) + "22, transparent 70%)")
                | ring(hue, 1),
            text(title_) | fg(ink) | font(17) | weight(Weight::semibold),
            text(body_)  | fg(body_c) | font(14) | leading(1.65f)
        ) | gap(14) | pad(26) | round(16) | grow() | card() | bord()
          | on(Hover, detail::raw_css("border-color", "#31405f"))
          | on(Hover, detail::raw_css("transform", "translateY(-3px)"))
          | transition("border-color .2s ease, transform .2s ease")
          | fade_up(600) | delay(delay_ms);
    }

    static NodeRef stat(std::string big, std::string label) {
        return col(
            text(std::move(big)) | font(30) | weight(Weight::black) | tracking_em(-0.02f)
                | detail::raw_css("background", "linear-gradient(135deg,#c7ceff,#7fe6ff)")
                | detail::raw_css("-webkit-background-clip", "text")
                | detail::raw_css("background-clip", "text")
                | detail::raw_css("color", "transparent"),
            text(std::move(label)) | fg(faint) | font(13) | weight(Weight::medium)
        ) | gap(5) | items_center;
    }

    static NodeRef view(const Model& m) {
        auto badge = row(
            box() | circle(6) | detail::raw_css("background", "#00d4ff") | glow(brand2, 8),
            text("v1.0 is live") | fg(body_c) | font(13) | weight(Weight::medium),
            box() | w(1) | h(12) | detail::raw_css("background", "#2a3348"),
            row(text("changelog") | fg(brand2) | font(13) | weight(Weight::semibold),
                text("\u2192") | fg(brand2) | font(13)) | gap(5) | items_center
        ) | gap(12) | items_center | pad_x(14) | pad_y(8) | pill
          | detail::raw_css("background", "rgba(255,255,255,.03)") | bord(0x232c40)
          | fade_up(500);

        auto headline = col(
            text("The web framework") | fg(ink),
            row(text("that speaks ") | fg(ink),
                text("C++")
                    | detail::raw_css("background", "linear-gradient(120deg,#9aa4ff,#00d4ff)")
                    | detail::raw_css("-webkit-background-clip", "text")
                    | detail::raw_css("background-clip", "text")
                    | detail::raw_css("color", "transparent")) | items_baseline
        ) | gap(4) | items_center | font_fluid(40, 72) | weight(Weight::black)
          | detail::raw_css("letter-spacing", "-0.03em")
          | detail::raw_css("line-height", "1.05") | text_center
          | fade_up(600) | delay(80);

        auto sub = text("Render UI on the server, stream only the diff. No JavaScript to "
                        "write, no build step, no client state to sync \u2014 just a Model, "
                        "a Msg, and a view function.")
                 | fg(body_c) | font_fluid(16, 19) | leading(1.7f) | max_w(600) | text_center
                 | fade_up(600) | delay(160);

        auto primary = row(
            text("Start building") | fg(0xffffff) | font(15) | weight(Weight::semibold),
            text("\u2192") | fg(0xffffff) | font(16)
        ) | gap(9) | items_center | pad_x(22) | pad_y(13) | round(11)
          | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#5b6cff)")
          | detail::raw_css("box-shadow", "0 10px 30px -8px rgba(109,124,255,.75)")
          | pointer | on(Hover, detail::raw_css("transform", "translateY(-2px)"))
          | on(Hover, detail::raw_css("box-shadow", "0 16px 40px -10px rgba(109,124,255,.9)"))
          | transition("transform .15s ease, box-shadow .15s ease");

        auto star = row(
            text(m.starred ? "\u2605" : "\u2606") | font(16) | fg(m.starred ? 0xffd24a : body_c),
            text("Star") | fg(ink) | font(15) | weight(Weight::semibold),
            box() | w(1) | h(16) | detail::raw_css("background", "#2a3348"),
            text(std::to_string(m.stars)) | fg(body_c) | font(14) | weight(Weight::semibold) | tabular_nums
        ) | gap(11) | items_center | pad_x(18) | pad_y(13) | round(11)
          | detail::raw_css("background", "rgba(255,255,255,.03)") | bord(0x2a3348)
          | pointer | on(Hover, detail::raw_css("border-color", "#3a4560"))
          | tap(Star{}) | transition("border-color .15s ease");

        auto actions = row(primary, star) | gap(14) | items_center | wrap | justify_center
                     | fade_up(600) | delay(240);

        auto hero = col(badge, headline, sub, actions, code_window())
            | gap(28) | items_center | pad_x(28)
            | detail::raw_css("padding-top", "48px")
            | detail::raw_css("padding-bottom", "72px")
            | max_w(880) | center_x;

        // logo cloud
        auto lc = [](std::string s){ return text(std::move(s)) | fg(0x46536b) | font(17)
                     | weight(Weight::bold) | tracking_em(0.02f); };
        auto logos = col(
            text("Trusted by teams shipping in production") | fg(faint) | font(12)
                | weight(Weight::semibold) | tracking_em(0.14f) | uppercase | text_center,
            row(lc("Vercel"), lc("Linear"), lc("Raycast"), lc("Supabase"), lc("Retool"))
                | gap(40) | wrap | justify_center | items_center
        ) | gap(20) | items_center | pad_x(28) | pad_y(20) | max_w(1000) | center_x;

        auto features = col(
            col(kicker("Why waya"),
                text("Everything you need, nothing you don't")
                    | fg(ink) | font_fluid(26, 38) | weight(Weight::bold)
                    | detail::raw_css("letter-spacing", "-0.02em") | text_center)
                | gap(12) | items_center,
            row(
                feature("\u26A1", brand2, "Real-time by default",
                        "State lives on the server. Every change streams to the client as a minimal patch over one socket.", 0),
                feature("\u25C8", brand, "Typed end to end",
                        "Your whole app is a Model, a Msg variant, and pure functions. If it compiles, the wiring is correct.", 120),
                feature("\u2726", 0xa06eff, "Beautiful, composably",
                        "A vocabulary of layout and style mods. Gradients, glass and motion are each a single word.", 240)
            ) | gap(18) | wrap
        ) | gap(40) | items_center | pad_x(28) | pad_y(64) | max_w(1140) | center_x;

        auto stats = row(
            stat("0kb", "JS shipped"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("<1ms", "diff time"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("100%", "server-rendered"),
            box() | w(1) | h(40) | detail::raw_css("background", "#1c2436"),
            stat("C++26", "one language")
        ) | gap(36) | items_center | justify_center | wrap
          | pad(32) | round(18) | max_w(900) | center_x | card() | bord();

        // final CTA band with a gradient border
        auto cta = col(
            text("Ready to build?") | fg(ink) | font_fluid(24, 34) | weight(Weight::black)
                | detail::raw_css("letter-spacing", "-0.02em"),
            text("Scaffold an app in one command and see your first paint in seconds.")
                | fg(body_c) | font(15) | leading(1.6f) | text_center | max_w(460),
            row(text("$ waya new my-app") | fg(0x86efac) | font(14)
                    | detail::raw_css("font-family", "ui-monospace,Menlo,monospace"))
                | pad_x(18) | pad_y(12) | round(10)
                | detail::raw_css("background", "rgba(0,0,0,.35)") | bord(0x243049)
        ) | gap(18) | items_center | pad(40) | round(20) | max_w(720) | center_x
          | detail::raw_css("background", "linear-gradient(180deg,#0e1424,#0a0f1b)")
          | gradient_border(brand, brand2, 1)
          | detail::raw_css("box-shadow", "0 30px 90px -40px rgba(109,124,255,.6)");

        auto footer = row(
            text("\u25C8 waya") | fg(body_c) | font(14) | weight(Weight::bold),
            box() | grow(),
            text("server-rendered \u00b7 no JS \u00b7 C++26") | fg(faint) | font(13)
        ) | items_center | w_full | max_w(1140) | center_x | pad_x(28) | pad_y(28);

        auto content = col(
            nav(), hero, logos, features,
            box(stats) | pad_x(28) | w_full,
            box() | h(56),
            box(cta) | pad_x(28) | w_full,
            box() | h(56),
            box() | h(1) | w_full | detail::raw_css("background", "#141b29"),
            footer
        ) | detail::raw_css("position", "relative") | detail::raw_css("z-index", "1");

        return stack(backdrop(m), content)
             | min_h(100_vh) | detail::raw_css("background", "#060912") | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt;
        mt.title = "waya \u00b7 the C++ web framework";
        mt.description = "Render UI on the server, stream only the diff. No JavaScript, no build step.";
        return mt;
    }
};

int main() {
    return live<Aurora>({ .port = 8080, .page_bg = 0x060912, .title = "waya" });
}
