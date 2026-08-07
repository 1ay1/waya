/// dragster/console.hpp — the panel below the TV: the timing readouts (elapsed,
/// best, gear, rpm) and the physical control buttons. Buttons dispatch the same
/// Msgs the keyboard does.
#pragma once

#include "store.hpp"
#include <waya/surface/node.hpp>

namespace dr {
using namespace waya::surface;

NodeRef brand_plate();
NodeRef readouts(const Model& m);
NodeRef controls(const Model& m);

} // namespace dr
