/// examples/prism.cpp — PRISM: a live theme playground. Flip between five
/// palettes and watch the ENTIRE component gallery re-tint in a single paint —
/// buttons, badges, inputs, toggles, sliders, a progress bar and cards, all
/// reading theme tokens (var(--wa-*)). Proof that `theme(t)` on the root is the
/// only wiring a themeable app needs. State (the toggle/slider) is real and
/// round-trips over the socket.
///
///   waya run prism            # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <array>
#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Prism {
    struct Model {
        int   theme_i = 1;        // 0 dark 1 midnight 2 ocean 3 rose 4 light
        bool  notify  = true;
        float volume  = 62;
        int   plan    = 1;        // segmented control
    };

    struct Pick { int i; }; struct ToggleNotify {}; struct Volume {}; struct Plan { int i; };
    using Msg = std::variant<Pick, ToggleNotify, Volume, Plan>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg, std::string value) {
        std::visit(overload{
            [&](Pick p)       { m.theme_i = p.i; },
            [&](ToggleNotify) { m.notify = !m.notify; },
            [&](Volume)       { m.volume = value.empty() ? m.volume : std::stof(value); },
            [&](Plan p)       { m.plan = p.i; },
        }, msg);
        return m;
    }

    static const char* theme_name(int i) {
        static const char* n[] = { "Dark", "Midnight", "Ocean", "Rose", "Light" };
        return n[i % 5];
    }
    static Theme theme_of(int i) {
        switch (i % 5) {
            case 0: return Theme::dark();
            case 1: return midnight();
            case 2: return ocean();
            case 3: return rose();
            default: return light();
        }
    }
    static std::uint32_t swatch(int i) { return theme_of(i).primary; }

    // A panel using theme tokens so it recolours with the theme.
    static NodeRef panel(NodeRef inner, std::string title_) {
        return col(
            text(std::move(title_)) | fg_muted | font(12) | semibold
                | tracking_em(0.10f) | uppercase,
            inner
        ) | gap(14) | pad(22) | round(18)
          | bg_surface | border_token() | elevation(2);
    }

    static NodeRef theme_switch(const Model& m) {
        std::vector<NodeRef> chips;
        for (int i = 0; i < 5; ++i) {
            bool on = m.theme_i == i;
            auto chip = row(
                box() | circle(12) | bg(swatch(i)) | ring(0xffffff, on ? 2 : 0),
                text(theme_name(i)) | fg_text | font(13) | semibold
            ) | gap(9) | items_center | pad_x(13) | pad_y(9) | round(11)
              | detail::raw_css("background", on ? "var(--wa-raised)" : "transparent")
              | border_token() | pointer | tap(Pick{ i })
              | transition("background-color .15s ease");
            chips.push_back(chip);
        }
        return row_(std::move(chips)) | gap(10) | wrap;
    }

    static NodeRef seg(const Model& m, int i, std::string label) {
        bool on = m.plan == i;
        auto n = text(label) | font(13) | semibold | pad_x(16) | pad_y(8) | round(9)
               | pointer | tap(Plan{ i })
               | (on ? fg_on_primary : fg_muted);
        if (on) n = n | bg_primary;
        return n;
    }

    static NodeRef view(const Model& m) {
        auto header = col(
            row(box(text("\u25C8") | font(22) | fg_on_primary)
                    | square(44) | center | round(12) | bg_primary
                    | glow(swatch(m.theme_i), 20),
                col(text("Prism") | fg_text | font(24) | weight(Weight::black) | leading(1.f),
                    text("live theming \u00b7 one root mod") | fg_muted | font(13)) | gap(2)
                ) | gap(14) | items_center,
            theme_switch(m)
        ) | gap(20);

        // buttons + badges gallery
        auto buttons = panel(col(
            row(button("Primary", ToggleNotify{}, Variant::primary),
                button("Secondary", ToggleNotify{}, Variant::secondary),
                button("Ghost", ToggleNotify{}, Variant::ghost),
                button("Danger", ToggleNotify{}, Variant::danger)) | gap(10) | wrap,
            row(badge("neutral"), badge("primary", Tone::primary),
                badge("success", Tone::success), badge("warning", Tone::warning),
                badge("danger", Tone::danger)) | gap(8) | wrap | items_center
        ) | gap(14), "Buttons & badges");

        // controls gallery
        auto controls = panel(col(
            row(col(text("Notifications") | fg_text | font(14) | semibold,
                    text("email + push") | fg_muted | font(12)) | gap(1),
                push(),
                toggle(m.notify, ToggleNotify{})) | items_center,
            divider(),
            col(row(text("Volume") | fg_text | font(14) | semibold, push(),
                    text(std::to_string((int)m.volume) + "%") | fg_muted | font(13) | tabular_nums)
                    | items_center,
                slider(m.volume, 0, 100, [](std::string){ return Volume{}; })) | gap(8),
            divider(),
            col(text("Plan") | fg_text | font(14) | semibold,
                row(seg(m, 0, "Free"), seg(m, 1, "Pro"), seg(m, 2, "Team"))
                    | gap(4) | pad(4) | round(11) | bg_raised
                    | detail::raw_css("width","fit-content")) | gap(8)
        ) | gap(16), "Controls");

        // a profile-ish card
        auto profile = panel(col(
            row(avatar("WA", 46),
                col(text("waya UI") | fg_text | font(16) | semibold,
                    text("themeable by default") | fg_muted | font(13)) | gap(1),
                push(),
                badge(theme_name(m.theme_i), Tone::primary)) | gap(14) | items_center,
            divider(),
            field("Workspace", input("acme-inc") | input_skin()),
            progress(m.volume, Tone::primary)
        ) | gap(16), "Profile");

        auto gallery = row(
            col(buttons, profile) | gap(20) | grow(),
            controls | w(360)
        ) | gap(20) | wrap;

        return col(header, gallery)
             | gap(24) | pad(32) | max_w(1040) | center_x | min_h(100_vh)
             | theme(theme_of(m.theme_i))
             | bg_page | fg_text
             | transition("background-color .35s ease, color .35s ease")
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Prism";
        mt.description = "A live theme playground \u2014 flip palettes and re-tint the whole app in one paint.";
        return mt;
    }
};

int main() {
    return live<Prism>({ .port = 8080, .title = "waya \u00b7 prism" });
}
