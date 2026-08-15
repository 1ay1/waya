/// dragster/console.hpp — a control as a KEY HINT: a keyboard-key badge showing
/// the key to press, with the action label beside it. Lights up in its cyan
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

// A keyboard-key badge: a small raised cap with the key text.
inline NodeRef keycap(std::string keytext, std::uint32_t c, bool active) {
    waya::Color k = waya::rgb(c);
    return box(text(std::move(keytext)) | fg(active ? page : ink)
                | font(11) | weight(Weight::black) | term | tracking_em(0.02f))
        | pad_x(8) | pad_y(5) | round(px(7)) | center | select_none
        | bg(active ? k.opaque() : line_c)
        | border(1, active ? k.lighten(0.25f).opaque() : waya::rgb(line_c).lighten(0.15f).opaque())
        | inset_light(active ? .35f : .06f)
        | min_w(20);
}

// The control chip: [key] LABEL, in a grouped tray. Whole chip is tappable.
template <class Msg>
inline NodeRef hud_pill(std::string keytext, std::string label, Msg msg, std::uint32_t c, bool active) {
    waya::Color base = waya::rgb(c);
    auto chip = row(
        keycap(std::move(keytext), c, active),
        text(std::move(label)) | fg(active ? base.lighten(0.55f).opaque() : inkSoft)
            | font(12) | weight(Weight::black) | tracking_em(0.08f) | term
    ) | items_center | gap(9);

    auto b = box(chip)
        | pad_x(11) | pad_y(8) | round(px(10))
        | pointer | select_none
        | clickable
        | bg(active ? base.alpha(0.14f) : waya::rgb(panel))
        | border(1, active ? base.opaque() : line_c)
        | (active ? glow(c, 16) : Mod{})
        | transition("transform .08s ease, background .14s ease, border-color .14s ease")
        | on(Hover, bg(active ? base.alpha(0.2f) : waya::rgb(line_c).alpha(0.5f)),
                    border_color(base.alpha(0.6f)))
        | on(Active, translate(0, 1));
    return b | tap(std::move(msg));
}

} // namespace dr
