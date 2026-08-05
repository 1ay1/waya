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
static void apply(NodeRef root, const Patch& p) {
    for (auto& op : p) {
        switch (op.op) {
            case Op::set_text: at(root, op.path).text = op.s; break;
            case Op::set_src:  at(root, op.path).src  = op.s; break;
            case Op::set_paint: case Op::set_path: case Op::replace: {
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
        CHECK(has(h, "data-change=\"3\"")); CHECK(has(h, "name=\"agree\""));
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
        CHECK(has(h, "<select")); CHECK(has(h, "data-change=\"5\""));
        CHECK(has(h, "<option value=\"a\">Apple</option>"));
        CHECK(has(h, "<option value=\"b\" selected>Banana</option>"));
    }

    // textarea carries its value as element content
    {
        auto n = textarea("multi\nline") | placeholder("bio") | on_input(6);
        CHECK(n->kind == Kind::textarea);
        auto h = html_of(n);
        CHECK(has(h, "<textarea")); CHECK(has(h, "placeholder=\"bio\""));
        CHECK(has(h, "data-input=\"6\"")); CHECK(has(h, ">multi\nline</textarea>"));
    }

    // button renders <button> and wires tap
    {
        auto n = button("Save") | tap(7);
        CHECK(n->kind == Kind::button);
        auto h = html_of(n);
        CHECK(has(h, "<button type=\"button\"")); CHECK(has(h, "data-tap=\"7\""));
        CHECK(has(h, ">Save</button>"));
    }

    // disabled reaches the attribute
    CHECK(has(html_of(input("x") | disabled()), "disabled"));

    // a control field change diffs as set_paint (not replace) — cheap update
    {
        auto a = checkbox(false) | on_change(1);
        auto b = checkbox(true)  | on_change(1);
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_paint);
    }
    // select value change → set_paint
    {
        auto a = select({option("x"),option("y")}, "x");
        auto b = select({option("x"),option("y")}, "y");
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_paint);
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
        apply(work, p);
        CHECK(keys(work) == keys(b));
    }

    // Reversal is sound too.
    {
        auto a = list({"1","2","3","4","5"});
        auto b = list({"5","4","3","2","1"});
        auto work = list({"1","2","3","4","5"});
        apply(work, diff(a, b));
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
        CHECK(has_insert && has_remove);
        auto work = list({"a","b","c","d"});
        apply(work, p);
        CHECK(keys(work) == keys(b));
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
        CHECK(has(j, "[9,"));   // insert_at op id on the JSON wire
    }

    // Changing a row's CONTENT while others reorder: the changed row updates in
    // place, the rest move — no full re-render.
    {
        auto a = list({"1","2","3"});
        auto b = box(text("3")|key("3"), text("ONE")|key("1"), text("2")|key("2"));
        for (auto& c : b->kids) finalize(*c); finalize(*b);
        auto p = diff(a, b);
        auto work = list({"1","2","3"});
        apply(work, p);
        CHECK(keys(work) == (std::vector<std::string>{"3","1","2"}));
    }

    std::cout << "test_input: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
