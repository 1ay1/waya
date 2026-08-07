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

// ═══════════════════════════════════════════════════════════════════════════
//  INPUT ATTRIBUTES — the full native constraint/behaviour surface, as mods.
//  These are the difference between "you can hack it with attr()" and real,
//  discoverable input support. Every one is a named mod over the attr channel.
// ═══════════════════════════════════════════════════════════════════════════

// ── validation & constraints (drive native browser validation + :invalid) ───
/// `required()` — the field must be filled for the form to submit.
inline Mod required(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("required", ""); }}; }
/// `readonly()` — shown but not editable (still submitted).
inline Mod readonly(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("readonly", ""); }}; }
/// `min_val(x)` / `max_val(x)` — numeric/date bounds (number, range, date…).
inline Mod min_val(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("min", v); }}; }
inline Mod max_val(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("max", v); }}; }
inline Mod min_val(double v){ return min_val(detail::numstr((float)v)); }
inline Mod max_val(double v){ return max_val(detail::numstr((float)v)); }
/// `step_by(x)` / `step_any()` — numeric/range granularity.
inline Mod step_by(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("step", v); }}; }
inline Mod step_by(double v){ return step_by(detail::numstr((float)v)); }
inline Mod step_any(){ return step_by(std::string("any")); }
/// `pattern("[0-9]{3}")` — a regex the value must match (native validation).
inline Mod pattern(std::string re){ return {[=](Node& n){ n.attrs.emplace_back("pattern", re); }}; }
/// `maxlength(n)` / `minlength(n)` — text length bounds.
inline Mod maxlength(int n){ return {[=](Node& nd){ nd.attrs.emplace_back("maxlength", std::to_string(n)); }}; }
inline Mod minlength(int n){ return {[=](Node& nd){ nd.attrs.emplace_back("minlength", std::to_string(n)); }}; }
/// `title_hint("Enter 3 digits")` — the message shown when validation fails.
inline Mod title_hint(std::string t){ return {[=](Node& n){ n.attrs.emplace_back("title", t); }}; }

// ── mobile & assistive behaviour ────────────────────────────────────────────
/// `inputmode("numeric"|"decimal"|"tel"|"email"|"url"|"search"|"none")` — the
/// on-screen KEYBOARD a phone shows, independent of the input type. Big UX win.
inline Mod inputmode(std::string mode){ return {[=](Node& n){ n.attrs.emplace_back("inputmode", mode); }}; }
/// `enterkey("send"|"go"|"search"|"next"|"done")` — the mobile Enter-key label.
inline Mod enterkey(std::string hint){ return {[=](Node& n){ n.attrs.emplace_back("enterkeyhint", hint); }}; }
/// `autocomplete("email"|"current-password"|"one-time-code"|"off"…)` — autofill.
inline Mod autocomplete(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("autocomplete", v); }}; }
/// `spellcheck(false)` — toggle spell-checking (off for codes/usernames).
inline Mod spellcheck(bool on){ return {[=](Node& n){ n.attrs.emplace_back("spellcheck", on?"true":"false"); }}; }
/// `autocapitalize("none"|"sentences"|"words"|"characters")` — mobile capitalisation.
inline Mod autocapitalize(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("autocapitalize", v); }}; }
/// `autocorrect(false)` — iOS autocorrect toggle.
inline Mod autocorrect(bool on){ return {[=](Node& n){ n.attrs.emplace_back("autocorrect", on?"on":"off"); }}; }

