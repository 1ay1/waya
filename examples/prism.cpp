/// examples/prism.cpp — PRISM: a settings panel that lives inside a theme
/// playground. Flip a palette in the top bar and the ENTIRE panel re-tints in a
/// single paint, because every surface reads theme tokens (var(--wa-*)). Real
/// toggle / slider / segmented state rides the socket. A clean two-column
/// settings layout, the kind you'd actually ship.
///
///   waya run prism            # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Prism {
    struct Model {
        int   theme_i = 1;       // 0 slate 1 midnight 2 ocean 3 rose 4 light
        bool  emails  = true;
        bool  twofa   = false;
        float volume  = 68;
        int   density = 1;       // segmented
    };

    struct Pick { int i; }; struct Emails {}; struct TwoFA {}; struct Volume {}; struct Density { int i; };
    using Msg = std::variant<Pick, Emails, TwoFA, Volume, Density>;

    static Model init() { return {}; }
    static Model update(Model m, Msg msg, std::string value) {
        std::visit(overload{
            [&](Pick p)    { m.theme_i = p.i; },
            [&](Emails)    { m.emails = !m.emails; },
            [&](TwoFA)     { m.twofa  = !m.twofa; },
            [&](Volume)    { if (!value.empty()) m.volume = std::stof(value); },
            [&](Density d) { m.density = d.i; },
        }, msg);
        return m;
    }

    static const char* theme_name(int i) {
        static const char* n[] = { "Slate", "Midnight", "Ocean", "Rose", "Light" };
        return n[i % 5];
    }
    static Theme theme_of(int i) {
        switch (i % 5) { case 0: return Theme::dark(); case 1: return midnight();
            case 2: return ocean(); case 3: return rose(); default: return light(); }
    }

    static Mod card() {
        return bg_surface | detail::raw_css("border", "1px solid var(--wa-line)") | round(16)
             | detail::raw_css("box-shadow", "0 12px 32px -16px rgba(0,0,0,.45)");
    }

    // a single settings row: title + description on the left, control on the right
    static NodeRef setting(std::string title_, std::string desc, NodeRef control) {
        return row(
            col(text(std::move(title_)) | fg_text | font(15) | weight(Weight::semibold),
                text(std::move(desc))   | fg_muted | font(13) | leading(1.5f) | max_w(320)) | gap(3),
            box() | grow(),
            box(std::move(control)) | detail::raw_css("flex","0 0 auto")
        ) | items_center | gap(24) | pad_y(18);
    }
    static NodeRef sep() { return box() | h(1) | w_full | detail::raw_css("background","var(--wa-line)"); }

    static NodeRef theme_bar(const Model& m) {
        std::vector<NodeRef> chips;
        for (int i = 0; i < 5; ++i) {
            bool sel = m.theme_i == i;
            Theme t = theme_of(i);
            auto sw = box() | square(22) | round(7)
                    | detail::raw_css("background",
                        "linear-gradient(135deg," + detail::hexstr(t.primary) + "," + detail::hexstr(t.accent) + ")");
            auto chip = row(sw, text(theme_name(i)) | fg_text | font(13) | weight(Weight::semibold))
                      | gap(9) | items_center | pad_x(12) | pad_y(9) | round(10) | pointer | tap(Pick{i})
                      | transition("background-color .15s ease, box-shadow .15s ease");
            if (sel) chip = chip | bg_raised
                         | detail::raw_css("box-shadow","0 0 0 1px var(--wa-primary)");
            else chip = chip | on(Hover, bg_raised);
            chips.push_back(chip);
        }
        return row_(std::move(chips)) | gap(8) | wrap;
    }

    static NodeRef dseg(const Model& m, int i, std::string label) {
        bool sel = m.density == i;
        auto n = text(label) | font(13) | weight(Weight::semibold) | pad_x(16) | pad_y(8)
               | round(8) | pointer | tap(Density{i}) | (sel ? fg_on_primary : fg_muted);
        if (sel) n = n | bg_primary;
        return n;
    }

    static NodeRef view(const Model& m) {
        auto header = row(
            col(text("Settings") | fg_text | font(26) | weight(Weight::black)
                    | detail::raw_css("letter-spacing","-0.02em"),
                text("Manage your workspace preferences") | fg_muted | font(14)) | gap(4),
            box() | grow(),
            theme_bar(m)
        ) | items_center | gap(24) | wrap | w_full;

        // account card
        auto account = col(
            row(box(text("AK") | fg_on_primary | font(16) | weight(Weight::bold))
                    | square(52) | center | circle(26) | bg_primary,
                col(text("Alex Kim") | fg_text | font(18) | weight(Weight::bold),
                    text("alex@waya.dev") | fg_muted | font(14)) | gap(2),
                box() | grow(),
                row(text("Edit profile") | fg_text | font(14) | weight(Weight::semibold))
                    | pad_x(16) | pad_y(9) | round(9)
                    | detail::raw_css("border","1px solid var(--wa-line)")
                    | pointer | on(Hover, bg_raised)) | gap(16) | items_center
        ) | pad(24) | card();

        // preferences card
        auto prefs = col(
            text("Preferences") | fg_muted | font(12) | weight(Weight::bold)
                | tracking_em(0.10f) | uppercase,
            setting("Email notifications", "Product updates, security alerts and weekly digests.",
                    toggle(m.emails, Emails{})),
            sep(),
            setting("Two-factor auth", "Require a second factor when signing in.",
                    toggle(m.twofa, TwoFA{})),
            sep(),
            setting("Interface density", "How compact the layout should be.",
                    row(dseg(m,0,"Cozy"), dseg(m,1,"Comfortable"), dseg(m,2,"Compact"))
                        | gap(3) | pad(3) | round(11) | bg_raised),
            sep(),
            col(row(text("Sound volume") | fg_text | font(15) | weight(Weight::semibold),
                    box() | grow(),
                    text(std::to_string((int)m.volume) + "%") | fg_muted | font(13)
                        | weight(Weight::semibold) | tabular_nums) | items_center,
                slider(m.volume, 0, 100, [](std::string){ return Volume{}; })) | gap(12) | pad_y(18)
        ) | gap(4) | pad_x(24) | pad_y(20) | card();

        // right rail preview
        auto preview = col(
            text("Preview") | fg_muted | font(12) | weight(Weight::bold)
                | tracking_em(0.10f) | uppercase,
            col(
                row(badge("Active", Tone::success), badge(theme_name(m.theme_i), Tone::primary))
                    | gap(8),
                text("Themeable by default") | fg_text | font(17) | weight(Weight::bold),
                text("Every component below reads var(--wa-*), so one root mod recolours all of it.")
                    | fg_muted | font(13) | leading(1.6f),
                row(button("Primary", Emails{}, Variant::primary),
                    button("Secondary", Emails{}, Variant::secondary)) | gap(10),
                button("Ghost action", Emails{}, Variant::ghost)
            ) | gap(14) | pad(20) | card()
        ) | gap(12) | w(320);

        auto body = row(
            col(account | fade_up(500), prefs | fade_up(560) | delay(80)) | gap(20) | grow(),
            preview | fade_up(600) | delay(160)
        ) | gap(20) | wrap | items_start;

        return col(header | fade_up(450), body)
             | gap(28) | pad(32) | max_w(1080) | center_x | min_h(100_vh)
             | theme(theme_of(m.theme_i)) | bg_page | fg_text
             | transition("background-color .35s ease, color .35s ease")
             | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "Prism \u00b7 settings";
        mt.description = "A live-theming settings panel \u2014 flip a palette, re-tint everything in one paint.";
        return mt;
    }
};

int main() { return live<Prism>({ .port = 8080, .page_bg = 0x0b1020, .title = "Prism" }); }
