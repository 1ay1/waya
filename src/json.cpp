/// \file json.cpp
/// The nlohmann-backed implementation of waya::json::Value. This is the ONLY
/// translation unit that includes nlohmann/json — the 25k-line header stays out
/// of every consumer's compile. All of Value's behaviour lives here behind the
/// PImpl declared in <waya/json.hpp>.

#include "waya/json.hpp"

// The bundled nlohmann/json is third-party; silence its warnings so waya can
// build -Werror clean. (Its C++26 use of std::is_trivial is deprecation-warned
// by libstdc++, etc.)
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#  pragma GCC diagnostic ignored "-Wdeprecated"
#endif
#include "waya/third_party/nlohmann/json.hpp"
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

namespace waya::json {

using njson = ::nlohmann::json;

// The hidden state: the JSON value plus the caches that back the by-reference
// accessors (operator[], arr(), items()). Kept out of the header so no consumer
// TU sees nlohmann.
struct Value::Impl {
    njson j;
    mutable std::shared_ptr<Value>       last;   // backing for operator[] refs
    mutable std::vector<Value>           arr;    // backing for arr()
    mutable std::map<std::string, Value> items;  // backing for items()

    Impl() : j(nullptr) {}
    explicit Impl(njson v) : j(std::move(v)) {}
};

// ── special members (PImpl needs them out-of-line) ──────────────────────────
Value::Value() : p_(std::make_unique<Impl>()) {}
Value::~Value() = default;
Value::Value(std::unique_ptr<Impl> p) : p_(std::move(p)) {}
Value::Value(const Value& o) : p_(std::make_unique<Impl>(o.p_->j)) {}
Value::Value(Value&& o) noexcept = default;
Value& Value::operator=(const Value& o) {
    if (this != &o) p_ = std::make_unique<Impl>(o.p_->j);
    return *this;
}
Value& Value::operator=(Value&& o) noexcept = default;

// A shared Null returned by reference for missing/mismatched lookups.
static const Value& null_ref() { static const Value n; return n; }

// ── type / predicates ───────────────────────────────────────────────────────
Type Value::type() const {
    switch (p_->j.type()) {
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
bool Value::is_null()   const { return p_->j.is_null(); }
bool Value::is_object() const { return p_->j.is_object(); }
bool Value::is_array()  const { return p_->j.is_array(); }
Value::operator bool()  const { return !p_->j.is_null(); }

// ── accessors ───────────────────────────────────────────────────────────────
bool        Value::as_bool(bool d)     const { return p_->j.is_boolean() ? p_->j.get<bool>() : d; }
double      Value::as_double(double d)  const { return p_->j.is_number()  ? p_->j.get<double>() : d; }
int         Value::as_int(int d)        const { return p_->j.is_number()  ? (int)p_->j.get<double>() : d; }
long long   Value::as_long(long long d) const { return p_->j.is_number()  ? (long long)p_->j.get<double>() : d; }
std::string Value::str(std::string d)   const { return p_->j.is_string()  ? p_->j.get<std::string>() : std::move(d); }

std::size_t Value::size() const { return (p_->j.is_array() || p_->j.is_object()) ? p_->j.size() : 0; }

const std::vector<Value>& Value::arr() const {
    p_->arr.clear();
    if (p_->j.is_array()) { p_->arr.reserve(p_->j.size()); for (auto& e : p_->j) p_->arr.push_back(Value(std::make_unique<Impl>(e))); }
    return p_->arr;
}

const Value& Value::operator[](std::string_view key) const {
    if (!p_->j.is_object()) return null_ref();
    auto it = p_->j.find(std::string(key));
    if (it == p_->j.end()) return null_ref();
    p_->last = std::make_shared<Value>(Value(std::make_unique<Impl>(*it)));
    return *p_->last;
}
const Value& Value::operator[](std::size_t i) const {
    if (!p_->j.is_array() || i >= p_->j.size()) return null_ref();
    p_->last = std::make_shared<Value>(Value(std::make_unique<Impl>(p_->j[i])));
    return *p_->last;
}
bool Value::has(std::string_view key) const {
    return p_->j.is_object() && p_->j.find(std::string(key)) != p_->j.end();
}
const std::map<std::string, Value>& Value::items() const {
    p_->items.clear();
    if (p_->j.is_object())
        for (auto it = p_->j.begin(); it != p_->j.end(); ++it)
            p_->items.emplace(it.key(), Value(std::make_unique<Impl>(it.value())));
    return p_->items;
}

// ── builders ────────────────────────────────────────────────────────────────
Value Value::null()                { return Value(std::make_unique<Impl>(njson(nullptr))); }
Value Value::boolean(bool v)       { return Value(std::make_unique<Impl>(njson(v))); }
Value Value::number(double v)      { return Value(std::make_unique<Impl>(njson(v))); }
Value Value::string(std::string v) { return Value(std::make_unique<Impl>(njson(std::move(v)))); }
Value Value::array(std::vector<Value> v) {
    njson a = njson::array();
    for (auto& e : v) a.push_back(e.p_->j);
    return Value(std::make_unique<Impl>(std::move(a)));
}
Value Value::object(std::map<std::string, Value> v) {
    njson o = njson::object();
    for (auto& [k, val] : v) o[k] = val.p_->j;
    return Value(std::make_unique<Impl>(std::move(o)));
}

void Value::push(Value v) {
    if (!p_->j.is_array()) p_->j = njson::array();
    p_->j.push_back(std::move(v.p_->j));
}
void Value::set(std::string k, Value v) {
    if (!p_->j.is_object()) p_->j = njson::object();
    p_->j[std::move(k)] = std::move(v.p_->j);
}

// ── serialize ───────────────────────────────────────────────────────────────
std::string Value::dump(int indent) const { return p_->j.dump(indent); }

// ── parse ───────────────────────────────────────────────────────────────────
Value parse(std::string_view text) {
    njson j = njson::parse(text, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return Value::null();
    return Value(std::make_unique<Value::Impl>(std::move(j)));
}

} // namespace waya::json
