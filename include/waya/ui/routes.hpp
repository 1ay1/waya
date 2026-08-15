#pragma once
/// \file ui/routes.hpp
/// Routes — one table from URL pattern to a view builder, params passed in.
///
/// `Router` (core) matches a path to a screen id + a `params` map, which you
/// then feed into a separate `screens()` switch that re-reads `m.param("id")`
/// by string. Two tables to keep in sync, and params fetched by stringly-typed
/// name far from where the route was declared.
///
/// `Routes` collapses that to ONE table: each pattern carries the builder that
/// renders it, and the builder receives the `Match` — so a route's params are
/// read right where the route is defined, and there's no screen-id enum or
/// second switch to drift.
///
///   auto pages = routes()
///       .at("/",              [](const Match&)  { return home(); })
///       .at("/users",         [](const Match&)  { return user_list(); })
///       .at("/users/:id",     [](const Match& m){ return user_detail(m.param("id")); })
///       .at("/docs/*",        [](const Match& m){ return docs(m.param("*")); })
///       .fallback(            []                 { return not_found(); });
///
///   // in view(): render whatever the current path resolves to
///   static NodeRef view(const Model& m){ return pages.view(m.path); }
///   // in subscribe(): keep m.path in sync with the URL
///   static Sub<Msg> subscribe(const Model&){ return Sub<Msg>::on_route([](std::string p){ return Nav{p}; }); }
///
/// It reuses the core `Router` for the actual pattern matching (`:name`, `*`,
/// query parsing), so params/queries behave identically — this is only the
/// pattern-to-view binding on top. `link_to(path)` (core) still makes the <a>.

#include "../surface/node.hpp"
#include "../surface/router.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

using namespace waya::surface;

/// A route table that maps URL patterns directly to view builders.
class Routes {
public:
    using View  = std::function<NodeRef(const Match&)>;
    using Plain = std::function<NodeRef()>;

    /// Register a pattern -> builder. The builder gets the `Match` (its params +
    /// query). Order matters: first match wins, so put specific before wildcard.
    Routes& at(std::string pattern, View build){
        int id = (int)builders_.size();
        router_.at(std::move(pattern), id);
        builders_.push_back(std::move(build));
        return *this;
    }
    /// Convenience: a builder that ignores params.
    Routes& at(std::string pattern, Plain build){
        return at(std::move(pattern), [b = std::move(build)](const Match&){ return b(); });
    }
    /// The view rendered when no route matches (a 404 screen). Optional \u2014 the
    /// default is an empty node.
    Routes& fallback(View build){ fallback_ = std::move(build); return *this; }
    Routes& fallback(Plain build){ fallback_ = [b = std::move(build)](const Match&){ return b(); }; return *this; }

    /// Match `path` and render its builder (with the path's params/query). If no
    /// route matches, render the fallback (or an empty node).
    [[nodiscard]] NodeRef view(const std::string& path) const {
        Match m = router_.match(path);
        if (m.matched && m.value >= 0 && (std::size_t)m.value < builders_.size())
            return builders_[m.value](m);
        if (fallback_) return fallback_(m);
        auto n = std::make_shared<Node>(); n->kind = Kind::box; finalize(*n); return n;
    }
    /// The `Match` for a path, if you need the params without rendering.
    [[nodiscard]] Match match(const std::string& path) const { return router_.match(path); }
    /// True if some route (not the fallback) matches `path`.
    [[nodiscard]] bool matches(const std::string& path) const { return router_.match(path).matched; }
    [[nodiscard]] std::size_t size() const { return builders_.size(); }

private:
    Router router_;
    std::vector<View> builders_;
    View fallback_;
};

/// `routes()` — start a fresh typed route table.
inline Routes routes(){ return Routes{}; }

} // namespace waya::ui
