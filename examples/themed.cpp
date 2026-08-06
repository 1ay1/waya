/// examples/themed.cpp — beautiful, live theme switching in a few lines.
///
///   cmake --build build -j && ./build/themed     # http://localhost:8080
///
/// The whole app is coloured with THEME TOKENS (bg_surface, fg_primary…), never
/// raw hex. A single `theme(model.theme)` on the root declares the tokens as CSS
/// variables. Tapping a swatch swaps the Theme in the model → the entire app
/// re-tints in ONE paint, and the colours animate smoothly (themed/theme_transition).
/// That's live theming: no per-node work, no CSS, no reload.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;

struct Themed {
    struct Model { Theme theme = Theme::dark(); std::string name = "dark"; };

    // A typed message carrying which theme to switch to.
    struct SetTheme { std::string name; };
    using Msg = std::variant<SetTheme>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](const SetTheme& s){
                m.name = s.name;
                m.theme = s.name=="light"    ? Theme::light()
                        : s.name=="midnight" ? Theme::midnight()
                        : s.name=="ocean"    ? Theme::ocean()
                        : s.name=="rose"     ? Theme::rose()
                        : s.name=="green"    ? Theme::dark().tint(0x22c55e, 0x86efac)
                        :                      Theme::dark();
            },
        }, msg);
        return m;
    }

    // A reusable card — coloured entirely with tokens, so it obeys the theme.
    template <typename... Cs>
    static NodeRef card(Cs... cs) {
        return col(std::move(cs)...) | gap(12) | pad(20) | round(16)
             | bg_surface | border_token() | theme_transition() | elevation(2);
    }

    static NodeRef swatch(const Model& m, std::string name, std::uint32_t a, std::uint32_t b) {
        bool active = (m.name == name);
        return col(
            box() | size(px(44)) | round(12) | css("background", "linear-gradient(135deg,"
                 + detail::hexstr(a) + "," + detail::hexstr(b) + ")"),
            text(name) | fg_muted | caption
        ) | gap(6) | center | pad(10) | round(14)
          | (active ? ring(0x6366f1, 2) : noop)
          | tap(SetTheme{name}) | css("cursor","pointer")
          | transition() | on(Hover, css("transform","translateY(-2px)"));
    }

    static NodeRef view(const Model& m) {
        auto content = col(
            col(
                text("Live theming") | display | fg_text,
                text("Tap a palette \u2014 the whole app re-tints, smoothly, in one paint.")
                    | fg_muted | body
            ) | gap(6),

            // the palette picker
            row(
                swatch(m, "dark",     0x6366f1, 0x22d3ee),
                swatch(m, "light",    0x6366f1, 0x0891b2),
                swatch(m, "midnight", 0x8b5cf6, 0x22d3ee),
                swatch(m, "ocean",    0x14b8a6, 0x38bdf8),
                swatch(m, "rose",     0xe11d48, 0xdb2777),
                swatch(m, "green",    0x22c55e, 0x86efac)
            ) | gap(12) | wrap,

            // token showcase: everything below is themed
            row(
                card(text("Surface") | fg_text | subtitle,
                     text("bg_surface + border_token") | fg_muted | caption) | grow(1),
                card(text("Primary") | fg_on_primary | subtitle,
                     text("bg_primary + on_primary") | fg_on_primary | caption)
                     | bg_primary | grow(1)
            ) | gap(16) | wrap,

            card(
                text("Buttons") | fg_text | subtitle,
                row(
                    text("Primary") | fg_on_primary | semibold | bg_primary | pad_x(18) | pad_y(10) | round(10) | theme_transition(),
                    text("Accent")  | fg_on_primary | semibold | bg_accent  | pad_x(18) | pad_y(10) | round(10) | theme_transition(),
                    text("Ghost")   | fg_text | semibold | border_token() | pad_x(18) | pad_y(10) | round(10) | theme_transition()
                ) | gap(12) | wrap
            )
        ) | gap(28);

        // theme(m.theme) declares the tokens; themed() paints bg+text with a
        // smooth transition. One place, live-switchable.
        return page(m.theme.bg,
            centered(52, content) | center
        ) | theme(m.theme) | themed();
    }
};

int main() {
    static_assert(SurfaceProgram<Themed>);
    return live<Themed>({.port = 8080, .title = "waya \u2014 live themes"});
}
