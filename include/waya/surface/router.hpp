#pragma once
/// \file router.hpp
/// A tiny, dependency-free URL router so a HUGE multi-screen app doesn't become
/// a string-compare ladder in `update`/`view`. Patterns use `:name` for a param
/// and `*` for a wildcard tail:
///
///   Route r = router()
///       .at("/",                 Home)
///       .at("/users",            UserList)
///       .at("/users/:id",        UserDetail)     // captures {id}
///       .at("/users/:id/edit",   UserEdit)
///       .at("/docs/*",           Docs);          // captures the rest as {*}
///
///   auto m = r.match("/users/42");   // m.matched, m.value==UserDetail, m.params["id"]=="42"
///
/// `match` returns the app's own screen id (an int/enum) plus the extracted
/// params. Pair with `on_route` for SSR + live routing, and `screens(...)` (in
/// sugar.hpp) to render the matching screen. This is the whole routing story:
/// one table, no manual path parsing, `:params` for free.

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace waya::surface {

/// The result of matching a path against the route table.
struct Match {
    bool matched = false;
    int  value   = -1;                                   ///< the app's screen id
    std::unordered_map<std::string, std::string> params; ///< captured :name / * segments
    /// Convenience: a param by name (empty if absent).
    std::string param(const std::string& k) const {
        auto it = params.find(k); return it == params.end() ? std::string{} : it->second;
    }
};

/// A URL router: a table of (pattern -> screen id), matched in insertion order
/// (first match wins, so put specific routes before wildcards).
class Router {
public:
    /// Register a pattern. `:name` captures one segment; a trailing `*` captures
    /// the rest of the path. Returns *this so calls chain.
    Router& at(std::string pattern, int value) {
        routes_.push_back({split(std::move(pattern)), value});
        return *this;
    }

    /// Match a path (query string and trailing slash are ignored). Returns the
    /// first route whose pattern fits, with any `:name`/`*` captures filled in.
    [[nodiscard]] Match match(std::string path) const {
        path = strip(path);
        auto segs = split(path);
        for (const auto& r : routes_) {
            Match m; m.value = r.value;
            if (fits(r.pattern, segs, m.params)) { m.matched = true; return m; }
        }
        return {};   // no match
    }

    /// The number of registered routes (handy in tests).
    [[nodiscard]] std::size_t size() const { return routes_.size(); }

private:
    struct Entry { std::vector<std::string> pattern; int value; };
    std::vector<Entry> routes_;

    static std::string strip(std::string p) {
        if (auto q = p.find('?'); q != std::string::npos) p.erase(q);
        if (auto h = p.find('#'); h != std::string::npos) p.erase(h);
        while (p.size() > 1 && p.back() == '/') p.pop_back();   // ignore trailing slash
        return p;
    }
    static std::vector<std::string> split(const std::string& p) {
        std::vector<std::string> out;
        std::size_t i = 0;
        while (i < p.size()) {
            if (p[i] == '/') { ++i; continue; }
            auto j = p.find('/', i);
            if (j == std::string::npos) j = p.size();
            out.push_back(p.substr(i, j - i));
            i = j;
        }
        return out;   // "/" -> {} (empty); "/users/42" -> {"users","42"}
    }
    /// Does `pattern` fit `segs`? Fills `params` with captures.
    static bool fits(const std::vector<std::string>& pattern,
                     const std::vector<std::string>& segs,
                     std::unordered_map<std::string, std::string>& params) {
        std::size_t pi = 0, si = 0;
        for (; pi < pattern.size(); ++pi) {
            const std::string& pat = pattern[pi];
            if (pat == "*") {                       // wildcard: capture the rest
                std::string rest;
                for (; si < segs.size(); ++si) { if (!rest.empty()) rest += '/'; rest += segs[si]; }
                params["*"] = rest;
                return true;
            }
            if (si >= segs.size()) return false;    // pattern longer than path
            if (!pat.empty() && pat[0] == ':') params[pat.substr(1)] = segs[si];  // capture
            else if (pat != segs[si]) return false; // literal must match
            ++si;
        }
        return si == segs.size();                   // consumed exactly (no extra path)
    }
};

/// `router()` — start a route table. Chain `.at(pattern, id)`.
inline Router router() { return Router{}; }

} // namespace waya::surface
