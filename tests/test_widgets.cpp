// tests/test_widgets.cpp — the stateful component library + icons + form data.
#include <waya/surface/live.hpp>
#include <waya/surface/diff.hpp>
#include <waya/ui.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }
static std::string html_of(NodeRef n) { return DomBackend{}.render(*n).html; }

struct Row { int id; std::string name; };

int main() {
    assets().clear();

    // ── toggle reflects state ────────────────────────────────────────────────
    check(has(css_of(toggle(true, 1)), "translateX(20px)"), "toggle on: knob slid");
    check(has(css_of(toggle(false, 1)), "translateX(0)"), "toggle off: knob home");
    check(has(html_of(toggle(true, 1)), "role=\"switch\""), "toggle has switch role");
    check(has(html_of(toggle(true, 1)), "aria-checked=\"true\""), "toggle aria-checked");

    // ── progress clamps + widths ─────────────────────────────────────────────
    check(has(css_of(progress(42)), "width:42%"), "progress width");
    check(has(css_of(progress(150)), "width:100%"), "progress clamps high");
    check(has(css_of(progress(-5)), "width:0%"), "progress clamps low");
    check(has(html_of(progress(30)), "role=\"progressbar\""), "progress role");

    // ── slider is a themed range that delivers its value ─────────────────────
    { auto s = slider(50, 0, 100, [](std::string v){ return v; });   // mapper: value -> Msg
      auto h = html_of(s);
      check(has(h, "type=\"range\""), "slider is a range input");
      check(has(h, "min=\"0\"") && has(h, "max=\"100\""), "slider min/max");
      check(has(h, "data-input"), "slider wires on_input so the dragged value reaches update");
      check(has(assets().style_css(), "wa-range"), "slider registers thumb css"); }

    // ── menu shows items only when open ──────────────────────────────────────
    check(has(html_of(menu(true, button("m"), {menu_item("A", 1), menu_item("B", 2)})), "A"),
          "open menu shows items");
    check(!has(html_of(menu(false, button("m"), {menu_item("A", 1)})), "backdrop-filter"),
          "closed menu has no panel chrome");

    // ── accordion: open panel shows body + rotated chevron ───────────────────
    { auto a = accordion(1, {{"One", text("b1")}, {"Two", text("b2")}}, +[](int i){ return i; });
      auto h = html_of(a), c = css_of(a);
      check(has(h, "One") && has(h, "Two"), "accordion headers render");
      check(has(h, "b2"), "accordion shows open panel body");
      check(!has(h, "b1"), "accordion hides closed panel body");
      check(has(c, "rotate(180deg)"), "open chevron rotated"); }

    // ── data_table aligns as a grid with header + rows ───────────────────────
    { std::vector<Row> rows{{1, "Ada"}, {2, "Linus"}};
      auto t = data_table<Row>(rows, {
          {"ID",   [](const Row& r){ return text(r.id); }},
          {"Name", [](const Row& r){ return text(r.name); }} });
      auto h = html_of(t), c = css_of(t);
      check(has(c, "display:grid"), "table is a grid");
      check(has(c, "grid-template-columns:repeat(2"), "table has 2 column tracks");
      check(has(h, "ID") && has(h, "Name"), "table headers");
      check(has(h, "Ada") && has(h, "Linus"), "table rows"); }

    // ── icons render inline SVG, unknown → empty ─────────────────────────────
    check(has(html_of(icon("check")), "<svg") && has(html_of(icon("check")), "polyline"), "icon renders svg");
    check(has(html_of(icon("search")), "circle"), "search icon geometry");
    check(has(html_of(icon("nonexistent")), "<svg"), "unknown icon still an (empty) svg");
    check(has(html_of(icon("check", 32)), "width='32'"), "icon size applied");
    check(has(html_of(icon("check")), "currentColor"), "icon uses currentColor for fg tint");

    // ── FormData parsing ─────────────────────────────────────────────────────
    { auto f = FormData::parse("email=a%40b.com&name=Ada+Lovelace&agree=on");
      check(f.get("email") == "a@b.com", "FormData url-decodes %40");
      check(f.get("name") == "Ada Lovelace", "FormData decodes +");
      check(f.checked("agree"), "FormData checkbox on");
      check(!f.checked("missing"), "FormData missing checkbox false");
      check(f.get("missing", "def") == "def", "FormData fallback");
      check(f.has("email") && !f.has("nope"), "FormData has()"); }

    // ── file_field: a labelled, skinned, wired file picker ────────────────
    { detail::begin_msg_capture();
      auto h = html_of(file_field("Avatar", [](FileData f){ return (int)f.content.size(); },
                                  "image/*", "PNG or JPG, up to 8 MB"));
      check(has(h, "type=\"file\""), "file_field renders a file input");
      check(has(h, "data-ev-file="), "file_field wires on_file");
      check(has(h, "accept=\"image/*\""), "file_field forwards accept");
      check(has(h, "<label"), "file_field is a real labelled field");
      check(has(h, "Avatar"), "file_field shows its label");
      check(has(h, "up to 8 MB"), "file_field shows its hint"); }

    // ── scene(): the vector-drawing vocabulary (replaces raw <svg> strings) ──
    {
        auto s = scene(100, 50,
            vrect(0, 0, 100, 50).fill(0x0b1020),
            vline(0, 25, 100, 25).stroke(0x22d3ee, 2).dashed(),
            vcircle(50, 25, 10).fill(rgba(0x6366f1, 0.8f)),
            vtext(50, 30, "a<b&c").fill(0xffffff).anchor_mid().bold());
        auto h = html_of(s);
        check(has(h, "<svg") && has(h, "viewBox=\"0 0 100 50\""), "scene emits one sized svg");
        check(has(h, "<rect") && has(h, "fill=\"#0b1020\""), "vrect + fill(hex)");
        check(has(h, "<line") && has(h, "stroke=\"#22d3ee\"") && has(h, "stroke-width=\"2\""), "vline + stroke");
        check(has(h, "stroke-dasharray=\"4 4\""), ".dashed()");
        check(has(h, "<circle") && has(h, "fill=\"rgba(99,102,241,0.8"), "vcircle + alpha fill");
        check(has(h, "text-anchor=\"middle\"") && has(h, "font-weight=\"700\""), "vtext placement");
        // THE point: content is escaped — no raw < or & reaches the DOM.
        check(has(h, "a&lt;b&amp;c") && !has(h, "a<b&c"), "vtext escapes its content");
    }
    // a scene is a normal node: it diffs like any subtree.
    {
        auto a = scene(10, 10, vcircle(5, 5, 4));
        auto b = scene(10, 10, vcircle(5, 5, 5));   // radius changed
        check(!diff(a, b).empty(), "a changed scene diffs (not a no-op)");
    }
    // bars() is now built on the scene vocabulary, not hand-glued svg strings
    { auto h = html_of(bars({1, 3, 2, 5}));
      check(has(h, "<svg") && has(h, "<rect"), "bars() renders via scene");
      check(!has(h, "<rect x='"), "bars() no longer hand-concatenates svg"); }

    std::cout << "test_widgets: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
