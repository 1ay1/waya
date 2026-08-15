#pragma once
/// \file ui/infinite_scroll.hpp
/// Paged<T> + infinite_scroll — accumulate pages, load more on scroll.
///
/// The pieces exist: `on_appear` fires a Msg when a sentinel scrolls into view,
/// and `RemoteData` models one fetch. Infinite scroll is those two combined:
/// keep a growing list of items, fetch the next page when the user nears the
/// bottom, and stop when the server says there are no more. `Paged<T>` holds
/// that state as one value.
///
///   struct Model { Paged<Post> feed; };
///
///   // update:
///   [&](LoadMore)   { if (!m.feed.can_load()) return {m, Cmd::none()};
///                     m.feed.begin_load();
///                     return {m, fetchPage(m.feed.next_page())}; }
///   [&](Loaded r)   { m.feed.append(parse_items(r.body), has_more(r)); return {m, Cmd::none()}; }
///
///   // view: the items, then a sentinel that triggers the next page
///   col(
///     col_(map(m.feed.items(), post_card)),
///     infinite_sentinel(m.feed, LoadMore{}))      // spinner while loading, nothing when done
///
/// `begin_load` marks a fetch in flight (so a fast scroll doesn't fire ten
/// requests); `append(items, has_more)` adds a page and records whether to keep
/// going; `can_load()` is false while loading or once exhausted. The sentinel
/// renders a spinner while loading and disappears when there's nothing left.

#include "../surface/node.hpp"
#include "components.hpp"

#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// A growing, page-loaded list. `T` is one item.
template <typename T>
struct Paged {
    std::vector<T> loaded;      // all items fetched so far, in order
    int page = 0;               // pages fetched (== next page index to request)
    bool loading = false;       // a fetch is in flight
    bool exhausted = false;     // the server reported no more pages
    std::string error;          // last fetch error (empty = ok)

    bool operator==(const Paged&) const = default;

    /// The accumulated items (what the view renders).
    [[nodiscard]] const std::vector<T>& items() const { return loaded; }
    [[nodiscard]] std::size_t size() const { return loaded.size(); }
    [[nodiscard]] bool empty() const { return loaded.empty(); }

    /// The page index to request next (0-based).
    [[nodiscard]] int next_page() const { return page; }
    /// True when a `LoadMore` should actually fetch: not already loading, and
    /// not exhausted. Guard your fetch on this so the sentinel can't stampede.
    [[nodiscard]] bool can_load() const { return !loading && !exhausted; }
    [[nodiscard]] bool is_loading() const { return loading; }
    [[nodiscard]] bool is_exhausted() const { return exhausted; }

    /// Mark a fetch as started (call before firing the Cmd).
    void begin_load(){ loading = true; error.clear(); }
    /// Append a fetched page. `has_more=false` marks the list exhausted (the
    /// sentinel then disappears). Advances the page counter.
    void append(std::vector<T> next, bool has_more){
        for (auto& x : next) loaded.push_back(std::move(x));
        ++page;
        loading = false;
        exhausted = !has_more;
    }
    /// A fetch failed: stop loading, record the error (the caller can offer retry).
    void fail(std::string msg){ loading = false; error = std::move(msg); }
    /// Start over (a fresh query / pull-to-refresh).
    void reset(){ loaded.clear(); page = 0; loading = false; exhausted = false; error.clear(); }
};

/// `infinite_sentinel(paged, onLoadMore)` — the bottom-of-list trigger. While
/// there are more pages it renders an `on_appear` sentinel (fires `onLoadMore`
/// when scrolled to) plus a spinner while a fetch is in flight; once exhausted
/// it renders nothing, so the list simply ends. Put it after your item rows.
template <typename T, typename OnLoadMore>
inline NodeRef infinite_sentinel(const Paged<T>& p, OnLoadMore onLoadMore){
    if (p.is_exhausted())
        return nothing();
    if (p.is_loading())
        return box(spinner()) | w_full | pad(20) | center;
    // A thin sentinel row: appearing in the viewport fires the next-page load.
    // Keyed by page so a fresh sentinel re-arms after each page (on_appear fires
    // once per DOM element; a new key = a new element = a new trigger).
    return box() | h(1) | w_full | on_appear(onLoadMore) | key("infinite-" + std::to_string(p.next_page()));
}

} // namespace waya::ui
