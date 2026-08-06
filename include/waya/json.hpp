#pragma once
/// \file json.hpp
/// A tiny, dependency-free JSON parser + builder. Real apps live on JSON — an
/// API response from Cmd::fetch, a request body to POST, a config blob. This is
/// enough to read and write it ergonomically without pulling in a library.
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
/// Not a spec-perfect implementation (no \u surrogate pairs, no bignum), but
/// correct for the JSON real APIs actually emit. Values are copyable.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace waya::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_array() const { return type_ == Type::Array; }
    explicit operator bool() const { return type_ != Type::Null; }

    // ── accessors (safe: wrong-type or missing returns a default) ────────────
    bool        as_bool(bool d = false) const { return type_ == Type::Bool ? b_ : d; }
    double      as_double(double d = 0) const { return type_ == Type::Number ? n_ : d; }
    int         as_int(int d = 0) const { return type_ == Type::Number ? (int)n_ : d; }
    long long   as_long(long long d = 0) const { return type_ == Type::Number ? (long long)n_ : d; }
    std::string str(std::string d = {}) const { return type_ == Type::String ? s_ : d; }

    const std::vector<Value>& arr() const { static const std::vector<Value> e; return type_ == Type::Array ? *a_ : e; }
    std::size_t size() const { return type_ == Type::Array ? a_->size() : (type_ == Type::Object ? o_->size() : 0); }

    /// `j["key"]` — object member (Null if absent/not-object).
    const Value& operator[](std::string_view key) const {
        static const Value null;
        if (type_ != Type::Object) return null;
        auto it = o_->find(std::string(key));
        return it == o_->end() ? null : it->second;
    }
    /// `j[i]` — array element (Null if out of range/not-array).
    const Value& operator[](std::size_t i) const {
        static const Value null;
        if (type_ != Type::Array || i >= a_->size()) return null;
        return (*a_)[i];
    }
    bool has(std::string_view key) const {
        return type_ == Type::Object && o_->find(std::string(key)) != o_->end();
    }
    const std::map<std::string, Value>& items() const {
        static const std::map<std::string, Value> e; return type_ == Type::Object ? *o_ : e;
    }

    // ── builders ─────────────────────────────────────────────────────────────
    static Value null() { return Value(); }
    static Value boolean(bool v) { Value x; x.type_ = Type::Bool; x.b_ = v; return x; }
    static Value number(double v) { Value x; x.type_ = Type::Number; x.n_ = v; return x; }
    static Value string(std::string v) { Value x; x.type_ = Type::String; x.s_ = std::move(v); return x; }
    static Value array(std::vector<Value> v = {}) { Value x; x.type_ = Type::Array; x.a_ = std::make_shared<std::vector<Value>>(std::move(v)); return x; }
    static Value object(std::map<std::string, Value> v = {}) { Value x; x.type_ = Type::Object; x.o_ = std::make_shared<std::map<std::string, Value>>(std::move(v)); return x; }

    void push(Value v) { if (type_ != Type::Array) { type_ = Type::Array; a_ = std::make_shared<std::vector<Value>>(); } a_->push_back(std::move(v)); }
    void set(std::string k, Value v) { if (type_ != Type::Object) { type_ = Type::Object; o_ = std::make_shared<std::map<std::string, Value>>(); } (*o_)[std::move(k)] = std::move(v); }

    // ── serialize ────────────────────────────────────────────────────────────
    std::string dump() const { std::string o; write(o); return o; }

private:
    void write(std::string& o) const {
        switch (type_) {
            case Type::Null: o += "null"; break;
            case Type::Bool: o += b_ ? "true" : "false"; break;
            case Type::Number: {
                if (n_ == (long long)n_) o += std::to_string((long long)n_);
                else { std::string s = std::to_string(n_); o += s; }
                break; }
            case Type::String: write_string(o, s_); break;
            case Type::Array: {
                o += '['; bool first = true;
                for (auto& e : *a_) { if (!first) o += ','; first = false; e.write(o); }
                o += ']'; break; }
            case Type::Object: {
                o += '{'; bool first = true;
                for (auto& [k, v] : *o_) { if (!first) o += ','; first = false; write_string(o, k); o += ':'; v.write(o); }
                o += '}'; break; }
        }
    }
    static void write_string(std::string& o, const std::string& s) {
        o += '"';
        for (char c : s) switch (c) {
            case '"': o += "\\\""; break; case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break; case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default: if ((unsigned char)c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; } else o += c;
        }
        o += '"';
    }

    Type type_;
    bool b_ = false;
    double n_ = 0;
    std::string s_;
    std::shared_ptr<std::vector<Value>> a_;
    std::shared_ptr<std::map<std::string, Value>> o_;

    friend class Parser;
};

