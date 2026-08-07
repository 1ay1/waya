/// matrix/hud.cpp — the HUD stack: breach progress, node grid, stats, alert.
#include "hud.hpp"
#include "theme.hpp"

#include <waya/surface/sugar.hpp>

namespace mtx {

using namespace waya::surface;

namespace {
std::uint32_t alert_color(int a){ return a==0 ? green : a==1 ? amber : red; }
const char*   alert_text (int a){ return a==0 ? "NOMINAL" : a==1 ? "WARNING" : "CRITICAL"; }

// a small titled hud box
NodeRef hbox(std::string title, NodeRef body, std::uint32_t accent = green) {
    return col(
        text(std::move(title)) | fg(dim) | font(10) | term_font | uppercase | tracking_em(0.18f),
        body
    ) | gap(9) | pad(14) | round(6) | w_full
      | detail::raw_css("background", "rgba(2,10,6,.95)")
      | detail::raw_css("border", "1px solid " + hx(line))
      | pane(accent);
}

// a bracketed control button
template <class Msg>
NodeRef ctl(std::string label, std::uint32_t c, Msg msg) {
    return box(text("[ " + label + " ]") | fg(c) | font(12) | term_font | weight(Weight::bold) | phosphor(c, 4))
        | pad_x(12) | pad_y(9) | round(4) | pointer
        | detail::raw_css("background", hxa(c, "12"))
        | detail::raw_css("border", "1px solid " + hxa(c, "44"))
        | transition("all .14s ease")
        | on(Hover, detail::raw_css("background", hxa(c, "22")))
        | tap(std::move(msg));
}
} // namespace

NodeRef hud_panel(const Model& m) {
    std::uint32_t ac = alert_color(m.alert);

    // ── alert badge ──
    auto badge = row(
        box() | detail::raw_css("width","9px") | detail::raw_css("height","9px") | round(999)
            | detail::raw_css("background", hx(ac)) | phosphor(ac, 8)
            | detail::raw_css("animation", m.alert>0 ? "mtx-pulse 1s ease-in-out infinite" : "none"),
        text(alert_text(m.alert)) | fg(ac) | font(13) | term_font | weight(Weight::bold) | phosphor(ac, 5)
            | tracking_em(0.1f)
    ) | gap(9) | items_center | pad_x(12) | pad_y(9) | round(4) | w_full
      | detail::raw_css("background", hxa(ac, "10")) | detail::raw_css("border","1px solid "+hxa(ac,"3a"));

    // ── breach meter ──
    auto meter = hbox("breach progress", col(
        row(text(std::to_string(m.breach)) | fg(ac) | font(30) | term_font | weight(Weight::black) | phosphor(ac,6) | tabular_nums,
            text("%") | fg(dim) | font(16) | term_font) | gap(2) | items_baseline,
        box(box() | detail::raw_css("height","8px") | round(2)
                | detail::raw_css("width", std::to_string(m.breach)+"%")
                | detail::raw_css("background", "linear-gradient(90deg,"+hx(green)+","+hx(ac)+")")
                | phosphor(ac, 6)
                | transition("width .25s linear"))
            | w_full | detail::raw_css("height","8px") | round(2)
            | detail::raw_css("background", hx(0x061a0e)) | detail::raw_css("border","1px solid "+hx(line))
    ) | gap(10), ac);

    // ── node grid (7 nodes; lit as they're pwned) ──
    std::vector<NodeRef> nodes;
    for (int i = 0; i < 7; ++i) {
        bool on = i < m.nodes_pwned;
        nodes.push_back(box(text(on ? "\u25c9" : "\u25cc") | fg(on ? green : line) | font(18) | term_font
                            | (on ? phosphor(green, 6) : detail::raw_css("opacity",".5")))
            | detail::raw_css("width","34px") | detail::raw_css("height","34px") | round(4) | center
            | detail::raw_css("background", on ? hxa(green,"12") : hxa(0xffffff,"03"))
            | detail::raw_css("border","1px solid "+ (on ? hxa(green,"44") : hx(line))));
    }
    auto grid = hbox("proxy nodes", row_(nodes) | gap(6) | wrap);

    // ── live stats ──
    auto stat = [](std::string k, std::string v, std::uint32_t c){
        return row(text(k) | fg(dim) | font(11) | term_font, box() | grow(),
                   text(std::move(v)) | fg(c) | font(12) | term_font | weight(Weight::bold) | tabular_nums | phosphor(c,3))
            | w_full | items_center; };
    auto stats = hbox("telemetry", col(
        stat("frames", std::to_string(m.frame), green),
        stat("nodes", std::to_string(m.nodes_pwned)+"/7", green),
        stat("packets", std::to_string(m.frame*137 % 99999), green),
        stat("uplink", m.running ? "ACTIVE" : "HALTED", m.running ? green : red)
    ) | gap(8));

    // ── controls ──
    auto controls = row(
        ctl(m.running ? "PAUSE" : "RESUME", green, Toggle{}),
        ctl("ESCALATE", amber, Escalate{}),
        ctl("REBOOT", red, Reboot{})
    ) | gap(10) | wrap;

    return col(badge, meter, grid, stats, controls) | gap(14) | w_full
        | detail::raw_css("width","300px") | detail::raw_css("max-width","100%");
}

} // namespace mtx
