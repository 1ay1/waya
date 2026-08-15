#pragma once
/// \file ui/i18n.hpp
/// A tiny internationalization layer: message catalogs + t(key).
///
/// A waya view is a pure function of state, so translation is just a lookup the
/// view does: swap the active catalog and every `t("save")` renders in the new
/// language on the next frame — no framework magic, because the whole UI is
/// already rebuilt from the model each frame.
///
///   Catalog en = catalog({
///       {"greeting", "Hello, {name}!"},
///       {"items",    "{n} item|{n} items"},      // singular | plural
///       {"save",     "Save"},
///   });
///   Catalog fr = catalog({
///       {"greeting", "Bonjour, {name} !"},
///       {"save",     "Enregistrer"},
///   });
///
///   // hold the active catalog in your model (or a thread-local for the render)
///   text(en.t("greeting", {{"name", user.name}}));   // "Hello, Ada!"
///   text(en.plural("items", cart.size()));           // "3 items"
///   text(en.t("save"));                              // "Save"
///
/// Interpolation replaces `{name}` from the argument map. `plural(key, n)` picks
/// the `singular|plural` arm by |n|==1 and substitutes `{n}`. A missing key
/// falls back to the key itself (so an untranslated string is visible, not
/// blank) — and `Catalog` can chain to a FALLBACK catalog (e.g. English) so a
/// partially-translated locale degrades gracefully.

#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace waya::ui {

/// A message catalog for one locale.
class Catalog {
public:
    using Args = std::vector<std::pair<std::string, std::string>>;

    Catalog() = default;
    explicit Catalog(std::unordered_map<std::string, std::string> msgs)
        : messages_(std::move(msgs)) {}

    /// Fall back to `other` (usually the default locale) for missing keys.
    /// Returns *this so you can chain `fr.fallback(en)`.
    Catalog& fallback(const Catalog& other){ fallback_ = std::make_shared<Catalog>(other); return *this; }

    /// Look up `key`, interpolating `{name}` placeholders from `args`. A missing
    /// key tries the fallback catalog, then renders the KEY itself (visible, not
    /// blank) so an untranslated string is obvious in the UI.
    [[nodiscard]] std::string t(const std::string& key, const Args& args = {}) const {
        const std::string* raw = find(key);
        std::string out = raw ? *raw : key;
        return interpolate(out, args);
    }

    /// Pluralize: the value is `"singular|plural"`; pick by |n|==1, substitute
    /// `{n}` with the count, plus any extra `args`. If there's no `|`, the whole
    /// string is used for both (with `{n}` still substituted).
    [[nodiscard]] std::string plural(const std::string& key, long n, const Args& extra = {}) const {
        const std::string* raw = find(key);
        std::string src = raw ? *raw : key;
        std::string chosen = src;
        auto bar = src.find('|');
        if (bar != std::string::npos)
            chosen = (n == 1 || n == -1) ? src.substr(0, bar) : src.substr(bar + 1);
        Args a = extra; a.push_back({ "n", std::to_string(n) });
        return interpolate(chosen, a);
    }

    /// True if the key exists in this catalog or its fallback chain.
    [[nodiscard]] bool has(const std::string& key) const { return find(key) != nullptr; }
    [[nodiscard]] std::size_t size() const { return messages_.size(); }

private:
    std::unordered_map<std::string, std::string> messages_;
    std::shared_ptr<Catalog> fallback_;

    const std::string* find(const std::string& key) const {
        auto it = messages_.find(key);
        if (it != messages_.end()) return &it->second;
        return fallback_ ? fallback_->find(key) : nullptr;
    }
    static std::string interpolate(const std::string& tpl, const Args& args){
        if (args.empty() || tpl.find('{') == std::string::npos) return tpl;
        std::string out; out.reserve(tpl.size());
        for (std::size_t i = 0; i < tpl.size(); ){
            if (tpl[i] == '{'){
                auto close = tpl.find('}', i);
                if (close != std::string::npos){
                    std::string name = tpl.substr(i + 1, close - i - 1);
                    const std::string* v = nullptr;
                    for (auto& [k, val] : args) if (k == name){ v = &val; break; }
                    if (v){ out += *v; i = close + 1; continue; }
                }
            }
            out += tpl[i++];
        }
        return out;
    }
};

/// `catalog({ {"key","value"}, … })` — build a catalog from message pairs.
inline Catalog catalog(std::initializer_list<std::pair<std::string, std::string>> msgs){
    std::unordered_map<std::string, std::string> m;
    for (auto& [k, v] : msgs) m[k] = v;
    return Catalog{ std::move(m) };
}

} // namespace waya::ui
