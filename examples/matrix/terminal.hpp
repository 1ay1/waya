/// matrix/terminal.hpp — the hacker terminal: a translucent pane over the rain
/// showing the typed log + a blinking cursor on the line in progress.
#pragma once

#include "store.hpp"
#include <waya/surface/node.hpp>

namespace mtx {
waya::surface::NodeRef terminal_pane(const Model& m);
} // namespace mtx
