/// dragster/screen.hpp — the CRT screen: a side-view drag strip (blue sky over a
/// green track), distance stripes scrolling past, and the red dragster whose
/// horizontal position tracks m.pos down the 1/4-mile. Declares the render.
#pragma once

#include "store.hpp"
#include <waya/surface/node.hpp>

namespace dr {
using namespace waya::surface;

/// The TV picture for the current model (strip + car + finish line).
NodeRef strip_screen(const Model& m);

/// The tachometer bar: TACH_ZONES segments that light up with rpm; the top zones
/// are the redline and glow hot when you're revving into them.
NodeRef tachometer(const Model& m);

/// Just the segmented shift-light BAR (no label/caption) — for embedding in the
/// data strip where the caption is supplied by the strip itself.
NodeRef tachometer_bar(const Model& m);

} // namespace dr