class Parser {
public:
    explicit Parser(std::string_view s) : s_(s) {}
    Value parse() { ws(); Value v = value(); return v; }
    bool ok() const { return ok_; }

private:
    std::string_view s_; std::size_t i_ = 0; bool ok_ = true;

    void ws() { while (i_ < s_.size() && (s_[i_]==' '||s_[i_]=='\t'||s_[i_]=='\n'||s_[i_]=='\r')) ++i_; }
    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }

    Value value() {
        ws();
        switch (peek()) {
            case '{': return object();
            case '[': return array();
            case '"': return Value::string(string());
            case 't': case 'f': return boolean();
            case 'n': lit("null"); return Value::null();
            default: return number();
        }
    }
    Value object() {
        Value o = Value::object(); ++i_; ws();
        if (peek() == '}') { ++i_; return o; }
        for (;;) {
            ws(); if (peek() != '"') { ok_ = false; return o; }
            std::string k = string(); ws();
            if (peek() != ':') { ok_ = false; return o; } ++i_;
            o.set(std::move(k), value()); ws();
            if (peek() == ',') { ++i_; continue; }
            if (peek() == '}') { ++i_; break; }
            ok_ = false; break;
        }
        return o;
    }
    Value array() {
        Value a = Value::array(); ++i_; ws();
        if (peek() == ']') { ++i_; return a; }
        for (;;) {
            a.push(value()); ws();
            if (peek() == ',') { ++i_; continue; }
            if (peek() == ']') { ++i_; break; }
            ok_ = false; break;
        }
        return a;
    }
    std::string string() {
        std::string o; ++i_;  // opening quote
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return o;
            if (c == '\\' && i_ < s_.size()) {
                char e = s_[i_++];
                switch (e) {
                    case 'n': o += '\n'; break; case 't': o += '\t'; break;
                    case 'r': o += '\r'; break; case 'b': o += '\b'; break;
                    case 'f': o += '\f'; break; case '/': o += '/'; break;
                    case '"': o += '"'; break; case '\\': o += '\\'; break;
                    case 'u': {
                        if (i_ + 4 <= s_.size()) {
                            unsigned cp = 0;
                            for (int k = 0; k < 4; ++k) { char h = s_[i_++]; cp = cp*16 + (h<='9'?h-'0':(h|32)-'a'+10); }
                            // encode BMP code point as UTF-8 (surrogate pairs unhandled)
                            if (cp < 0x80) o += (char)cp;
                            else if (cp < 0x800) { o += (char)(0xC0|(cp>>6)); o += (char)(0x80|(cp&0x3F)); }
                            else { o += (char)(0xE0|(cp>>12)); o += (char)(0x80|((cp>>6)&0x3F)); o += (char)(0x80|(cp&0x3F)); }
                        }
                        break; }
                    default: o += e;
                }
            } else o += c;
        }
        ok_ = false; return o;
    }
    Value boolean() {
        if (peek() == 't') { lit("true"); return Value::boolean(true); }
        lit("false"); return Value::boolean(false);
    }
    Value number() {
        std::size_t start = i_;
        if (peek() == '-') ++i_;
        while (i_ < s_.size() && ((s_[i_]>='0'&&s_[i_]<='9')||s_[i_]=='.'||s_[i_]=='e'||s_[i_]=='E'||s_[i_]=='+'||s_[i_]=='-')) ++i_;
        if (i_ == start) { ok_ = false; return Value::null(); }
        return Value::number(std::strtod(std::string(s_.substr(start, i_-start)).c_str(), nullptr));
    }
    void lit(const char* w) { for (std::size_t k = 0; w[k]; ++k) { if (peek() != w[k]) { ok_ = false; return; } ++i_; } }
};

/// `json::parse(text)` — parse JSON into a Value (Null on syntax error).
inline Value parse(std::string_view text) { Parser p(text); Value v = p.parse(); return p.ok() ? v : Value::null(); }

// convenience free builders
inline Value null() { return Value::null(); }
inline Value boolean(bool v) { return Value::boolean(v); }
inline Value number(double v) { return Value::number(v); }
inline Value string(std::string v) { return Value::string(std::move(v)); }
inline Value array(std::vector<Value> v = {}) { return Value::array(std::move(v)); }
inline Value object(std::map<std::string, Value> v = {}) { return Value::object(std::move(v)); }

} // namespace waya::json
