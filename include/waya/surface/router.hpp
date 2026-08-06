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
    std::unordered_map<std::string, std::string> query;  ///< parsed ?a=1&b=2 pairs
    /// Convenience: a param by name (empty if absent).
    std::string param(const std::string& k) const {
        auto it = params.find(k); return it == params.end() ? std::string{} : it->second;
    }
    /// A query-string value by key (empty if absent). So `?page=2&sort=name`
    /// reads as `m.q("page")` / `m.q("sort")` — no manual path parsing, ever.
    std::string q(const std::string& k) const {
        auto it = query.find(k); return it == query.end() ? std::string{} : it->second;
    }
    /// Does the query carry this key at all (distinguishes `?x=` from absent)?
    bool has_q(const std::string& k) const { return query.count(k) > 0; }
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

    /// Match a path. The query string is PARSED (into `m.query`, read via
    /// `m.q("key")`) and the trailing slash is ignored. Returns the first route
    /// whose pattern fits, with any `:name`/`*` captures filled in.
    [[nodiscard]] Match match(std::string path) const {
        auto qmap = parse_query(path);        // pull ?a=1&b=2 before stripping
        path = strip(path);
        auto segs = split(path);
        for (const auto& r : routes_) {
            Match m; m.value = r.value; m.query = qmap;
            if (fits(r.pattern, segs, m.params)) { m.matched = true; return m; }
        }
        Match none; none.query = std::move(qmap);
        return none;   // no route matched, but the query is still available
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
    /// Parse the `?a=1&b=2` portion of a path into a decoded key→value map. Same
    /// wire format as an HTML form body, so `?` params and posted fields read
    /// the same way. Percent- and `+`-decoded.
    static std::unordered_map<std::string, std::string> parse_query(const std::string& path) {
        std::unordered_map<std::string, std::string> out;
        auto q = path.find('?');
        if (q == std::string::npos) return out;
        std::string qs = path.substr(q + 1);
        if (auto h = qs.find('#'); h != std::string::npos) qs.erase(h);   // drop fragment
        std::size_t i = 0;
        while (i < qs.size()) {
            std::size_t amp = qs.find('&', i); if (amp == std::string::npos) amp = qs.size();
            std::string pair = qs.substr(i, amp - i);
            std::size_t eq = pair.find('=');
            std::string k = url_decode(eq == std::string::npos ? pair : pair.substr(0, eq));
            std::string v = eq == std::string::npos ? std::string{} : url_decode(pair.substr(eq + 1));
            if (!k.empty()) out[std::move(k)] = std::move(v);
            i = amp + 1;
        }
        return out;
    }
    static std::string url_decode(const std::string& s) {
        std::string o; o.reserve(s.size());
        auto hex = [](char c)->int{ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10;
                                    if(c>='A'&&c<='F')return c-'A'+10; return 0; };
        for (std::size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '+') o += ' ';
            else if (c == '%' && i + 2 < s.size()) { o += (char)(hex(s[i+1])*16 + hex(s[i+2])); i += 2; }
            else o += c;
        }
        return o;
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
