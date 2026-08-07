#pragma once
/// \file assets.hpp
/// The document-level asset registry — the core's seam for everything that lives
/// in <head> instead of on a node: @keyframes, @font-face, :root design tokens,
/// global CSS rules (::selection, scrollbars, resets), and raw <head> markup
/// (favicons, <link> to a font CDN, analytics).
///
/// WHY THIS EXISTS. Per-node styling (the `Mod` vocabulary) can express any
/// element, but some things are inherently document-scoped: a web font must be
/// declared once, a custom keyframe can't hang off a single node, `:root`
/// custom properties are global by definition. Without this seam a component
/// could not ship its own font or animation — the app author would have to know
/// its internals and wire them up by hand. With it, a component just registers
/// what it needs the first time it's used, and the shell folds it into <head>.
///
///   using namespace waya::surface;
///   assets().font_face("Inter", "/fonts/Inter.woff2");   // @font-face
///   assets().root_var("--brand", "#6366f1");             // :root token
///   assets().keyframes("wobble", "0%{transform:none}50%{transform:rotate(6deg)}100%{transform:none}");
///   assets().css("::selection{background:#6366f1;color:#fff}");
///   assets().head("<link rel=\"icon\" href=\"/favicon.svg\">");
///
/// Everything is DEDUPED by key (name for keyframes/fonts/vars, the full text
/// for css/head), so registering the same asset from a component used 100 times
/// emits it exactly once. The registry is process-global, write-once config —
/// the same shape as the built-in motion library — so a component defined deep
/// in a header can contribute an asset without threading state through view().
///
/// This is what makes the core CLOSED under "build anything": paired with the
/// per-node escape hatches (css/attr/var/as/markup/states), there is no piece
/// of a web document waya cannot express from userland.

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace waya::surface {

/// A process-global collector for document-level assets. Thread-safe; reads and
/// writes are guarded so a component registering during a render on one
/// connection can't race the shell assembling <head> on another.
class Assets {
public:
    static Assets& instance() { static Assets a; return a; }

    /// Register an @keyframes block by NAME. `spec` is the body between the
    /// braces, e.g. "0%{opacity:0}100%{opacity:1}". Deduped by name — the first
    /// registration wins, later ones with the same name are ignored (so a
    /// component's canonical definition is stable). Prefix custom names to avoid
    /// clashing with the built-in `wa-*` library.
    Assets& keyframes(const std::string& name, const std::string& spec) {
        std::lock_guard<std::mutex> l(m_);
        if (!has(keyframes_, name)) keyframes_.push_back({name, spec});
        return *this;
    }

    /// Register an @font-face. `src` is a single URL; the format is inferred
    /// from the extension (woff2/woff/ttf/otf). For finer control (weight,
    /// display, multiple sources) use `css(...)` with your own @font-face.
    Assets& font_face(const std::string& family, const std::string& src,
                      const std::string& weight = "400", const std::string& style = "normal") {
        std::string fmt = "woff2";
        auto dot = src.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = src.substr(dot + 1);
            if (ext == "woff") fmt = "woff"; else if (ext == "ttf") fmt = "truetype";
            else if (ext == "otf") fmt = "opentype"; else if (ext == "woff2") fmt = "woff2";
        }
        std::string body = "@font-face{font-family:'" + family + "';src:url('" + src +
            "') format('" + fmt + "');font-weight:" + weight + ";font-style:" + style +
            ";font-display:swap}";
        return raw_css(family + "|" + src + "|" + weight + "|" + style, std::move(body));
    }

    /// Register a `:root` custom property (a design token). Deduped by name; the
    /// LAST value wins (so an app can override a component-library default).
    Assets& root_var(const std::string& name, const std::string& value) {
        std::lock_guard<std::mutex> l(m_);
        for (auto& v : root_vars_) if (v.first == name) { v.second = value; return *this; }
        root_vars_.push_back({name, value});
        return *this;
    }

    /// Register a raw global CSS rule (any selector: ::selection, a scrollbar,
    /// a reset override, an @media block). Deduped by exact text.
    Assets& css(const std::string& rule) { return raw_css(rule, rule); }

    /// Register raw <head> markup (a <link>, <meta>, favicon, third-party
    /// <script>). Deduped by exact text. NOT escaped — trusted author content.
    Assets& head(const std::string& html) {
        std::lock_guard<std::mutex> l(m_);
        if (std::find(head_.begin(), head_.end(), html) == head_.end()) head_.push_back(html);
        return *this;
    }

    /// Assemble every registered CSS asset into one <style> body (keyframes +
    /// :root vars + font-faces + global rules). Read by the shell.
    std::string style_css() const {
        std::lock_guard<std::mutex> l(m_);
        std::string o;
        if (!root_vars_.empty()) {
            o += ":root{";
            for (auto& [k, v] : root_vars_) { o += k; o += ':'; o += v; o += ';'; }
            o += '}';
        }
        for (auto& [name, spec] : keyframes_) { o += "@keyframes "; o += name; o += '{'; o += spec; o += '}'; }
        for (auto& [key, body] : css_) { (void)key; o += body; }
        return o;
    }

    /// Assemble every registered raw <head> asset. Read by the shell.
    std::string head_html() const {
        std::lock_guard<std::mutex> l(m_);
        std::string o;
        for (auto& h : head_) o += h;
        return o;
    }

    /// Clear everything (tests / a fresh server run in-process).
    void clear() {
        std::lock_guard<std::mutex> l(m_);
        keyframes_.clear(); root_vars_.clear(); css_.clear(); head_.clear();
    }

private:
    Assets() = default;
    static bool has(const std::vector<std::pair<std::string,std::string>>& v, const std::string& k) {
        for (auto& e : v) { if (e.first == k) return true; }
        return false;
    }
    Assets& raw_css(const std::string& key, std::string body) {
        std::lock_guard<std::mutex> l(m_);
        if (!has(css_, key)) css_.push_back({key, std::move(body)});
        return *this;
    }

    mutable std::mutex m_;
    std::vector<std::pair<std::string,std::string>> keyframes_;   // name → body
    std::vector<std::pair<std::string,std::string>> root_vars_;   // --name → value
    std::vector<std::pair<std::string,std::string>> css_;         // dedup-key → rule body
    std::vector<std::string> head_;                               // raw <head> html
};

/// `assets()` — the process-global asset registry. Register document-level
/// keyframes / fonts / :root tokens / global CSS / <head> markup on it; the
/// live shell folds them into every served page's <head>.
inline Assets& assets() { return Assets::instance(); }

} // namespace waya::surface
