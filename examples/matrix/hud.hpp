/// matrix/hud.hpp — the heads-up panels: breach meter, pwned-node grid, live
/// stats, alert badge, and the control buttons.
#pragma once

#include "store.hpp"
#include <waya/surface/node.hpp>

namespace mtx {
waya::surface::NodeRef hud_panel(const Model& m);
} // namespace mtx
