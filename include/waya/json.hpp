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
/// This is a thin, ergonomic adapter over nlohmann/json. Crucially the nlohmann
/// header — 25k lines of templates — is HIDDEN behind a PImpl: it is included
/// only by src/json.cpp (compiled once into waya_runtime), never by this header.
/// A translation unit that includes <waya/json.hpp> pays for this small stable
/// API, not for all of nlohmann. The waya::json::Value API below is stable; the
/// backing library can change without touching your call sites.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace waya::json {

enum class Type { Null, Bool, Number, String, Array, Object };

/// A JSON value. Copyable; accessors are safe (wrong-type or missing returns a
/// default, never throws). All method bodies live in src/json.cpp so no consumer
/// TU sees the nlohmann backing type.
class Value {
public:
    Value();
    ~Value();
    Value(const Value&);
    Value(Value&&) noexcept;
    Value& operator=(const Value&);
    Value& operator=(Value&&) noexcept;

    Type type() const;
    bool is_null()   const;
    bool is_object() const;
    bool is_array()  const;
    explicit operator bool() const;

    // ── accessors (safe: wrong-type or missing returns a default) ────────────
    bool        as_bool(bool d = false)   const;
    double      as_double(double d = 0)   const;
    int         as_int(int d = 0)         const;
    long long   as_long(long long d = 0)  const;
    std::string str(std::string d = {})   const;

    /// The array elements (empty if not an array). Returns a stable vector view.
    const std::vector<Value>& arr() const;
    std::size_t size() const;

    /// `j["key"]` — object member (Null if absent / not an object).
    const Value& operator[](std::string_view key) const;
    /// `j[i]` — array element (Null if out of range / not an array).
    const Value& operator[](std::size_t i) const;
    bool has(std::string_view key) const;
    /// Object members as a map (empty if not an object).
    const std::map<std::string, Value>& items() const;

    // ── builders ─────────────────────────────────────────────────────────────
    static Value null();
    static Value boolean(bool v);
    static Value number(double v);
    static Value string(std::string v);
    static Value array(std::vector<Value> v = {});
    static Value object(std::map<std::string, Value> v = {});

    void push(Value v);
    void set(std::string k, Value v);

    // ── serialize ────────────────────────────────────────────────────────────
    /// Compact serialization; `dump(2)` pretty-prints with a 2-space indent.
    std::string dump(int indent = -1) const;

private:
    // The nlohmann value + the caches backing the by-reference accessors, all
    // hidden in the .cpp. Value owns one Impl on the heap.
    struct Impl;
    std::unique_ptr<Impl> p_;

    // Construct from an already-built Impl (used by parse/builders in the .cpp).
    explicit Value(std::unique_ptr<Impl> p);
    friend Value parse(std::string_view);
};

/// `json::parse(text)` — parse JSON into a Value (Null on syntax error).
Value parse(std::string_view text);

// convenience free builders
inline Value null()                { return Value::null(); }
inline Value boolean(bool v)       { return Value::boolean(v); }
inline Value number(double v)      { return Value::number(v); }
inline Value string(std::string v) { return Value::string(std::move(v)); }
inline Value array(std::vector<Value> v = {}) { return Value::array(std::move(v)); }
inline Value object(std::map<std::string, Value> v = {}) { return Value::object(std::move(v)); }

} // namespace waya::json
