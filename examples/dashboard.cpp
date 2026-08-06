/// examples/dashboard.cpp — proof that waya builds dense, real UIs out of small
/// REUSABLE components, where a "component" is simply a function returning a
/// NodeRef. No component base class, no lifecycle, no boilerplate — you compose
/// nodes with the same `|` grammar you'd use for a single box.
///
///   cmake --build build -j && ./build/dashboard   # http://localhost:8080
///
/// It renders to 100%-valid HTML5 + CSS3 (checked against the W3C validators).

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>

#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

// ── Components: each is a plain function `… -> NodeRef` ───────────────────────

/// A circular avatar with initials (uses a ZStack to centre the text).
static NodeRef avatar(std::string initials, std::uint32_t c = brand) {
    return stack(text(std::move(initials)) | fg(0xffffff) | font(15) | semibold)
         | size(px(40)) | round(999) | bg(c);
}

/// A pill badge.
static NodeRef badge(std::string label, std::uint32_t c) {
    return text(std::move(label)) | fg(0xffffff) | font(12) | semibold
         | pad_x(10) | pad_y(4) | round(999) | bg(c);
}

/// A card container — variadic, so it accepts ANY children (slot-passing).
template <typename... Cs>
static NodeRef card(Cs... cs) {
    return col(std::move(cs)...) | gap(16) | pad_fluid(16, 20)
         | round(16) | bg(bg1) | border(1, line);
}

/// A KPI stat with an up/down delta.
static NodeRef stat(std::string label, std::string value, std::string delta, bool up) {
    return card(
        text(std::move(label)) | fg(muted) | font(13),
        text(std::move(value)) | fg(ink) | font_fluid(24, 32) | weight(Weight::black)
                               | css("white-space", "nowrap"),
        row(text(up ? "\u25b2" : "\u25bc") | fg(up ? good : 0xef4444) | font(12),
            text(std::move(delta)) | fg(up ? good : 0xef4444) | font(13)) | gap(6) | center
    ) | grow(1) | css("min-width", "9rem");
}

/// A settings toggle row (label + description + checkbox, space-between).
static NodeRef toggle_row(std::string label, std::string desc, bool on, int msg) {
    return row(
        col(text(std::move(label)) | fg(ink) | font(15) | semibold,
            text(std::move(desc)) | fg(muted) | font(13)) | gap(2) | grow(1),
        checkbox(on) | on_change(msg) | size(px(20))
    ) | gap(16) | center | pad_y(12);
}

/// A tab bar; the active tab is highlighted and underlined.
static NodeRef tab_bar(std::vector<std::string> labels, int active, int base_msg) {
    return row_(each_i(labels, [&](const std::string& l, std::size_t i){
        bool active_ = (int)i == active;
        return text(l) | fg(active_ ? ink : muted) | font(14) | (active_ ? semibold : noop)
             | pad_x(14) | pad_y(10) | tap(base_msg + (int)i)
             | css("border-bottom", active_ ? "2px solid #6366f1" : "2px solid transparent")
             | transition() | on(Hover, fg(ink));
    })) | gap(4) | wrap
       | css("border-bottom", "1px solid #1f2937");
}

/// A data table from headers + rows — all built with `each`.
static NodeRef data_table(std::vector<std::string> headers,
                          std::vector<std::vector<std::string>> rows) {
    auto head = row_(each(headers, [](const std::string& h){
        return text(h) | fg(muted) | font(12) | weight(Weight::bold) | grow(1)
             | css("text-transform", "uppercase") | css("letter-spacing", "0.05em");
    })) | gap(12) | pad_y(8);
    auto body = col_(each(rows, [](const std::vector<std::string>& r){
        return row_(each(r, [](const std::string& c){ return text(c) | fg(ink) | font(14) | grow(1); }))
             | gap(12) | pad_y(10) | css("border-top", "1px solid #1f2937");
    }));
    return card(head, body);
}

// ── The app: compose the components. Nothing here knows they're "components". ─

struct Dashboard {
    struct Model { int tab = 0; bool notif = true, weekly = false, twofa = true; };
    using Msg = int;
    enum { Tab0, Tab1, Tab2, Notif = 10, Weekly, TwoFA };

    static Model init() { return {}; }
    static Model update(Model m, Msg msg, std::string) {
        if (msg <= Tab2) m.tab = msg;
        else if (msg == Notif)  m.notif  = !m.notif;
        else if (msg == Weekly) m.weekly = !m.weekly;
        else if (msg == TwoFA)  m.twofa  = !m.twofa;
        return m;
    }

    static NodeRef view(const Model& m) {
        auto header = row(
            row(avatar("AY"),
                col(text("Ayush Sharma") | fg(ink) | font(16) | semibold,
                    text("ayush@waya.dev") | fg(muted) | font(13)) | gap(2)) | gap(12) | center,
            row(badge("LIVE", good), badge("Pro", brand2)) | gap(8) | center
        ) | between | wrap | center | gap(16);

        auto stats = row(
            stat("Revenue", "$48.2k", "12.5%", true),
            stat("Active users", "8,421", "3.1%", true),
            stat("Churn", "2.4%", "0.8%", false)
        ) | gap(16) | wrap;

        NodeRef body =
            m.tab == 0 ? data_table({"Name", "Role", "Status"}, {
                            {"Ada Lovelace", "Engineer", "Active"},
                            {"Alan Turing",  "Researcher", "Active"},
                            {"Grace Hopper", "Architect", "Away"}})
          : m.tab == 1 ? card(
                            toggle_row("Email notifications", "Get notified on new activity", m.notif, Notif),
                            toggle_row("Weekly digest", "A summary every Monday", m.weekly, Weekly),
                            toggle_row("Two-factor auth", "Extra security at sign-in", m.twofa, TwoFA))
          :              card(text("Analytics coming soon.") | fg(muted) | font(15) | pad_y(24) | center);

        return page(bg0, centered(60,
            col(header, stats, tab_bar({"Team", "Settings", "Analytics"}, m.tab, Tab0), body) | gap(24)
        ));
    }
};

int main() {
    static_assert(SurfaceProgram<Dashboard>);
    return live<Dashboard>({.port = 8080});
}
