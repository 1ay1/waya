/// examples/studio.cpp — a design studio: live theme switching + every polish
/// mod on one gorgeous screen. Tap a palette and the whole app re-tints in one
/// paint, smoothly. Built from frost(), hover_lift(), the type scale, and theme
/// tokens.
///
///   cmake --build build -j && ./build/studio     # http://localhost:8080

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::color;
using namespace waya::ui;

struct Studio {
    struct Model { Theme theme = midnight(); std::string name = "midnight"; bool about = false; };
    struct Pick { std::string name; }; struct About {};
    using Msg = std::variant<Pick, About>;

    static Model init() { return {}; }
    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](const Pick& p){
                m.name = p.name;
                m.theme = p.name=="dark"?Theme::dark() : p.name=="light"?light()
                        : p.name=="ocean"?ocean() : p.name=="rose"?rose()
                        : midnight();
            },
            [&](About){ m.about = !m.about; },
        }, msg);
        return m;
    }

    template <typename... Cs>
    static NodeRef card(Cs... cs) {
        return col(std::move(cs)...) | gap(14) | pad(24) | round(20)
             | bg_surface | border_token() | elevation(3) | theme_transition();
    }

    static NodeRef swatch(const Model& m, std::string name, std::uint32_t a, std::uint32_t b) {
        bool active = m.name == name;
        return col(
            box() | size(px(52)) | round(14) | gradient_bg(a, b, 135) | glow(a, 20) | when_(active, float_(3)),
            text(name) | fg_muted | caption | (active ? semibold : noop)
        ) | gap(8) | center | pad(12) | round(16)
          | (active ? ring(a, 2) : noop) | interactive() | tap(Pick{name});
    }

    static NodeRef view(const Model& m) {
        auto header = row(
            col(
                text("waya studio") | fg_text | display | font_fluid(32, 54) | weight(Weight::black)
                    | css("letter-spacing","-.02em") | aurora_text(0x8b5cf6, 0x22d3ee, 0xf472b6, 8),
                text("design tokens, motion, glass — switch a theme, watch it flow")
                    | fg_muted | body
            ) | gap(8),
            push(),
            link("about") | tap(About{})
        ) | fade_up(500) | css("align-items","flex-start");

        auto palette = card(
            text("Theme") | fg_muted | label,
            row(
                swatch(m, "midnight", 0x8b5cf6, 0x22d3ee),
                swatch(m, "dark",     0x6366f1, 0x22d3ee),
                swatch(m, "ocean",    0x14b8a6, 0x38bdf8),
                swatch(m, "light",    0x6366f1, 0x0891b2),
                swatch(m, "rose",     0xe11d48, 0xdb2777)
            ) | gap(12) | wrap
        ) | fade_up(600) | delay(60);

        auto showcase = row(
            card(
                text("Motion") | fg_muted | label,
                row(
                    box() | size(px(40)) | round(999) | bg_primary | spin(),
                    box() | size(px(40)) | round(12) | bg_accent | pulse(),
                    box() | size(px(40)) | round(12) | shimmer()
                ) | gap(16) | center
            ) | grow(1) | css("min-width", "14rem") | hover_lift(3),
            card(
                text("Type") | fg_muted | label,
                text("Display") | fg_text | display | font(28),
                text("Heading") | fg_text | heading,
                text("Body text flows nicely.") | fg_muted | body,
                text("CAPTION \u00b7 MONO") | fg_muted | caption | mono
            ) | grow(1) | css("min-width", "14rem") | hover_lift(3)
        ) | gap(20) | wrap | fade_up(700) | delay(120);

        auto buttons = card(
            text("Buttons") | fg_muted | label,
            row(
                text("Primary") | fg_on_primary | semibold | bg_primary | pad_x(20) | pad_y(11) | round(11) | interactive() | theme_transition(),
                text("Accent")  | fg_on_primary | semibold | bg_accent  | pad_x(20) | pad_y(11) | round(11) | interactive() | theme_transition(),
                text("Glass")   | fg_text | semibold | frost(12) | pad_x(20) | pad_y(11) | round(11) | interactive(),
                text("Ghost")   | fg_text | semibold | border_token() | pad_x(20) | pad_y(11) | round(11) | interactive() | theme_transition()
            ) | gap(12) | wrap
        ) | fade_up(800) | delay(180);

        auto about = dialog(m.about, About{},
            text("About waya studio") | fg_text | subtitle | weight(Weight::bold),
            divider(),
            text("Every colour here is a theme token; every card, button and this "
                 "dialog are one-line helpers. Click the backdrop to close — but "
                 "clicking this panel won't.") | fg_muted | body | leading(1.6f),
            row(push(), text("Got it") | fg_on_primary | semibold | bg_primary
                | pad_x(20) | pad_y(11) | round(11) | interactive() | tap(About{})));

        return page(m.theme.bg, centered(56, col(header, palette, showcase, buttons, about) | gap(28)) | center)
             | theme(m.theme) | themed();
    }
};

int main() {
    static_assert(SurfaceProgram<Studio>);
    return live<Studio>({ .port = 8080, .page_bg = 0x0a0a0f, .title = "waya studio" });
}
