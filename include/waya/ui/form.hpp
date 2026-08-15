#pragma once
/// \file ui/form.hpp
/// Form<F> — a whole form's state and validity as ONE value.
///
/// Per-field errors (added in patterns.hpp) fix ONE control. But a form is more:
/// which fields the user has touched (so you don't scream errors at an untouched
/// field), whether the WHOLE form is valid (to enable/disable submit), and
/// running the validators in one place. Hand-rolled that's a pile of parallel
/// maps — values, errors, touched flags — kept in sync by hand.
///
/// `Form<F>` folds them into one value over your own field-key enum/strings:
///
///   struct Model { Form<> signup; };
///   // wire fields to it in update:
///   [&](Edit e) { m.signup.set(e.field, e.value); return {m, Cmd::none()}; }
///   [&](Submit) { m.signup.touch_all();
///                 if (!m.signup.validate(rules).valid()) return {m, Cmd::none()};
///                 return {m, postSignup(m.signup.values())}; }
///
///   // view: read a field's value + its (touched-gated) error
///   text_field("Email", f.get("email"), Edit{"email"}, "", "",
///              "email", f.error_for("email"))
///   button("Sign up", Submit{}) | when_(!f.valid(), disabled())
///
/// A "rule" is `std::function<std::string(const Form&)>` returning an error
/// message for a field (empty = ok), so validation is pure data you can unit
/// test. Errors only SHOW for touched fields — the standard "don't nag before
/// they've typed" UX — but `touch_all()` on submit reveals every problem at once.

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace waya::ui {

/// A form over string-keyed fields. `F` is reserved for a future typed-field
/// variant; the default keys fields by name (the pragmatic 95%).
template <typename F = void>
struct Form {
    std::map<std::string, std::string> fields;   // field key -> current value
    std::map<std::string, std::string> errors;   // field key -> error (empty/absent = ok)
    std::map<std::string, bool>        touched;   // field key -> user has interacted

    bool operator==(const Form&) const = default;

    // ── values ──────────────────────────────────────────────────────────────
    /// Read a field's value ("" if unset).
    [[nodiscard]] std::string get(const std::string& key) const {
        auto it = fields.find(key); return it==fields.end() ? std::string{} : it->second;
    }
    /// Set a field's value AND mark it touched (a keystroke both edits + touches).
    void set(const std::string& key, std::string value){
        fields[key] = std::move(value); touched[key] = true;
    }
    /// Set without marking touched (a programmatic prefill).
    void preset(const std::string& key, std::string value){ fields[key] = std::move(value); }
    /// All field values, for building the request body.
    [[nodiscard]] const std::map<std::string,std::string>& values() const { return fields; }

    // ── touched ──────────────────────────────────────────────────────────────
    [[nodiscard]] bool is_touched(const std::string& key) const {
        auto it = touched.find(key); return it!=touched.end() && it->second;
    }
    /// Reveal every field's error at once (call on a submit attempt).
    void touch_all(){ for (auto& [k, _] : fields) touched[k] = true;
                      for (auto& [k, _] : errors) touched[k] = true; }
    void reset_touched(){ touched.clear(); }

    // ── validation ────────────────────────────────────────────────────────────
    /// A rule: given the whole form, return an error for `its` field (empty = ok).
    using Rule = std::function<std::string(const Form&)>;
    /// Run `rules` (field key -> rule), storing each result. Returns *this so you
    /// can chain `.validate(rules).valid()`. Pure: same form + rules -> same errors.
    Form& validate(const std::vector<std::pair<std::string, Rule>>& rules){
        errors.clear();
        for (auto& [key, rule] : rules){
            std::string e = rule(*this);
            if (!e.empty()) errors[key] = std::move(e);
        }
        return *this;
    }
    /// The raw error for a field (regardless of touched) \u2014 for custom rendering.
    [[nodiscard]] std::string raw_error(const std::string& key) const {
        auto it = errors.find(key); return it==errors.end() ? std::string{} : it->second;
    }
    /// The error to SHOW for a field: its error, but only once touched. Feed this
    /// straight to `text_field(..., f.error_for("email"))`.
    [[nodiscard]] std::string error_for(const std::string& key) const {
        return is_touched(key) ? raw_error(key) : std::string{};
    }
    /// True when no field has an error (run validate() first). Gate submit on it.
    [[nodiscard]] bool valid() const { return errors.empty(); }
    [[nodiscard]] std::size_t error_count() const { return errors.size(); }
};

// ── ready-made rules ─────────────────────────────────────────────────────────
// Compose these into your `{key, rule}` list. Each captures the field key so it
// reads its own value out of the form.
namespace rules {
/// The field must be non-empty (after trimming leading/trailing spaces).
inline Form<>::Rule required(std::string key, std::string msg = "Required"){
    return [key, msg](const Form<>& f){
        std::string v = f.get(key);
        std::size_t a = v.find_first_not_of(" \t");
        return a==std::string::npos ? msg : std::string{};
    };
}
/// The field must look like an email (a single '@' with text either side).
inline Form<>::Rule email(std::string key, std::string msg = "Enter a valid email"){
    return [key, msg](const Form<>& f){
        std::string v = f.get(key);
        auto at = v.find('@');
        bool ok = at!=std::string::npos && at>0 && at+1<v.size() && v.find('@', at+1)==std::string::npos
                  && v.find('.', at)!=std::string::npos;
        return ok ? std::string{} : msg;
    };
}
/// At least `n` characters.
inline Form<>::Rule min_len(std::string key, std::size_t n, std::string msg = {}){
    return [key, n, msg](const Form<>& f){
        if (f.get(key).size() >= n) return std::string{};
        return msg.empty() ? ("Must be at least " + std::to_string(n) + " characters") : msg;
    };
}
/// Two fields must match (password confirmation).
inline Form<>::Rule matches(std::string key, std::string other, std::string msg = "Doesn't match"){
    return [key, other, msg](const Form<>& f){
        return f.get(key) == f.get(other) ? std::string{} : msg;
    };
}
} // namespace rules

} // namespace waya::ui
