/// tests/test_input.cpp — the form controls and keyed-list move diffing.
///
/// Two features proven here:
///   1. Rich inputs — checkbox / radio / select / textarea / button each render
///      to the right HTML with value/checked/selected/name wired through.
///   2. Keyed reconciliation — a list of `key(...)` children that only reorders
///      emits `move` ops (not a re-render of every row), and the patch is SOUND:
///      replaying it over the old order reproduces the new order exactly.

#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/diff.hpp>
#include <waya/surface/wire.hpp>
#include <waya/surface/sugar.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }
static std::string html_of(const NodeRef& n){ return DomBackend{}.render(*n).html; }

// ── A tiny SOUNDNESS applier: replay a Patch over a node tree, so we can assert
//    diff(a,b) applied to a yields b. Mirrors the client's op semantics (paths
//    are dotted child indices; move/insert carry indices). Only needs to handle
//    the ops the keyed differ emits: replace/remove/insert/insert_at/move + the
//    in-place set_text/set_paint (which we model as "replace with next node").
static Node& at(NodeRef root, const std::string& path) {
    if (path.empty()) return *root;
    Node* cur = root.get();
    std::size_t i = 0;
    while (i < path.size()) {
        std::size_t dot = path.find('.', i);
        int idx = std::stoi(path.substr(i, dot - i));
        cur = cur->kids[idx].get();
        if (dot == std::string::npos) break;
        i = dot + 1;
    }
    return *cur;
}
static Node* parent_of(NodeRef root, const std::string& path, int& child_idx) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) { child_idx = std::stoi(path); return root.get(); }
    child_idx = std::stoi(path.substr(dot + 1));
    return &at(root, path.substr(0, dot));
}
// Replay a patch over `root`, mutating it in place. Named `replay_patch` (not
// `apply`) deliberately: calling an unqualified `apply(root, vec<PatchOp>)`
// drags in std::apply via ADL (vector's associated namespace is std), and
// overload resolution then instantiates std::apply's tuple_size<vector<...>>
// requirement into a hard error on libc++.
static void replay_patch(NodeRef root, const Patch& p) {
    for (auto& op : p) {
        switch (op.op) {
            case Op::set_text: case Op::set_inner: at(root, op.path).text = op.s; break;
            case Op::set_prop: {
                Node& n = at(root, op.path);
                if (op.prop == "value") { if (n.kind == Kind::select) n.selected = op.s; else n.text = op.s; }
                else if (op.prop == "src")     n.src = op.s;
                else if (op.prop == "checked") n.checked = !op.s.empty();
                break; }
            case Op::set_shell: case Op::replace: {
                int ci; Node* par = parent_of(root, op.path, ci);
                if (op.node) par->kids[ci] = op.node;    // adopt the fresh subtree
                break; }
            case Op::remove: {
                int ci; Node* par = parent_of(root, op.path, ci);
                par->kids.erase(par->kids.begin() + ci); break; }
            case Op::insert: {
                Node& par = at(root, op.path);
                if (op.to >= 0) par.kids.insert(par.kids.begin() + op.to, op.node);
                else par.kids.push_back(op.node);
                break; }
            case Op::move: {
                Node& par = at(root, op.path);
                NodeRef nd = par.kids[op.from];
                par.kids.erase(par.kids.begin() + op.from);
                par.kids.insert(par.kids.begin() + op.to, nd);
                break; }
        }
    }
}
// Order of keys under the root box — for asserting reorders.
static std::vector<std::string> keys(const NodeRef& box) {
    std::vector<std::string> k; for (auto& c : box->kids) k.push_back(c->key); return k;
}

static NodeRef list(std::vector<std::string> ks) {
    std::vector<NodeRef> kids;
    for (auto& s : ks) kids.push_back(text(s) | key(s));
    auto n = box(); n->kids = std::move(kids);
    // re-finalize so hashes reflect the assembled children
    for (auto& c : n->kids) finalize(*c);
    finalize(*n);
    return n;
}

