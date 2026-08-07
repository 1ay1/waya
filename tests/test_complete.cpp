// tests/test_complete.cpp — browser-parity coverage. Every mod in complete.hpp
// and builder in forms.hpp renders the right CSS/HTML, so waya covers what the
// browser allows without dropping to css().
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* m){ if(c) ++pass; else { ++fail; std::cerr << "FAIL: " << m << "\n"; } }
static std::string css(NodeRef n){ return DomBackend{}.render(*n).css; }
static std::string html(NodeRef n){ return DomBackend{}.render(*n).html; }
static bool has(const std::string& h, const std::string& s){ return h.find(s) != std::string::npos; }

int main(){
    // ── flexbox completeness ─────────────────────────────────────────────────
    check(has(css(box() | basis(rem(20))), "flex-basis:20rem"), "basis");
    check(has(css(box() | self_center), "align-self:center"), "self_center");
    check(has(css(box() | order(3)), "order:3"), "order");
    check(has(css(box() | content_between), "align-content:space-between"), "content_between");
    check(has(css(box() | row_gap(8) | col_gap(12)), "row-gap:8px"), "row_gap");
    check(has(css(box() | col_gap(12)), "column-gap:12px"), "col_gap");

    // ── grid placement ───────────────────────────────────────────────────────
    check(has(css(box() | col_line("1 / 3")), "grid-column:1 / 3"), "col_line");
    check(has(css(box() | row_span_(2)), "grid-row:span 2"), "row_span_");
    check(has(css(box() | place_self("center")), "place-self:center"), "place_self");
    check(has(css(box() | justify_self_end), "justify-self:end"), "justify_self_end");
    check(has(css(box() | auto_flow("dense")), "grid-auto-flow:dense"), "auto_flow");

    // ── display / box ────────────────────────────────────────────────────────
    check(has(css(box() | inline_block), "display:inline-block"), "inline_block");
    check(has(css(box() | invisible), "visibility:hidden"), "invisible");
    check(has(css(box() | isolate), "isolation:isolate"), "isolate");
    check(has(css(box() | object_pos("top")), "object-position:top"), "object_pos");
    check(has(css(box() | box_sizing("content-box")), "box-sizing:content-box"), "box_sizing");
    check(has(css(box() | appearance_none), "appearance:none"), "appearance_none");
    check(has(css(box() | will_change("transform")), "will-change:transform"), "will_change");

    // ── interaction ──────────────────────────────────────────────────────────
    check(has(css(box() | touch_action("pan-y")), "touch-action:pan-y"), "touch_action");
    check(has(css(box() | user_select("none")), "user-select:none"), "user_select");
    check(has(css(box() | cursor_grab), "cursor:grab"), "cursor_grab");
    check(has(css(box() | resize("vertical")), "resize:vertical"), "resize");
    check(has(css(box() | accent(0x6366f1)), "accent-color:#6366f1"), "accent");
    check(has(css(box() | caret(0x22d3ee)), "caret-color:#22d3ee"), "caret");

    // ── scroll ───────────────────────────────────────────────────────────────
    check(has(css(box() | scroll_snap_x), "scroll-snap-type:x mandatory"), "scroll_snap_x");
    check(has(css(box() | snap_center), "scroll-snap-align:center"), "snap_center");
    check(has(css(box() | smooth_scroll), "scroll-behavior:smooth"), "smooth_scroll");
    check(has(css(box() | scrollbar_gutter("stable")), "scrollbar-gutter:stable"), "scrollbar_gutter");

    // ── transforms (2D + 3D) ─────────────────────────────────────────────────
    check(has(css(box() | translate_z(40)), "translateZ(40px)"), "translate_z");
    check(has(css(box() | rotate_x(12)), "rotateX(12deg)"), "rotate_x");
    check(has(css(box() | rotate_y(20)), "rotateY(20deg)"), "rotate_y");
    check(has(css(box() | skew(6, 2)), "skew(6deg,2deg)"), "skew");
    check(has(css(box() | perspective(800)), "perspective:800px"), "perspective");
    check(has(css(box() | transform_origin("top left")), "transform-origin:top left"), "transform_origin");
    check(has(css(box() | preserve_3d), "transform-style:preserve-3d"), "preserve_3d");
    check(has(css(box() | backface_hidden), "backface-visibility:hidden"), "backface_hidden");

    // ── visual: clip / mask / blend / filter / outline / gradients ───────────
    check(has(css(box() | clip_path("circle(50%)")), "clip-path:circle(50%)"), "clip_path");
    check(has(css(box() | mask_("linear-gradient(#000,transparent)")), "mask:linear-gradient"), "mask");
    check(has(css(box() | mix_blend("screen")), "mix-blend-mode:screen"), "mix_blend");
    check(has(css(box() | filter("blur(4px)")), "filter:blur(4px)"), "filter");
    check(has(css(box() | outline(2, 0x6366f1)), "outline:2px solid #6366f1"), "outline");
    check(has(css(box() | outline_offset(3)), "outline-offset:3px"), "outline_offset");
    check(has(css(box() | inset_shadow("0 2px 4px #000")), "box-shadow:inset"), "inset_shadow");
    check(has(css(box() | conic_gradient("#f00,#00f")), "conic-gradient"), "conic_gradient");
    check(has(css(box() | radial_gradient("#f00,#00f")), "radial-gradient"), "radial_gradient");
    check(has(css(box() | linear_gradient("135deg,#a,#b")), "linear-gradient(135deg"), "linear_gradient");

    // ── text ─────────────────────────────────────────────────────────────────
    check(has(css(box() | text_shadow("0 1px 2px #000")), "text-shadow:0 1px 2px #000"), "text_shadow");
    check(has(css(box() | text_indent(20)), "text-indent:20px"), "text_indent");
    check(has(css(box() | white_space("pre-wrap")), "white-space:pre-wrap"), "white_space");
    check(has(css(box() | word_break("break-all")), "word-break:break-all"), "word_break");
    check(has(css(box() | hyphens()), "hyphens:auto"), "hyphens");
    check(has(css(box() | vertical_align("middle")), "vertical-align:middle"), "vertical_align");
    check(has(css(box() | writing_mode("vertical-rl")), "writing-mode:vertical-rl"), "writing_mode");
    check(has(css(box() | text_columns(3)), "column-count:3"), "text_columns");
    check(has(css(box() | text_wrap("balance")), "text-wrap:balance"), "text_wrap");
    check(has(css(box() | list_style("none")), "list-style:none"), "list_style");

    // ── forms: every native input type ───────────────────────────────────────
    check(has(html(number_input("5", 0, 10, 1)), "type=\"number\"") && has(html(number_input("5",0,10,1)), "max=\"10\""), "number_input");
    check(has(html(range_input("50", 0, 100)), "type=\"range\""), "range_input");
    check(has(html(date_input()), "type=\"date\""), "date_input");
    check(has(html(time_input()), "type=\"time\""), "time_input");
    check(has(html(datetime_input()), "type=\"datetime-local\""), "datetime_input");
    check(has(html(color_input("#6366f1")), "type=\"color\""), "color_input");
    check(has(html(password_input()), "type=\"password\""), "password_input");
    check(has(html(email_input()), "type=\"email\""), "email_input");
    check(has(html(tel_input()), "type=\"tel\""), "tel_input");
    check(has(html(url_input()), "type=\"url\""), "url_input");
    check(has(html(search_input()), "type=\"search\""), "search_input");
    check(has(html(file_input(true, "image/*")), "type=\"file\"") && has(html(file_input(true,"image/*")), "multiple"), "file_input");
    check(has(html(hidden_input("csrf", "abc")), "type=\"hidden\"") && has(html(hidden_input("csrf","abc")), "name=\"csrf\""), "hidden_input");

    // ── forms: native progress / meter / structural ──────────────────────────
    check(has(html(progress_el(0.6)), "<progress value=\"0.6\""), "progress_el");
    check(has(html(meter_el(0.8, 0, 1)), "<meter value=\"0.8\""), "meter_el");
    check(has(html(fieldset("Shipping", input("a") | name("addr"))), "<fieldset>") &&
          has(html(fieldset("Shipping", input("a") | name("addr"))), "<legend>Shipping</legend>"), "fieldset");
    { auto wl = with_list("cities", input("Par"), {"Paris","Berlin"});
      check(has(html(wl), "list=\"cities\"") && has(html(wl), "<datalist id=\"cities\"") && has(html(wl), "value=\"Paris\""), "with_list"); }
    check(has(html(label_for("Name", "nm")), "<label for=\"nm\">Name</label>"), "label_for");
    check(has(html(option_group("EU", { option("fr","France") })), "<optgroup label=\"EU\"") &&
          has(html(option_group("EU", { option("fr","France") })), "value=\"fr\""), "option_group");

    // ── input attributes: validation & constraints ──────────────────────────
    check(has(html(input("") | required()), " required"), "required");
    check(has(html(input("") | readonly()), " readonly"), "readonly");
    check(has(html(number_input("1") | min_val(0.0) | max_val(9.0)), "min=\"0\"") &&
          has(html(number_input("1") | min_val(0.0) | max_val(9.0)), "max=\"9\""), "min_val/max_val");
    check(has(html(number_input("1") | step_any()), "step=\"any\""), "step_any");
    check(has(html(number_input("1") | step_by(0.5)), "step=\"0.5\""), "step_by");
    check(has(html(input("") | pattern("[0-9]{3}")), "pattern=\"[0-9]{3}\""), "pattern");
    check(has(html(input("") | maxlength(20) | minlength(3)), "maxlength=\"20\"") &&
          has(html(input("") | maxlength(20) | minlength(3)), "minlength=\"3\""), "maxlength/minlength");
    check(has(html(input("") | title_hint("3 digits")), "title=\"3 digits\""), "title_hint");

    // ── input attributes: mobile & assistive ─────────────────────────────────
    check(has(html(input("") | inputmode("numeric")), "inputmode=\"numeric\""), "inputmode");
    check(has(html(input("") | enterkey("send")), "enterkeyhint=\"send\""), "enterkey");
    check(has(html(input("") | autocomplete("email")), "autocomplete=\"email\""), "autocomplete");
    check(has(html(input("") | spellcheck(false)), "spellcheck=\"false\""), "spellcheck");
    check(has(html(input("") | autocapitalize("none")), "autocapitalize=\"none\""), "autocapitalize");

    // ── input attributes: multi/sizing/misc ────────────────────────────────
    check(has(html(select({}, "") | allow_multiple()), " multiple"), "allow_multiple");
    check(has(html(file_input() | accepts(".pdf")), "accept=\".pdf\""), "accepts");
    check(has(html(textarea("") | rows(5) | cols(30)), "rows=\"5\""), "rows/cols");
    check(has(html(input("") | size_attr(12)), "size=\"12\""), "size_attr");
    check(has(html(input("") | id("em")), "id=\"em\""), "id");
    check(has(html(input("") | default_value("x")), "value=\"x\""), "default_value");
    check(has(html(input("") | form_id("login")), "form=\"login\""), "form_id");
    check(has(html(file_input() | capture("user")), "capture=\"user\""), "capture");

    // ── input events wire attributes (generic client delegation handles firing) ─
    check(has(html(input("") | on_invalid(1)), "data-ev-invalid=\""), "on_invalid");
    check(has(html(input("") | on_paste(1)), "data-ev-paste=\""), "on_paste");
    check(has(html(box() | on_wheel(1)), "data-ev-wheel=\""), "on_wheel");
    check(has(html(box() | on_scroll(1)), "data-ev-scroll=\""), "on_scroll");
    check(has(html(box() | on_context(1)), "data-ev-contextmenu=\""), "on_context");
    check(has(html(input("") | on_copy(1)), "data-ev-copy=\""), "on_copy");
    check(has(html(search_input() | on_search([](std::string v){ return v; })), "data-ev-search=\""), "on_search");

    std::cout << "test_complete: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
