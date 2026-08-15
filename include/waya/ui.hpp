#pragma once
/// \file ui.hpp
/// The official waya component library — one include for batteries.
///
///   #include <waya/surface/live.hpp>   // the framework
///   #include <waya/ui.hpp>             // the components
///   using namespace waya::surface;
///   using namespace waya::ui;
///
/// Everything in `waya::ui` is built ONLY on the public surface core — no private
/// API, no runtime hooks. Each component is a plain function you can read, copy,
/// and replace. The core (waya/surface/*) lets you build any UI; this library is
/// a well-made default set so the common 90% is one call.

#include "ui/theme.hpp"
#include "ui/space.hpp"
#include "ui/motion.hpp"
#include "ui/components.hpp"
#include "ui/icons.hpp"
#include "ui/widgets.hpp"
#include "ui/number_input.hpp"
#include "ui/charts.hpp"
#include "ui/chart.hpp"
#include "ui/scene.hpp"
#include "ui/code_view.hpp"
#include "ui/async.hpp"
#include "ui/toast.hpp"
#include "ui/optimistic.hpp"
#include "ui/form.hpp"
#include "ui/keymap.hpp"
#include "ui/command_palette.hpp"
#include "ui/history.hpp"
#include "ui/routes.hpp"
#include "ui/table.hpp"
#include "ui/data_grid.hpp"
#include "ui/reorderable.hpp"
#include "ui/i18n.hpp"
#include "ui/wizard.hpp"
#include "ui/infinite_scroll.hpp"
#include "ui/tree.hpp"
#include "ui/markdown.hpp"
#include "ui/presence.hpp"
#include "ui/split_pane.hpp"
#include "ui/virtual_list.hpp"
#include "ui/patterns.hpp"
#include "ui/pickers.hpp"