int main() {
    // ═══ 1. RICH INPUTS ══════════════════════════════════════════════════════
    // checkbox
    {
        auto n = checkbox(true) | on_change(3) | name("agree");
        CHECK(n->kind == Kind::checkbox && n->checked);
        auto h = html_of(n);
        CHECK(has(h, "type=\"checkbox\"")); CHECK(has(h, " checked"));
        CHECK(has(h, "data-change=\"")); CHECK(has(h, "name=\"agree\""));
    }
    // an unchecked checkbox omits `checked`
    CHECK(!has(html_of(checkbox(false)), " checked"));

    // radio group
    {
        auto n = radio("plan", "pro", true) | on_change(4);
        CHECK(n->kind == Kind::radio && n->checked && n->text == "pro");
        auto h = html_of(n);
        CHECK(has(h, "type=\"radio\"")); CHECK(has(h, "value=\"pro\""));
        CHECK(has(h, "name=\"plan\"")); CHECK(has(h, " checked"));
    }

    // select + options, one selected
    {
        auto n = select({option("a","Apple"), option("b","Banana")}, "b") | on_change(5);
        CHECK(n->kind == Kind::select && n->selected == "b");
        auto h = html_of(n);
        CHECK(has(h, "<select")); CHECK(has(h, "data-change=\""));
        CHECK(has(h, "<option value=\"a\">Apple</option>"));
        CHECK(has(h, "<option value=\"b\" selected>Banana</option>"));
    }

    // textarea carries its value as element content
    {
        auto n = textarea("multi\nline") | placeholder("bio") | on_input(6);
        CHECK(n->kind == Kind::textarea);
        auto h = html_of(n);
        CHECK(has(h, "<textarea")); CHECK(has(h, "placeholder=\"bio\""));
        CHECK(has(h, "data-input=\"")); CHECK(has(h, ">multi\nline</textarea>"));
    }

    // button renders <button> and wires tap
    {
        auto n = button("Save") | tap(7);
        CHECK(n->kind == Kind::button);
        auto h = html_of(n);
        CHECK(has(h, "<button type=\"button\"")); CHECK(has(h, "data-tap=\""));
        CHECK(has(h, ">Save</button>"));
    }

    // disabled reaches the attribute
    CHECK(has(html_of(input("x") | disabled()), "disabled"));

    // a control field change diffs as set_prop (checked property) — cheap update
    {
        auto a = checkbox(false) | on_change(1);
        auto b = checkbox(true)  | on_change(1);
        auto p = diff(a, b);
        bool has_checked = false;
        for (auto& op : p) if (op.op == Op::set_prop && op.prop == "checked") has_checked = true;
        CHECK(has_checked);
    }
    // select value change → set_prop(value)
    {
        auto a = select({option("x"),option("y")}, "x");
        auto b = select({option("x"),option("y")}, "y");
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_prop && p[0].prop == "value" && p[0].s == "y");
    }

    // ═══ 2. KEYED-LIST MOVE DIFFING ══════════════════════════════════════════
    // Unkeyed lists still diff positionally (no regression).
    {
        auto a = box(text("a"), text("b"));
        auto b = box(text("a"), text("c"));
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_text && p[0].s == "c");
    }

    // Pure reorder of a keyed list → move ops only, no set_text/replace.
    {
        auto a = list({"1","2","3"});
        auto b = list({"3","1","2"});
        auto p = diff(a, b);
        bool only_moves = !p.empty();
        for (auto& op : p) if (op.op != Op::move) only_moves = false;
        CHECK(only_moves);   // reorder never re-renders a row
        // SOUND: replaying the patch over a reproduces b's order exactly.
        auto work = list({"1","2","3"});
        replay_patch(work, p);
        CHECK(keys(work) == keys(b));
    }

    // Reversal is sound too.
    {
        auto a = list({"1","2","3","4","5"});
        auto b = list({"5","4","3","2","1"});
        auto work = list({"1","2","3","4","5"});
        replay_patch(work, diff(a, b));
        CHECK(keys(work) == keys(b));
    }

    // Insert + remove + reorder together, all keyed → still sound.
    {
        auto a = list({"a","b","c","d"});
        auto b = list({"d","x","a","c"});  // remove b, add x, reorder
        auto p = diff(a, b);
        bool has_insert=false, has_remove=false, has_move=false;
        for (auto& op : p) {
            if (op.op==Op::insert) has_insert=true;
            if (op.op==Op::remove) has_remove=true;
            if (op.op==Op::move)   has_move=true;
        }
        CHECK(has_insert && has_remove && has_move);   // reorder produces a move too
        auto work = list({"a","b","c","d"});
        replay_patch(work, p);
        CHECK(keys(work) == keys(b));
    }

    // ── same-position, DIFFERENT key → one replace, not a morph ──────────────
    // Two screens sharing a slot (a route switch): keying them makes the diff a
    // single replace instead of morphing one screen's nodes into the other's.
    {
        auto screenA = col(text("clock"), text("00:00")) | key("screen:clock");
        auto screenB = col(text("about"), text("info"))  | key("screen:about");
        finalize(*screenA); finalize(*screenB);
        auto p = diff(screenA, screenB);
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::replace);       // identity changed → replace whole subtree
        CHECK(p[0].path == "");
    }
    // SAME key, changed content → still a normal in-place diff (no replace).
    {
        auto a = col(text("a"), text("1")) | key("s");
        auto b = col(text("a"), text("2")) | key("s");
        finalize(*a); finalize(*b);
        auto p = diff(a, b);
        bool no_replace = true; for (auto& op : p) if (op.op == Op::replace) no_replace = false;
        CHECK(no_replace);
        CHECK(p.size() == 1 && p[0].op == Op::set_text && p[0].s == "2");
    }

    // A keyed insert carries a target index (encodes as insert_at on the wire).
    {
        auto a = list({"a","b"});
        auto b = list({"a","z","b"});  // insert z at index 1
        auto p = diff(a, b);
        bool found=false;
        for (auto& op : p) if (op.op==Op::insert && op.to==1) found=true;
        CHECK(found);
        auto j = patch_json(p);
        CHECK(has(j, "[8,"));   // insert_at op id on the JSON wire (WIRE_INSERT_AT)
    }

    // Changing a row's CONTENT while others reorder: the changed row updates in
    // place, the rest move — no full re-render.
    {
        auto a = list({"1","2","3"});
        auto b = box(text("3")|key("3"), text("ONE")|key("1"), text("2")|key("2"));
        for (auto& c : b->kids) { finalize(*c); } finalize(*b);
        auto p = diff(a, b);
        auto work = list({"1","2","3"});
        replay_patch(work, p);
        CHECK(keys(work) == (std::vector<std::string>{"3","1","2"}));
    }

    //  3. INTERACTION EVENTS ─ keyboard / focus / submit / drag
    enum { Save, Close, Blurred, Dropped };
    // on_key emits data-ev-keydown="<token>|<key>" — token is an opaque hash, but
    // the KEY arg is stable, so we check the "|Enter" suffix.
    {
        auto h = html_of(box() | on_enter(Save));
        CHECK(has(h, "data-ev-keydown=\"")); CHECK(has(h, "|Enter\""));
    }
    { auto h = html_of(box() | on_escape(Close)); CHECK(has(h, "data-ev-keydown=\"") && has(h, "|Escape\"")); }
    { auto h = html_of(box() | on_key("ArrowDown", Save)); CHECK(has(h, "|ArrowDown\"")); }
    // focus / blur / generic
    CHECK(has(html_of(input("x") | on_blur(Blurred)), "data-ev-blur=\""));
    CHECK(has(html_of(box() | on_focus(Save)), "data-ev-focus=\""));
    CHECK(has(html_of(box() | on("pointerenter", Save)), "data-ev-pointerenter=\""));
    // draggable + on_drop
    {
        auto h = html_of(box() | draggable("card-7"));
        CHECK(has(h, "draggable=\"true\"")); CHECK(has(h, "name=\"card-7\""));
    }
    CHECK(has(html_of(box() | on_drop(Dropped)), "data-ev-drop=\""));
    // drop_arg tags a target with its id; drop_target does both in one mod, so a
    // drop delivers "<payload>:<id>" to the mapper. (regression: data-drop-arg
    // was read by the client but nothing emitted it.)
    CHECK(has(html_of(box() | drop_arg("col-2")), "data-drop-arg=\"col-2\""));
    {
        auto h = html_of(box() | drop_target("col-3", [](std::string s){ (void)s; return Dropped; }));
        CHECK(has(h, "data-ev-drop=\""));            // the handler is wired
        CHECK(has(h, "data-drop-arg=\"col-3\""));   // AND the target id is tagged
    }

    // MULTIPLE global shortcuts on ONE node coalesce into a SINGLE
    // data-ev-shortcut attribute (regression: each hotkey emitted its own
    // data-ev-shortcut, and since HTML attrs are unique the parser kept only the
    // first — so a game root with space/arrows/enter only responded to one key).
    // They must pack as a ';'-separated "<token>|<key>" list.
    {
        enum { A, B, C };
        auto h = html_of(box() | hotkey(" ", A) | hotkey("ArrowUp", B) | hotkey("Enter", C));
        std::size_t first = h.find("data-ev-shortcut=");
        CHECK(first != std::string::npos);
        CHECK(h.find("data-ev-shortcut=", first + 1) == std::string::npos);  // exactly one
        CHECK(has(h, "| "));         // space key present
        CHECK(has(h, "|ArrowUp"));
        CHECK(has(h, "|Enter"));
        CHECK(has(h, ";"));          // the list separator
    }
    { enum { X }; auto h = html_of(box() | hotkey("k", X)); CHECK(has(h, "data-ev-shortcut=\"") && has(h, "|k")); }

    // form + on_submit
    {
        auto f = form(input("a") | name("user"), button("go")) | on_submit(Save);
        CHECK(f->kind == Kind::form);
        auto h = html_of(f);
        CHECK(has(h, "<form")); CHECK(has(h, "data-ev-submit=\""));
        CHECK(has(h, "name=\"user\""));
    }

    // ═══ a11y / arbitrary attrs ════════════════════════════════════
    {
        auto h = html_of(box() | role("dialog") | aria("label", "Close") | title("hi") | tab_index(0));
        CHECK(has(h, "role=\"dialog\"")); CHECK(has(h, "aria-label=\"Close\""));
        CHECK(has(h, "title=\"hi\"")); CHECK(has(h, "tabindex=\"0\""));
    }
    CHECK(has(html_of(box() | attr("data-x", "1")), "data-x=\"1\""));

    // ═══ media + markup ══════════════════════════════════════════
    CHECK(has(html_of(video("v.mp4")), "<video src=\"v.mp4\""));
    CHECK(has(html_of(audio("a.mp3")), "<audio src=\"a.mp3\""));
    {
        // markup is NOT escaped (trusted raw HTML)
        auto h = html_of(markup("<b>bold</b>"));
        CHECK(has(h, "<b>bold</b>"));
    }
    // text IS escaped (contrast)
    CHECK(has(html_of(text("<b>x")), "&lt;b&gt;x"));

    // ═══ sugar: when / show / each / each_keyed / overlay ═══════════════════
    CHECK(when(true,  text("y"))->kind == Kind::text);
    CHECK(when(false, text("y"))->kind == Kind::box);      // empty box when false
    CHECK(show(false, text("y"))->kind == Kind::box);
    CHECK(when(true, text("a"), text("b"))->text == "a");
    {
        std::vector<int> xs{1,2,3};
        auto nodes = each(xs, [](int v){ return text(std::to_string(v)); });
        CHECK(nodes.size() == 3 && nodes[2]->text == "3");
        auto keyed = each_keyed(xs, [](int v){ return "k"+std::to_string(v); },
                                    [](int v){ return text(std::to_string(v)); });
        CHECK(keyed[0]->key == "k1" && keyed[2]->key == "k3");
    }
    {
        // overlay is a fixed, high-z full-screen layer
        auto ov = overlay(text("modal"));
        CHECK(ov->style.pos == Pos::fixed);
        CHECK(ov->style.has_z && ov->style.z >= 1000);
        CHECK(modal(false, text("x"))->kind == Kind::box);   // closed → empty
        CHECK(modal(true,  text("x"))->style.pos == Pos::fixed);
        // overlay() is a REAL modal: it carries the markers the client uses to
        // isolate focus/keyboard/clicks from the layer underneath.
        auto h = html_of(overlay(text("m")));
        CHECK(has(h, "data-modal=\"1\""));
        CHECK(has(h, "role=\"dialog\""));
        CHECK(has(h, "aria-modal=\"true\""));
        CHECK(has(h, "tabindex=\"-1\""));
    }

    // ═══ MOBILE RESPONSIVENESS ─ breakpoint mods + visibility ══════════════
    {
        // on_phone flips a row to a column below 768px (max-width query)
        auto css = DomBackend{}.render(*(row(text("a"), text("b")) | gap(24)
                        | on_phone(column, gap(8)))).css;
        CHECK(has(css, "@media(max-width:767.98px)"));
        CHECK(has(css, "flex-direction:column"));   // inside the phone query
        // desktop keeps the base row
        CHECK(has(css, "flex-direction:row"));
    }
    {
        // on_desktop applies at/above 768px (min-width query)
        auto css = DomBackend{}.render(*(box() | on_desktop(pad(24)))).css;
        CHECK(has(css, "@media(min-width:768px)"));
        CHECK(has(css, "padding:24px"));
    }
    {
        // below(Lg,..) is a max-width query at the Lg breakpoint
        auto css = DomBackend{}.render(*(box() | below(Lg, pad(8)))).css;
        CHECK(has(css, "@media(max-width:1023.98px)"));
    }
    {
        // responsive visibility: only_desktop hides below Md; only_phone hides above
        auto d = DomBackend{}.render(*(text("menu") | only_desktop())).css;
        CHECK(has(d, "@media(max-width:767.98px)") && has(d, "display:none"));
        auto p = DomBackend{}.render(*(text("burger") | only_phone())).css;
        CHECK(has(p, "@media(min-width:768px)") && has(p, "display:none"));
    }
    {
        // hide_below / hide_above target the right query
        CHECK(has(DomBackend{}.render(*(box() | hide_below(Md))).css, "@media(max-width:767.98px)"));
        CHECK(has(DomBackend{}.render(*(box() | hide_above(Md))).css, "@media(min-width:768px)"));
    }

    // event change diffs as set_shell (attrs/wiring channel, node-level)
    {
        auto a = box() | on_enter(Save);
        auto b = box() | on_enter(Close);
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_shell);
    }

    std::cout << "test_input: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
