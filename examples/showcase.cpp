/// examples/showcase.cpp — SHOWCASE: a full product page assembled almost
/// entirely from waya/ui PATTERNS (nav_bar, hero, page_header, stat,
/// metric_card, section, list_row, feature_card, banner, empty_state, kbd,
/// tag). It shows how little code a real, polished screen takes when the
/// building blocks are one call each.
///
///   waya run showcase         # then open the printed URL

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/ui.hpp>

#include <string>
#include <variant>

using namespace waya::surface;
using namespace waya::surface::literals;
using namespace waya::ui;

struct Showcase {
    struct Model { int tab = 0; bool starred = false; long stars = 1240; };
    struct Tab { int i; }; struct Star {};
    using Msg = std::variant<Tab, Star>;

    static Model init() { return {}; }
    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Tab t) { m.tab = t.i; },
            [&](Star)  { m.starred = !m.starred; m.stars += m.starred ? 1 : -1; },
        }, msg);
        return m;
    }

    static NodeRef brand() {
        return row(box(icon("home", 17) | fg(0xffffff)) | size(30) | center | round(9)
                       | detail::raw_css("background", "linear-gradient(135deg,#6d7cff,#00d4ff)"),
                   text("waya") | fg_text | detail::raw_css("font-size", "17px") | weight(Weight::bold))
             | gap(10) | items_center;
    }

    static NodeRef view(const Model& m) {
        auto star_btn = row(icon("star", 15) | (m.starred ? fg(0xffd24a) : fg_muted),
                            text(std::to_string(m.stars)) | fg_text | detail::raw_css("font-size","14px")
                                | semibold | tabular_nums)
            | gap(8) | items_center | pad_x(14) | pad_y(9) | round(10)
            | detail::raw_css("border","1px solid var(--wa-line, rgba(255,255,255,.14))")
            | pointer | tap(Star{}) | role("button") | aria_label("Star") | tab_index(0);

        auto nav = nav_bar(brand(),
            nav_link("Features") | tap(Tab{0}),
            nav_link("Metrics") | tap(Tab{1}),
            nav_link("Activity") | tap(Tab{2}),
            star_btn,
            button("Get started", Star{}));

        auto tabs_row = tabs(m.tab, {{0,"Features"},{1,"Metrics"},{2,"Activity"}},
                             [](int i){ return Tab{ i }; });

        // three tab panels, only one shown — when() collapses the others to nothing.
        auto features = row(
            feature_card("bell", "Real-time", "State lives on the server; every change streams as a minimal patch.", Tone::primary),
            feature_card("settings", "Typed", "Your app is a Model, a Msg variant, and pure functions.", Tone::success),
            feature_card("heart", "Beautiful", "Gradients, glass and motion are each a single word.", Tone::danger)
        ) | gap(16) | wrap;

        auto metrics = col(
            row(metric_card("Revenue", "$48.2k", "+12%", Tone::success),
                metric_card("Active users", "8,642", "+4.1%", Tone::primary),
                metric_card("Error rate", "0.4%", "-2%", Tone::danger)) | gap(16) | wrap,
            banner("You're on the Free plan \u2014 upgrade for unlimited seats.", Tone::warning)
        ) | gap(20);

        auto activity = section("Recent activity",
            list_row(avatar("AK"), "Alex Kim", "deployed to production \u00b7 2m ago", badge("deploy", Tone::success)),
            list_row(avatar("JD"), "Jane Doe", "opened a pull request \u00b7 8m ago", badge("PR", Tone::primary)),
            list_row(avatar("SM"), "Sam Ito", "left a comment \u00b7 15m ago"),
            divider(),
            key_value("Total commits", "1,204"),
            key_value("Contributors", "38"),
            key_value("Open issues", "12"));

        auto content = col(
            when(m.tab == 0, [&]{ return features; }),
            when(m.tab == 1, [&]{ return metrics; }),
            when(m.tab == 2, [&]{ return activity; })
        );

        auto body = col(
            hero_section("A framework you can read",
                         "Server-rendered UI in pure C++ \u2014 every screen is a function.",
                         button("Start building", Star{}),
                         row(kbd("\u2318"), kbd("K")) | gap(4)),
            page_header("Overview", "Everything at a glance", button("Export", Star{}, Variant::secondary)),
            tabs_row,
            content,
            when(false, [&]{ return empty_state("Nothing here", "This panel is hidden."); })
        ) | gap(28) | pad_x(28) | pad_y(28) | max_w(1080) | center_x | w_full;

        return col(nav, body)
             | min_h(100_vh) | theme(midnight()) | bg_page | fg_text | as_main;
    }

    static Meta meta(const Model&) {
        Meta mt; mt.title = "waya \u00b7 Showcase";
        mt.description = "A product page built entirely from waya/ui patterns.";
        return mt;
    }
};

int main() { return live<Showcase>({ .port = 8080, .page_bg = 0x0a0a0f, .title = "Showcase" }); }
