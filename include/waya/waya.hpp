#pragma once
/// \file waya.hpp
/// waya — a C++26 server-side web framework.
///
/// Include this single header to get the public API: the type-state HTML DSL,
/// the maya-style styling vocabulary, and server-side rendering. Higher tiers
/// (HTTP server, live sessions) are opt-in via their own headers.
///
///   #include <waya/waya.hpp>
///   using namespace waya::dsl;
///   using namespace waya::style;
///   using namespace waya::style::literals;
///
///   auto page = html_(
///       head_(title_(text("Hi"))),
///       body_(
///           h1_(text("Hello")) | fg(0x3b82f6) | bold,
///           div_(text("world")) | row | gap(12_px) | pad(16_px)
///       )
///   );
///   std::string html = waya::render::render_document(page);

// ── DSL: elements, attributes, the content model ───────────────────────────
#include "dsl/element.hpp"
#include "dsl/dynamic.hpp"

// ── Styling: the vocabulary and its type-state gates ────────────────────────
#include "style/tokens.hpp"
#include "style/length.hpp"

// ── Rendering ───────────────────────────────────────────────────────────────
#include "render/html.hpp"

namespace waya {
// Pull the DSL and style vocabularies into `waya::` for convenience; users may
// also `using namespace waya::dsl; using namespace waya::style;` explicitly.
namespace dsl {}
namespace style {}
} // namespace waya
