/// dragster/console.hpp — a control as a KEY HINT: a keyboard-key badge showing
/// the key to press, with the action label beside it. Lights up in its accent
/// colour when active (throttle on / running), presses on tap, dispatches its
/// Msg — matching the keyboard. Header-only (templated).
#pragma once

#include "store.hpp"
#include "theme.hpp"
#include <waya/color.hpp>
#include <waya/surface/node.hpp>
#include <waya/surface/sugar.hpp>
#include <string>

namespace dr {
using namespace waya::surface;

// A keyboard-key badge: a raised cap with the key text (e.g. "space", "↑", "R").
inline NodeRef keycap(std::string keytext, std::uint32_t c, bool active) {
    waya::Color k = waya::rgb(c);
    return box(text(std::move(keytext)) | fg(active ? 0x0b0c11 : 0xe8ecf4)
                | font(11) | weight(Weight::black) | term | tracking_em(0.02f))
        | pad_x(8) | pad_y(5) | round(px(6)) | center | select_none
        | bg(active ? k.opaque() : 0x272b36u)
        | border(1, active ? k.lighten(0.25f).opaque() : 0x3a3f4eu)
        | detail::raw_css("box-shadow",
            active ? "inset 0 1px 0 rgba(255,255,255,.3), 0 2px 0 " + k.darken(0.4f).css()
                   : "inset 0 1px 0 rgba(255,255,255,.08), 0 2px 0 rgba(0,0,0,.5)")
        | detail::raw_css("min-width", "20px");
}

// The control chip: [key] LABEL, in a grouped tray. Whole chip is tappable.
template <class Msg>
inline NodeRef hud_pill(std::string keytext, std::string label, Msg msg, std::uint32_t c, bool active) {
    waya::Color base = waya::rgb(c);
    auto chip = row(
        keycap(std::move(keytext), c, active),
        text(std::move(label)) | fg(active ? base.lighten(0.5f).opaque() : 0xd7dce8u)
            | font(13) | weight(Weight::black) | tracking_em(0.06f) | term
    ) | items_center | gap(9);

    auto b = box(chip)
        | pad_x(11) | pad_y(9) | round(px(10))
        | pointer | select_none
        | detail::raw_css("pointer-events", "auto")
        | bg(active ? base.alpha(0.16f) : waya::rgba(0xffffff, 0.03f))
        | border(1, active ? base.opaque() : 0x2c313cu)
        | (active ? glow(c, 14) : Mod{})
        | transition("transform .07s ease, background .12s ease, box-shadow .12s ease")
        | on(Hover, bg(active ? base.alpha(0.22f) : waya::rgba(0xffffff, 0.08f)))
        | on(Active, detail::raw_css("transform", "translateY(2px)"));
    return b | tap(std::move(msg));
}

} // namespace dr
