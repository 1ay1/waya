/// matrix/rain.hpp — the digital rain: falling columns of glyphs rendered as an
/// SVG. The bright "head" glyph leads a fading green trail down each column.
#pragma once

#include "store.hpp"
#include <waya/surface/node.hpp>

namespace mtx {
// Full-bleed rain canvas for the current frame.
waya::surface::NodeRef rain_canvas(const Model& m);
} // namespace mtx
