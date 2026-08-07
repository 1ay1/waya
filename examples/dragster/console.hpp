/// dragster/console.hpp — the HUD control button. A chunky, tactile "key cap"
/// with a keyboard-glyph badge and a bold label; it glows and depresses when
/// active/pressed and dispatches its Msg on tap, matching the keyboard.
/// Header-only (templated).
#pragma once

#include "store.hpp"
#include "theme.hpp"
#include <waya/color.hpp>
#include <waya/surface/node.hpp>
#include <waya/surface/sugar.hpp>
#include <string>

namespace dr {
using namespace waya::surface;

// readable label colour on a dark cap: a brightened version of the accent.
inline std::uint32_t ink_on(std::uint32_t c){ return waya::rgb(c).lighten(0.35f).opaque(); }

// A little keyboard-key badge (e.g. "␣", "▲", "⏎") shown on the button.
inline NodeRef keycap(std::string glyph, std::uint32_t c, bool active) {
    waya::Color k = waya::rgb(c);
    return box(text(std::move(glyph)) | fg(active ? 0x0a0a0f : c) | font(12) | weight(Weight::black) | term)
        | size(px(22)) | round(px(6)) | center | select_none
        | bg(active ? k.alpha(0.9f) : k.alpha(0.16f))
        | border(1, active ? c : c)
        | detail::raw_css("box-shadow",
            active ? "inset 0 -2px 0 " + k.darken(0.35f).css()
                   : "inset 0 -2px 0 rgba(0,0,0,.35)");
}

// The control button: [keycap] LABEL, as a raised key cap that presses in.
template <class Msg>
inline NodeRef hud_pill(std::string glyph, std::string label, Msg msg, std::uint32_t c, bool active) {
    waya::Color base = waya::rgb(c);

    auto face = row(
        keycap(std::move(glyph), c, active),
        text(std::move(label)) | fg(active ? 0x0a0a0f : ink_on(c))
            | font(13) | weight(Weight::black) | tracking_em(0.08f) | term
    ) | items_center | gap(9);

    auto b = box(face)
        | pad_x(14) | pad_y(10) | round(px(12))
        | pointer | select_none
        // the HUD frame is no_pointer; opt this control back in.
        | detail::raw_css("pointer-events", "auto")
        | (active ? gradient(base.lighten(0.12f).opaque(), base.darken(0.14f).opaque(), 180)
                  : gradient(0x14161c, 0x0c0d12, 180))
        | border(1.5f, active ? base.lighten(0.2f).opaque() : base.darken(0.3f).opaque())
        // the raised "cap" look: bright top edge + a chunky bottom lip + a soft
        // drop shadow; when active it also glows in the button colour.
        | detail::raw_css("box-shadow",
            (active
              ? "0 0 22px " + base.alpha(0.55f).css() + ", "
              : "") +
            std::string("inset 0 1px 0 rgba(255,255,255,.18), "
            "0 4px 0 ") + (active ? base.darken(0.4f).css() : std::string("rgba(0,0,0,.55)")) +
            ", 0 7px 14px -5px rgba(0,0,0,.7)")
        | transition("transform .07s ease, box-shadow .07s ease, background .12s ease")
        // press: sink into the lip.
        | on(Active, detail::raw_css("transform", "translateY(3px)"),
                     detail::raw_css("box-shadow",
                        "inset 0 1px 0 rgba(255,255,255,.12), 0 1px 0 rgba(0,0,0,.55)"))
        | on(Hover, detail::raw_css("filter", "brightness(1.12)"));
    return b | tap(std::move(msg));
}

} // namespace dr