// ── multi-value / sizing / misc ─────────────────────────────────────────────
/// `allow_multiple()` — a select or file input accepts multiple values.
inline Mod allow_multiple(bool on = true){ return {[=](Node& n){ if(on) n.attrs.emplace_back("multiple", ""); }}; }
/// `accepts("image/*")` — file-picker accept filter.
inline Mod accepts(std::string types){ return {[=](Node& n){ n.attrs.emplace_back("accept", types); }}; }
/// `rows(n)` / `cols(n)` — textarea dimensions.
inline Mod rows(int n){ return {[=](Node& nd){ nd.attrs.emplace_back("rows", std::to_string(n)); }}; }
inline Mod cols(int n){ return {[=](Node& nd){ nd.attrs.emplace_back("cols", std::to_string(n)); }}; }
/// `size_attr(n)` — the visible character width of a text input.
inline Mod size_attr(int n){ return {[=](Node& nd){ nd.attrs.emplace_back("size", std::to_string(n)); }}; }
/// `wrap_hard()` / `wrap_soft()` — textarea line-wrap submission behaviour.
inline Mod wrap_hard(){ return {[=](Node& n){ n.attrs.emplace_back("wrap", "hard"); }}; }
inline Mod wrap_soft(){ return {[=](Node& n){ n.attrs.emplace_back("wrap", "soft"); }}; }
/// `capture("user"|"environment")` — which camera a file input opens (mobile).
inline Mod capture(std::string src){ return {[=](Node& n){ n.attrs.emplace_back("capture", src); }}; }
/// `form_id("login")` — associate a control with a <form> by id (outside it).
inline Mod form_id(std::string id){ return {[=](Node& n){ n.attrs.emplace_back("form", id); }}; }
/// `id("email")` — set the element id (for label_for pairing / anchors).
inline Mod id(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("id", v); }}; }
/// `default_value(v)` — the value attribute (uncontrolled initial value).
inline Mod default_value(std::string v){ return {[=](Node& n){ n.attrs.emplace_back("value", v); }}; }
/// `list_id("cities")` — point a text input at a <datalist> by id (see with_list).
inline Mod list_id(std::string id){ return {[=](Node& n){ n.attrs.emplace_back("list", id); }}; }

// ═══════════════════════════════════════════════════════════════════════════
//  INPUT EVENTS — beyond on_input/on_change/on_enter (in node.hpp): the rest of
//  the browser's input event surface, value-carrying where it matters.
// ═══════════════════════════════════════════════════════════════════════════

/// `on_invalid(Msg)` — fires when native validation rejects the field on submit.
template <typename Msg> Mod on_invalid(Msg m){ return on("invalid", std::move(m)); }
/// `on_search(fn)` — a search input's clear/enter (fires with the value).
template <typename Fn> Mod on_search(Fn fn){ return on_ev("search", std::move(fn)); }
/// `on_paste(Msg)` — the user pasted into the field.
template <typename Msg> Mod on_paste(Msg m){ return on("paste", std::move(m)); }
/// `on_select_text(Msg)` — the user selected text within the field.
template <typename Msg> Mod on_select_text(Msg m){ return on("select", std::move(m)); }
/// `on_wheel(Msg)` — mouse-wheel over the element (custom scrubbers, zoom).
template <typename Msg> Mod on_wheel(Msg m){ return on("wheel", std::move(m)); }
/// `on_scroll(Msg)` — a scroll container scrolled (infinite lists, scroll-spy).
template <typename Msg> Mod on_scroll(Msg m){ return on("scroll", std::move(m)); }
/// `on_context(Msg)` — right-click / long-press context menu.
template <typename Msg> Mod on_context(Msg m){ return on("contextmenu", std::move(m)); }
/// `on_copy(Msg)` / `on_cut(Msg)` — clipboard copy/cut from the field.
template <typename Msg> Mod on_copy(Msg m){ return on("copy", std::move(m)); }
template <typename Msg> Mod on_cut(Msg m){ return on("cut", std::move(m)); }
/// `on_keyup(fn)` — keyup with the key string (debounce-on-release patterns).
template <typename Fn> Mod on_keyup(Fn fn){ return on_ev("keyup", std::move(fn)); }
/// `on_beforeinput(Msg)` — fires before the value changes (intercept/filter).
template <typename Msg> Mod on_beforeinput(Msg m){ return on("beforeinput", std::move(m)); }

} // namespace waya::surface
