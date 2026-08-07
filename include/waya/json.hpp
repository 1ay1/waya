#pragma once
/// \file json.hpp
/// waya's JSON — read and write it ergonomically without a heavy dependency.
/// Real apps live on JSON: an API response from Cmd::fetch, a request body to
/// POST, a config blob.
///
///   auto j = json::parse(body);
///   std::string name = j["user"]["name"].str();
///   int age          = j["user"]["age"].as_int();
///   for (auto& item : j["items"].arr()) use(item["id"].as_int());
///
///   json::Value out = json::object({
///       {"ok", json::boolean(true)},
///       {"items", json::array({ json::number(1), json::number(2) })},
///   });
///   std::string s = out.dump();
///
/// This is a thin, ergonomic adapter over nlohmann/json (bundled, header-only)
/// — the de-facto standard modern-C++ JSON library. It is spec-complete:
/// correct number parsing, full `\uXXXX` escapes INCLUDING UTF-16 surrogate
/// pairs, and strict syntax validation (a malformed document parses to Null via
/// `json::parse`). The waya::json::Value API below is stable; the backing
/// library can change without touching your call sites.

// The bundled nlohmann/json is third-party; silence its warnings so waya can
// build -Werror clean. (Its C++26 use of std::is_trivial is deprecation-warned
// by libstdc++, etc.)
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  pragma GCC diagnostic ignored "-Wdeprecated"
#endif
#include "third_party/nlohmann/json.hpp"
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace waya::json {

using njson = ::nlohmann::json;

enum class Type { Null, Bool, Number, String, Array, Object };

/// A JSON value. Copyable; accessors are safe (wrong-type or missing returns a
/// default, never throws).
class Value {
public:
    Value() : j_(nullptr) {}
    explicit Value(njson j) : j_(std::move(j)) {}

    Type type() const {
        switch (j_.type()) {
            case njson::value_t::null:            return Type::Null;
            case njson::value_t::boolean:         return Type::Bool;
            case njson::value_t::number_integer:
            case njson::value_t::number_unsigned:
            case njson::value_t::number_float:    return Type::Number;
            case njson::value_t::string:          return Type::String;
            case njson::value_t::array:           return Type::Array;
            case njson::value_t::object:          return Type::Object;
            default:                              return Type::Null;   // binary/discarded
        }
    }
    bool is_null()   const { return j_.is_null(); }
    bool is_object() const { return j_.is_object(); }
    bool is_array()  const { return j_.is_array(); }
    explicit operator bool() const { return !j_.is_null(); }

    // ── accessors (safe: wrong-type or missing returns a default) ────────────
    bool        as_bool(bool d = false)   const { return j_.is_boolean() ? j_.get<bool>() : d; }
    double      as_double(double d = 0)   const { return j_.is_number()  ? j_.get<double>() : d; }
    int         as_int(int d = 0)         const { return j_.is_number()  ? (int)j_.get<double>() : d; }
    long long   as_long(long long d = 0)  const { return j_.is_number()  ? (long long)j_.get<double>() : d; }
    std::string str(std::string d = {})   const { return j_.is_string()  ? j_.get<std::string>() : std::move(d); }

    /// The array elements (empty if not an array). Returns a stable vector view.
    const std::vector<Value>& arr() const {
        auto& cache = ensure_arr();
        return cache;
    }
    std::size_t size() const { return (j_.is_array() || j_.is_object()) ? j_.size() : 0; }

    /// `j["key"]` — object member (Null if absent / not an object).
    const Value& operator[](std::string_view key) const {
        if (!j_.is_object()) return null_ref();
        auto it = j_.find(std::string(key));
        if (it == j_.end()) return null_ref();
        return stash(Value(*it));
    }
    /// `j[i]` — array element (Null if out of range / not an array).
    const Value& operator[](std::size_t i) const {
        if (!j_.is_array() || i >= j_.size()) return null_ref();
        return stash(Value(j_[i]));
    }
    bool has(std::string_view key) const {
        return j_.is_object() && j_.find(std::string(key)) != j_.end();
    }
    /// Object members as a map (empty if not an object).
    const std::map<std::string, Value>& items() const {
        auto& cache = ensure_items();
        return cache;
    }

    // ── builders ─────────────────────────────────────────────────────────────
    static Value null()                { return Value(njson(nullptr)); }
    static Value boolean(bool v)       { return Value(njson(v)); }
    static Value number(double v)      { return Value(njson(v)); }
    static Value string(std::string v) { return Value(njson(std::move(v))); }
    static Value array(std::vector<Value> v = {}) {
        njson a = njson::array();
        for (auto& e : v) a.push_back(e.j_);
        return Value(std::move(a));
    }
    static Value object(std::map<std::string, Value> v = {}) {
        njson o = njson::object();
        for (auto& [k, val] : v) o[k] = val.j_;
        return Value(std::move(o));
    }

    void push(Value v)               { if (!j_.is_array())  j_ = njson::array();  j_.push_back(std::move(v.j_)); }
    void set(std::string k, Value v) { if (!j_.is_object()) j_ = njson::object(); j_[std::move(k)] = std::move(v.j_); }

    // ── serialize ────────────────────────────────────────────────────────────
    /// Compact serialization; `dump(2)` pretty-prints with a 2-space indent.
    std::string dump(int indent = -1) const { return j_.dump(indent); }

    /// Escape hatch: the underlying nlohmann value (for advanced use).
    const njson& raw() const { return j_; }

private:
    // A shared Null returned by reference for missing/mismatched lookups.
    static const Value& null_ref() { static const Value n; return n; }

    // operator[] returns `const Value&`, so the returned Value must outlive the
    // call. We keep the most-recent child on the heap, owned by this node. This
    // matches how JSON is used in waya (read-then-use, single-threaded per view).
    const Value& stash(Value v) const {
        last_ = std::make_shared<Value>(std::move(v));
        return *last_;
    }
    const std::vector<Value>& ensure_arr() const {
        arr_.clear();
        if (j_.is_array()) { arr_.reserve(j_.size()); for (auto& e : j_) arr_.emplace_back(e); }
        return arr_;
    }
    const std::map<std::string, Value>& ensure_items() const {
        items_.clear();
        if (j_.is_object())
            for (auto it = j_.begin(); it != j_.end(); ++it) items_.emplace(it.key(), Value(it.value()));
        return items_;
    }

    njson j_;
    mutable std::shared_ptr<Value>        last_;    // backing for operator[] refs
    mutable std::vector<Value>            arr_;     // backing for arr()
    mutable std::map<std::string, Value>  items_;   // backing for items()
};

/// `json::parse(text)` — parse JSON into a Value (Null on syntax error).
inline Value parse(std::string_view text) {
    njson j = njson::parse(text, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return Value::null();
    return Value(std::move(j));
}

// convenience free builders
inline Value null()                { return Value::null(); }
inline Value boolean(bool v)       { return Value::boolean(v); }
inline Value number(double v)      { return Value::number(v); }
inline Value string(std::string v) { return Value::string(std::move(v)); }
inline Value array(std::vector<Value> v = {}) { return Value::array(std::move(v)); }
inline Value object(std::map<std::string, Value> v = {}) { return Value::object(std::move(v)); }

} // namespace waya::json
