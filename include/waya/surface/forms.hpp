#pragma once
/// \file forms.hpp
/// The full BROWSER FORM vocabulary as first-class builders — every native
/// input `type`, plus the structural elements (`fieldset`, `legend`, `datalist`,
/// `optgroup`, native `progress`/`meter`). The core `input()` handles free text;
/// these are the typed variants and grouping wrappers so you never hand-roll a
/// `type(...)` / `as("fieldset")` again.
///
///   number_input(m.qty)   | on_input([](std::string v){ return SetQty{v}; })
///   date_input(m.when)    | on_change([](std::string v){ return SetWhen{v}; })
///   color_input(m.tint)   | on_input(...)
///   range_input(m.vol, 0, 100)
///   fieldset("Shipping", address_fields...)
///   with_list("cities", input(v), { "Paris", "Berlin", "Tokyo" })
///
/// Each returns a real, accessible native control — the browser's own date
/// picker, colour swatch, number spinner, file dialog — styled like any node.

#include "node.hpp"
#include <string>
#include <vector>

namespace waya::surface {

// ── typed text-like inputs ──────────────────────────────────────────────────
// All are `input()` with a native `type`, so on_input/on_change and every style
// mod work exactly as on a text input.

/// `number_input(value)` — a numeric spinner. `on_input`/`on_change` carry the
/// value as a string; parse it in update.
inline NodeRef number_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "number"; return n; }
/// `number_input(value, min, max, step)` — with native constraints.
inline NodeRef number_input(std::string value, double min, double max, double step = 1){
    auto n = number_input(std::move(value));
    n->attrs.emplace_back("min", detail::numstr((float)min));
    n->attrs.emplace_back("max", detail::numstr((float)max));
    n->attrs.emplace_back("step", detail::numstr((float)step));
    return n;
}
/// `range_input(value, min, max, step)` — a slider (native range).
inline NodeRef range_input(std::string value, double min = 0, double max = 100, double step = 1){
    auto n = input(std::move(value)); n->input_type = "range";
    n->attrs.emplace_back("min", detail::numstr((float)min));
    n->attrs.emplace_back("max", detail::numstr((float)max));
    n->attrs.emplace_back("step", detail::numstr((float)step));
    return n;
}
/// `date_input` / `time_input` / `datetime_input` / `month_input` / `week_input`
/// — the browser's native pickers.
inline NodeRef date_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "date"; return n; }
inline NodeRef time_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "time"; return n; }
inline NodeRef datetime_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "datetime-local"; return n; }
inline NodeRef month_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "month"; return n; }
inline NodeRef week_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "week"; return n; }
/// `color_input(value)` — a native colour swatch/picker (value like "#6366f1").
inline NodeRef color_input(std::string value = "#000000"){ auto n = input(std::move(value)); n->input_type = "color"; return n; }
/// `password_input` / `email_input` / `tel_input` / `url_input` / `search_input`
/// — text inputs with the right semantic type (keyboard, validation, autofill).
inline NodeRef password_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "password"; return n; }
inline NodeRef email_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "email"; return n; }
inline NodeRef tel_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "tel"; return n; }
inline NodeRef url_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "url"; return n; }
inline NodeRef search_input(std::string value = {}){ auto n = input(std::move(value)); n->input_type = "search"; return n; }
/// `file_input()` — a file picker. `file_input(true)` for multiple; pass an
/// `accept` string ("image/*", ".pdf,.doc") to constrain.
inline NodeRef file_input(bool multiple = false, std::string accept = {}){
    auto n = input({}); n->input_type = "file";
    if (multiple) n->attrs.emplace_back("multiple", "");
    if (!accept.empty()) n->attrs.emplace_back("accept", accept);
    return n;
}
/// `hidden_input(name, value)` — a non-visual field carried in form submission.
inline NodeRef hidden_input(std::string name, std::string value){
    auto n = input(std::move(value)); n->input_type = "hidden"; n->name = std::move(name); return n;
}

// ── native progress / meter ─────────────────────────────────────────────────
/// `progress_el(value, max)` — a native <progress> bar (indeterminate if value<0).
inline NodeRef progress_el(double value, double max = 1.0){
    auto n = std::make_shared<Node>(); n->kind = Kind::markup;
    std::string v = value < 0 ? "" : " value=\"" + detail::numstr((float)value) + "\"";
    n->text = "<progress" + v + " max=\"" + detail::numstr((float)max) + "\"></progress>";
    finalize(*n); return n;
}
/// `meter_el(value, min, max)` — a native <meter> gauge (disk usage, score).
inline NodeRef meter_el(double value, double min = 0, double max = 1.0){
    auto n = std::make_shared<Node>(); n->kind = Kind::markup;
    n->text = "<meter value=\"" + detail::numstr((float)value) + "\" min=\"" + detail::numstr((float)min)
            + "\" max=\"" + detail::numstr((float)max) + "\"></meter>";
    finalize(*n); return n;
}

// ── structural grouping ─────────────────────────────────────────────────────
/// `fieldset(legend, fields…)` — a labelled group of controls (a real <fieldset>
/// with a <legend>). Improves accessibility and groups related inputs.
template <typename... Cs>
NodeRef fieldset(std::string legend_text, Cs... fields){
    auto leg = std::make_shared<Node>(); leg->kind = Kind::text; leg->text = std::move(legend_text); leg->tag = "legend"; finalize(*leg);
    auto fs = box(std::move(leg), std::move(fields)...);
    fs->tag = "fieldset";
    finalize(*fs);
    return fs;
}

/// `option_group(label, {option(...)...})` — an <optgroup> for a `select`. Wrap
/// options under a heading. (Pass its result inside `select`'s children region
/// via markup, or use the flat `select` for the common case.)
inline NodeRef option_group(std::string label, std::vector<Opt> opts){
    std::string html = "<optgroup label=\"";
    for (char c : label){ if (c=='"'||c=='&'||c=='<') html += ' '; else html += c; }
    html += "\">";
    for (auto& o : opts){
        html += "<option value=\"";
        for (char c : o.value){ if (c=='"'||c=='<'||c=='&') continue; html += c; }
        html += "\">";
        for (char c : (o.label.empty()?o.value:o.label)){ if (c=='<'||c=='&') continue; html += c; }
        html += "</option>";
    }
    html += "</optgroup>";
    return markup(std::move(html));
}

/// `with_list(id, input_node, {suggestions…})` — attach a native <datalist> of
/// autocomplete suggestions to a text input. The browser shows them as you type
/// (a real, accessible combobox) — no custom dropdown needed.
inline NodeRef with_list(std::string id, NodeRef field, std::vector<std::string> suggestions){
    field->attrs.emplace_back("list", id);
    std::string html = "<datalist id=\"" + id + "\">";
    for (auto& s : suggestions){
        html += "<option value=\"";
        for (char c : s){ if (c=='"'||c=='<'||c=='&') continue; html += c; }
        html += "\">";
    }
    html += "</datalist>";
    return box(std::move(field), markup(std::move(html)));
}

/// `label_for(text, id)` — an explicit <label for="id"> paired with a control by
/// id (when you can't nest the control inside the label).
inline NodeRef label_for(std::string text, std::string for_id){
    auto n = std::make_shared<Node>(); n->kind = Kind::text; n->text = std::move(text);
    n->tag = "label"; n->attrs.emplace_back("for", std::move(for_id)); finalize(*n); return n;
}

} // namespace waya::surface
