#pragma once
/// \file ui/reorderable.hpp
/// reorderable — drag-to-reorder a list, the move computed for you.
///
/// waya has `draggable()` + `on_drop()` primitives, but wiring a REORDERABLE
/// list from them means: mark each item draggable with its index, make each a
/// drop target tagged with its index, parse the "src:dest" the drop delivers,
/// and splice the vector. `reorderable` folds that into two pieces:
///
///   • `reorder_row(index, onDrop, content)` wraps one item so it can be dragged
///     onto any other — dropping fires `onDrop(from, to)`.
///   • `apply_reorder(vec, from, to)` performs the move on your data.
///
/// Usage:
///
///   struct Model { std::vector<Task> tasks; };
///   struct Dropped { int from, to; };
///
///   // update:
///   [&](Dropped d){ apply_reorder(m.tasks, d.from, d.to); return {m, Cmd::none()}; }
///
///   // view:
///   std::vector<NodeRef> rows;
///   for (int i = 0; i < (int)m.tasks.size(); ++i)
///       rows.push_back(reorder_row(i,
///           [](int from, int to){ return Dropped{from, to}; },
///           task_card(m.tasks[i])));
///   col_(std::move(rows)) | gap(8)
///
/// The drop payload is the dragged item's index; the target carries its own
/// index as the drop-arg, so `on_drop` receives "from:to" and the mapper splits
/// it. `apply_reorder` is a pure vector op you can unit-test.

#include "../surface/node.hpp"

#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// Move the element at `from` to `to`, shifting the rest (the standard list
/// reorder). No-op if either index is out of range or they're equal.
template <typename T>
inline void apply_reorder(std::vector<T>& v, int from, int to){
    int n = (int)v.size();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;
    T moved = std::move(v[from]);
    v.erase(v.begin() + from);
    v.insert(v.begin() + to, std::move(moved));
}

/// Parse the "from:to" payload a reorder drop delivers into a pair of indices.
/// Returns {-1,-1} on a malformed payload (so the mapper can drop it).
inline std::pair<int,int> parse_reorder(const std::string& payload){
    auto colon = payload.find(':');
    if (colon == std::string::npos) return { -1, -1 };
    // both halves must be all digits (a bad drag drops safely).
    auto all_digits = [](const std::string& s){
        if (s.empty()) return false;
        for (char c : s) if (c < '0' || c > '9') return false;
        return true;
    };
    std::string a = payload.substr(0, colon), b = payload.substr(colon + 1);
    if (!all_digits(a) || !all_digits(b)) return { -1, -1 };
    return { std::stoi(a), std::stoi(b) };
}

/// `reorder_row(index, onDrop, content)` — wrap `content` as a draggable,
/// droppable list item. Dragging item A onto item B fires `onDrop(A, B)`; feed
/// that to `apply_reorder`. Keyed by index so the diff moves DOM cleanly.
/// `onDrop` is `Msg(int from, int to)`.
template <typename OnDrop>
inline NodeRef reorder_row(int index, OnDrop onDrop, NodeRef content){
    using Msg = decltype(onDrop(0, 0));
    return box(std::move(content))
        | draggable(std::to_string(index))       // dragged payload = my index
        | drop_arg(std::to_string(index))        // as a target, tag my index
        | on_drop([onDrop](std::string payload) -> Msg {
              auto [from, to] = parse_reorder(payload);
              return onDrop(from, to);            // note: from==-1 is a no-op in apply_reorder
          })
        | grab
        | key("ro-" + std::to_string(index));
}

} // namespace waya::ui
