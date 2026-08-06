#pragma once
/// \file ui/theme.hpp
/// The official theme presets — ready-made colour palettes for the token system.
///
/// The CORE ships the token *mechanism* (the `Theme` struct, `theme()` to bind
/// it to CSS variables, the `fg_*`/`bg_*` token mods) and ONE neutral baseline
/// (`Theme::dark()`). This file adds OPINIONATED palettes — light, midnight,
/// ocean, rose — so you get a polished look without hand-picking twelve hex
/// values. They're just `Theme` values; nothing here is privileged over a theme
/// you write yourself.
///
///   #include <waya/ui.hpp>
///   using namespace waya::ui;
///   root | theme(light());     // or midnight(), ocean(), rose(), or your own

#include "../surface/sugar.hpp"

namespace waya::ui {

using surface::Theme;

/// A crisp light palette (white surfaces, indigo primary).
inline Theme light() {
    return { 0xf8fafc, 0xffffff, 0xf1f5f9, 0xe2e8f0, 0x0f172a, 0x64748b,
             0x6366f1, 0x0891b2, 0x059669, 0xd97706, 0xdc2626, 0xffffff };
}
/// Near-black with a violet accent — high-contrast dark.
inline Theme midnight() {
    return { 0x0a0a0f, 0x14141f, 0x1e1e2e, 0x2a2a3c, 0xe4e4f0, 0x8888a8,
             0x8b5cf6, 0x22d3ee, 0x34d399, 0xf59e0b, 0xef4444, 0xffffff };
}
/// Deep teal / cyan.
inline Theme ocean() {
    return { 0x081c22, 0x0d2b33, 0x123c47, 0x1d5563, 0xe0f2f1, 0x80cbc4,
             0x14b8a6, 0x38bdf8, 0x34d399, 0xfbbf24, 0xfb7185, 0x042f2e };
}
/// Warm light with a rose accent.
inline Theme rose() {
    return { 0xfff1f2, 0xffffff, 0xffe4e6, 0xfecdd3, 0x4c0519, 0x9f1239,
             0xe11d48, 0xdb2777, 0x059669, 0xd97706, 0xdc2626, 0xffffff };
}

} // namespace waya::ui
