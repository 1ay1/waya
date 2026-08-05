#pragma once
/// \file traits.hpp
/// Per-element metadata: name, content categories, permitted children, void-ness,
/// and (for a few elements) a strict-parent constraint.
///
/// This is the machine-readable HTML5 content model. Each WAYA_ELEMENT row is
/// one `<element>` entry from https://html.spec.whatwg.org/, curated for Phase 1.
/// The generator (tools/gen_elements.py) will later emit this table wholesale.

#include "category.hpp"
#include "tag.hpp"

#include <string_view>

namespace waya::html {

/// Primary template — every Tag specialises this.
template <Tag T> struct Traits;

/// Declares one element's content model.
///   NAME       — the HTML tag name as written in output
///   CATS       — categories this element BELONGS TO (what parents accept it)
///   PERMITS    — categories this element accepts as CHILDREN
///   VOIDNESS   — true if the element is void (<br>, <img>) — no children, no close
#define WAYA_ELEMENT(TAG, NAME, CATS, PERMITS, VOIDNESS)                     \
    template <> struct Traits<Tag::TAG> {                                    \
        static constexpr std::string_view name = NAME;                       \
        static constexpr Cat  categories = (CATS);                           \
        static constexpr Cat  permits    = (PERMITS);                        \
        static constexpr bool is_void    = (VOIDNESS);                       \
    }

// ── Document structure ──────────────────────────────────────────────────────
WAYA_ELEMENT(html,    "html",    Cat::None,                              Cat::HtmlSect,                  false);
WAYA_ELEMENT(head,    "head",    Cat::HtmlSect,                          Cat::Metadata,                  false);
WAYA_ELEMENT(body,    "body",    Cat::HtmlSect,                          Cat::Flow,                      false);
WAYA_ELEMENT(title,   "title",   Cat::Metadata,                          Cat::Phrasing,                  false);
WAYA_ELEMENT(meta,    "meta",    Cat::Metadata,                          Cat::None,                      true);
WAYA_ELEMENT(link,    "link",    Cat::Metadata,                          Cat::None,                      true);
WAYA_ELEMENT(style,   "style",   Cat::Metadata,                          Cat::Phrasing,                  false);
WAYA_ELEMENT(base,    "base",    Cat::Metadata,                          Cat::None,                      true);
WAYA_ELEMENT(script,  "script",  Cat::Metadata|Cat::Flow|Cat::Phrasing|Cat::ScriptSupp, Cat::Phrasing,  false);
WAYA_ELEMENT(noscript,"noscript",Cat::Metadata|Cat::Flow|Cat::Phrasing,  Cat::Flow,                      false);

// ── Sections & headings ─────────────────────────────────────────────────────
WAYA_ELEMENT(header,  "header",  Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(footer,  "footer",  Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(main,    "main",    Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(nav,     "nav",     Cat::Flow|Cat::Sectioning|Cat::Palpable,Cat::Flow,                      false);
WAYA_ELEMENT(section, "section", Cat::Flow|Cat::Sectioning|Cat::Palpable,Cat::Flow,                      false);
WAYA_ELEMENT(article, "article", Cat::Flow|Cat::Sectioning|Cat::Palpable,Cat::Flow,                      false);
WAYA_ELEMENT(aside,   "aside",   Cat::Flow|Cat::Sectioning|Cat::Palpable,Cat::Flow,                      false);
WAYA_ELEMENT(hgroup,  "hgroup",  Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Heading,                   false);
WAYA_ELEMENT(h1,      "h1",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);
WAYA_ELEMENT(h2,      "h2",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);
WAYA_ELEMENT(h3,      "h3",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);
WAYA_ELEMENT(h4,      "h4",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);
WAYA_ELEMENT(h5,      "h5",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);
WAYA_ELEMENT(h6,      "h6",      Cat::Flow|Cat::Heading|Cat::Palpable,   Cat::Phrasing,                  false);

// ── Grouping / flow ─────────────────────────────────────────────────────────
WAYA_ELEMENT(div,     "div",     Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(p,       "p",       Cat::Flow|Cat::Palpable,                Cat::Phrasing,                  false);
WAYA_ELEMENT(hr,      "hr",      Cat::Flow,                              Cat::None,                      true);
WAYA_ELEMENT(pre,     "pre",     Cat::Flow|Cat::Palpable,                Cat::Phrasing,                  false);
WAYA_ELEMENT(blockquote,"blockquote",Cat::Flow|Cat::Palpable,           Cat::Flow,                      false);
WAYA_ELEMENT(figure,  "figure",  Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(figcaption,"figcaption",Cat::None,                          Cat::Flow,                      false);
WAYA_ELEMENT(ul,      "ul",      Cat::Flow|Cat::Palpable,                Cat::ListItem|Cat::ScriptSupp,  false);
WAYA_ELEMENT(ol,      "ol",      Cat::Flow|Cat::Palpable,                Cat::ListItem|Cat::ScriptSupp,  false);
WAYA_ELEMENT(li,      "li",      Cat::ListItem,                          Cat::Flow,                      false);
WAYA_ELEMENT(dl,      "dl",      Cat::Flow|Cat::Palpable,                Cat::DListItem,                 false);
WAYA_ELEMENT(dt,      "dt",      Cat::DListItem,                         Cat::Flow,                      false);
WAYA_ELEMENT(dd,      "dd",      Cat::DListItem,                         Cat::Flow,                      false);

// ── Text-level (phrasing) ───────────────────────────────────────────────────
WAYA_ELEMENT(a,       "a",       Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::Flow,     false);
WAYA_ELEMENT(span,    "span",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(em,      "em",      Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(strong,  "strong",  Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(small,   "small",   Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(s,       "s",       Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(cite,    "cite",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(code,    "code",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(sub,     "sub",     Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(sup,     "sup",     Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(i,       "i",       Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(b,       "b",       Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(u,       "u",       Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(mark,    "mark",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(br,      "br",      Cat::Flow|Cat::Phrasing,                Cat::None,                      true);
WAYA_ELEMENT(wbr,     "wbr",     Cat::Flow|Cat::Phrasing,                Cat::None,                      true);
WAYA_ELEMENT(time,    "time",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(abbr,    "abbr",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(kbd,     "kbd",     Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(samp,    "samp",    Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(var_,    "var",     Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);

// ── Embedded ────────────────────────────────────────────────────────────────
WAYA_ELEMENT(img,     "img",     Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Interactive|Cat::Palpable, Cat::None, true);
WAYA_ELEMENT(picture, "picture", Cat::Flow|Cat::Phrasing|Cat::Embedded,  Cat::Embedded|Cat::ScriptSupp,  false);
WAYA_ELEMENT(source,  "source",  Cat::None,                              Cat::None,                      true);
WAYA_ELEMENT(video,   "video",   Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Interactive|Cat::Palpable, Cat::Flow, false);
WAYA_ELEMENT(audio,   "audio",   Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Interactive|Cat::Palpable, Cat::Flow, false);
WAYA_ELEMENT(iframe,  "iframe",  Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Interactive|Cat::Palpable, Cat::None, false);
WAYA_ELEMENT(canvas,  "canvas",  Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Palpable, Cat::Flow,        false);
WAYA_ELEMENT(svg,     "svg",     Cat::Flow|Cat::Phrasing|Cat::Embedded|Cat::Palpable, Cat::Any,         false);

// ── Tabular ─────────────────────────────────────────────────────────────────
WAYA_ELEMENT(table,   "table",   Cat::Flow|Cat::Palpable,                Cat::TableSect|Cat::TableRow,   false);
WAYA_ELEMENT(caption, "caption", Cat::TableSect,                         Cat::Flow,                      false);
WAYA_ELEMENT(colgroup,"colgroup",Cat::TableSect,                         Cat::None,                      false);
WAYA_ELEMENT(col,     "col",     Cat::None,                              Cat::None,                      true);
WAYA_ELEMENT(thead,   "thead",   Cat::TableSect,                         Cat::TableRow,                  false);
WAYA_ELEMENT(tbody,   "tbody",   Cat::TableSect,                         Cat::TableRow,                  false);
WAYA_ELEMENT(tfoot,   "tfoot",   Cat::TableSect,                         Cat::TableRow,                  false);
WAYA_ELEMENT(tr,      "tr",      Cat::TableRow,                          Cat::TableCell,                 false);
WAYA_ELEMENT(td,      "td",      Cat::TableCell,                         Cat::Flow,                      false);
WAYA_ELEMENT(th,      "th",      Cat::TableCell,                         Cat::Flow,                      false);

// ── Forms ───────────────────────────────────────────────────────────────────
WAYA_ELEMENT(form,    "form",    Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(label,   "label",   Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::Phrasing, false);
WAYA_ELEMENT(input,   "input",   Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::None,     true);
WAYA_ELEMENT(button,  "button",  Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::Phrasing, false);
WAYA_ELEMENT(select,  "select",  Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::OptionLike,false);
WAYA_ELEMENT(option,  "option",  Cat::OptionLike,                        Cat::Phrasing,                  false);
WAYA_ELEMENT(optgroup,"optgroup",Cat::OptionLike,                        Cat::OptionLike,                false);
WAYA_ELEMENT(textarea,"textarea",Cat::Flow|Cat::Phrasing|Cat::Interactive|Cat::Palpable, Cat::Phrasing, false);
WAYA_ELEMENT(fieldset,"fieldset",Cat::Flow|Cat::Palpable,                Cat::Flow,                      false);
WAYA_ELEMENT(legend,  "legend",  Cat::None,                              Cat::Phrasing,                  false);
WAYA_ELEMENT(datalist,"datalist",Cat::Flow|Cat::Phrasing,                Cat::OptionLike|Cat::Phrasing,  false);
WAYA_ELEMENT(output,  "output",  Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(progress,"progress",Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);
WAYA_ELEMENT(meter,   "meter",   Cat::Flow|Cat::Phrasing|Cat::Palpable,  Cat::Phrasing,                  false);

// ── Interactive ─────────────────────────────────────────────────────────────
WAYA_ELEMENT(details, "details", Cat::Flow|Cat::Interactive|Cat::Palpable, Cat::SummaryLike|Cat::Flow,   false);
WAYA_ELEMENT(summary, "summary", Cat::SummaryLike,                       Cat::Phrasing|Cat::Heading,     false);
WAYA_ELEMENT(dialog,  "dialog",  Cat::Flow,                              Cat::Flow,                      false);

#undef WAYA_ELEMENT

} // namespace waya::html
