/// dragster/console.hpp — the flat HUD control pill used on the full-window
/// screen. A glassy button that lights up when active and dispatches its Msg on
/// tap (touch/click), matching the keyboard. Header-only (templated).
#pragma once

#include "store.hpp"
#include "theme.hpp"
#include <waya/color.hpp>
#include <waya/surface/node.hpp>
#include <waya/surface/sugar.hpp>
#include <string>

namespace dr {
using namespace waya::surface;

template <class Msg>
inline NodeRef hud_pill(std::string label, Msg msg, std::uint32_t c, bool active) {
    waya::Color base = waya::rgb(c);
    auto face = text(std::move(label))
        | fg(active ? 0x0a0a0f : c) | font(13) | weight(Weight::black) | tracking_em(0.06f) | term;

    auto b = box(face)
        | pad_x(16) | pad_y(11) | round(rem(2))
        | pointer | select_none
        // the HUD frame is no_pointer; opt this control back in (no Mod for it).
        | detail::raw_css("pointer-events", "auto")
        | (active ? gradient(c, base.darken(0.18f).opaque(), 180)
                  : bg(base.alpha(0.14f)))
        | border(1.5f, c)
        | (active ? glow(c, 18) : frost(6, 0.04f))
        | press(0.96f)
        | on(Hover, bg(base.alpha(active ? 1.0f : 0.26f)));
    return b | tap(std::move(msg));
}

} // namespace dr
