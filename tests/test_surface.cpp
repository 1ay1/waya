/// tests/test_surface.cpp — the Surface Model: primitives, the full style
/// vocabulary (box model, flex, position, effects, states, universal css),
/// minimal diff, and the wire format.

#include <waya/surface/node.hpp>
#include <waya/surface/dom.hpp>
#include <waya/surface/diff.hpp>
#include <waya/surface/wire.hpp>
#include <waya/surface/client.hpp>

#include <iostream>
#include <string>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)
static bool has(const std::string& h, std::string_view n){ return h.find(n)!=std::string::npos; }

enum { Inc, Reset };

static NodeRef view(int n) {
    detail::begin_msg_capture();   // the runtime does this before every render, so
                                  // tap tokens are stable per-render (salt resets)
    return col(
        text("Count") | fg(0x3b82f6) | font(28) | bold,
        text(n) | fg(0xe2e8f0),
        path({{0,50},{40,20},{80,35}}) | stroke(0x22d3ee, 2),
        text("+") | pad(8) | bg(0x334155) | round(8) | tap(Inc)
    ) | gap(16) | pad(24) | center;
}

int main() {
    // ── the vocabulary builds a tree; the user wrote no HTML ────────────────
    auto s = view(42);
    CHECK(s->kind == Kind::box);
    CHECK(s->style.flow == Flow::col);
    CHECK(s->kids.size() == 4);
    CHECK(s->kids[2]->kind == Kind::path && s->kids[2]->points.size() == 3);
    CHECK(s->kids[3]->on_tap != -1);   // a wire token was assigned for tap(Inc)

    // ── the full style vocabulary reaches CSS ───────────────────────────────
    {
        auto node = box(text("x"))
            | pad(12) | pad_x(20) | margin(8) | w(px(200)) | h(pct(50))
            | round(12) | border(2, 0x334155) | shadow() | opacity(0.9f)
            | justify(Justify::between) | align(Align::center) | wrap
            | absolute(px(10), px(20)) | z(5)
            | css("backdrop-filter", "blur(8px)")            // universal channel
            | on(Hover, bg(0x1e293b))                        // state
            | at(Md, w(fill));                               // breakpoint
        auto css = DomBackend{}.render(*node).css;
        CHECK(has(css, "padding:12px"));
        CHECK(has(css, "width:200px"));
        CHECK(has(css, "height:50%"));
        CHECK(has(css, "border-radius:12px"));
        CHECK(has(css, "border:2px solid"));
        CHECK(has(css, "box-shadow:"));
        CHECK(has(css, "opacity:0.9"));
        CHECK(has(css, "justify-content:space-between"));
        CHECK(has(css, "align-items:center"));
        CHECK(has(css, "flex-wrap:wrap"));
        CHECK(has(css, "position:absolute"));
        CHECK(has(css, "z-index:5"));
        CHECK(has(css, "backdrop-filter:blur(8px)"));         // universal css
        CHECK(has(css, ":hover{"));                           // state
        CHECK(has(css, "@media(min-width:768px)"));           // breakpoint
    }

    // ── DOM backend: HTML + interned classes + svg for paths ────────────────
    {
        auto out = DomBackend{}.render(*s);
        CHECK(has(out.html, "<div class=\"ws-"));
        CHECK(has(out.html, "<span"));
        CHECK(has(out.html, "Count"));
        CHECK(has(out.html, "<svg"));
        CHECK(has(out.html, "data-tap=\""));   // a tap wire token is emitted
        CHECK(has(out.css, "flex-direction:column"));
        // interning: identical styles collapse
        auto s2 = box(text("a") | pad(8), text("b") | pad(8));
        auto css2 = DomBackend{}.render(*s2).css;
        int rules = 0; for (std::size_t p=0;(p=css2.find(".ws-",p))!=std::string::npos;++p) ++rules;
        CHECK(rules <= 3);
    }

    // ── diff: count change = ONE op via the subtree hash fast-path ──────────
    {
        auto p = diff(view(42), view(43));
        CHECK(p.size() == 1);
        CHECK(p[0].op == Op::set_text);
        CHECK(p[0].s == "43");
        CHECK(p[0].path == "1");
    }
    CHECK(diff(view(7), view(7)).empty());

    // ── a style-only change diffs as set_shell (attrs channel) ────────────
    {
        auto a = text("x") | fg(0x111111);
        auto b = text("x") | fg(0x222222);
        auto p = diff(a, b);
        CHECK(p.size() == 1 && p[0].op == Op::set_shell);
    }

    // ── keyed diff is SAFE against duplicate keys ───────────────────────────
    // Duplicate sibling keys would corrupt an identity-based reconcile (find
    // returns the first match), so fully_keyed() must reject them and the diff
    // falls back to positional — which is always sound.
    {
        auto uniq = col(text("a") | key("k1"), text("b") | key("k2"));
        CHECK(detail::fully_keyed(uniq->kids, uniq->kids));   // clean keyed list
        auto dup = col(text("a") | key("k1"), text("b") | key("k1"));
        CHECK(!detail::fully_keyed(dup->kids, dup->kids));    // duplicates rejected
        // diffing a duplicate-keyed list must still produce a usable patch, not
        // crash or mis-address: change the second child's text.
        auto a = col(text("a") | key("k1"), text("b") | key("k1"));
        auto b = col(text("a") | key("k1"), text("c") | key("k1"));
        auto p = diff(a, b);
        CHECK(!p.empty());
        bool touches_child1 = false;
        for (auto& op : p) if (op.path.rfind("1", 0) == 0) touches_child1 = true;
        CHECK(touches_child1);   // positional path "1" addressed, not corrupted
    }

    // ── wire: patch JSON carries {css, ops} ─────────────────────────────────
    {
        auto j = patch_json(diff(view(1), view(2)));
        CHECK(has(j, "\"css\":"));
        // set_text is wire opcode 2; the count text lives at path "1".
        CHECK(has(j, "\"ops\":[[2,\"1\",\"2\"]]"));
    }

    // ── client WS URL is DEPLOYMENT-ROBUST (regression guard) ───────────────
    // The live socket URL must be derived from the page, not hardcoded, or it
    // breaks behind HTTPS/proxies/tunnels (mixed content + wrong port). This is
    // the test that would have caught the `ws://host:8080` bug.
    {
        std::string js = waya::surface::detail::client(8080);
        CHECK(has(js, "wss://"));                        // uses secure scheme on https
        CHECK(has(js, "location.protocol"));            // scheme derived from the page
        CHECK(has(js, "location.host"));                // host+port from the page, not hardcoded
        CHECK(!has(js, "'ws://'+location.hostname+':8080"));  // the exact old bug is gone
        // analytics: a pageview fires on every in-app route change (pushState),
        // dispatched as a CustomEvent + forwarded to the common SPA analytics
        // APIs, so nav is tracked without a page load.
        CHECK(has(js, "waya:pageview"));                 // the CustomEvent any tool can listen to
        CHECK(has(js, "firePageview"));                  // called from route()
        CHECK(has(js, "window.gtag") && has(js, "window.plausible"));  // GA4 + Plausible auto-forwarded
    }

    // ── "anything": a 5000-point chart is ONE node ──────────────────────
    {
        std::vector<Pt> big; for (int i=0;i<5000;++i) big.push_back({(float)i,(float)(i%100)});
        auto huge = path(big);
        CHECK(huge->points.size() == 5000 && huge->kids.empty());
    }

    // ── ONE protocol: a full paint and a delta are the same frame shape ─────
    // The client (terminal) has one code path; a full paint is just a `paint`
    // op (7) repainting the root. This is what makes it resyncable.
    {
        auto full  = full_frame(*view(0));
        auto delta = delta_frame(diff(view(0), view(1)));
        CHECK(has(full,  "\"css\":"));   CHECK(has(full,  "\"ops\":"));
        CHECK(has(delta, "\"css\":"));   CHECK(has(delta, "\"ops\":"));
        CHECK(has(full,  "[9,\"\","));   // the paint op (OP_PAINT=9) carries the root html
        CHECK(has(delta, "[2,\"1\",\"1\"]")); // the delta is a set_text (opcode 2)
    }

    // ── EVERYTHING IS A MOD: style, layout, state, interactivity all `node|mod` ─
    // maya's uniformity — one operator, one kind of thing, composes freely.
    {
        // tap and a style attr are the SAME kind of thing (both Mod)
        auto n = text("x") | fg(0x111111) | tap(3) | pad(8);
        CHECK(n->on_tap != -1);   // token assigned for tap(3)
        CHECK(n->style.has_fg && n->style.fg == 0x111111);
        CHECK(n->style.pad.value == 8);
        // Mods compose into a named bundle, then apply to any node
        Mod chip = pad(8) | round(999) | bg(0x334155) | fg(0xffffff);
        auto a = text("a") | chip;
        auto b = text("b") | chip;
        CHECK(a->style.pad.value == 8 && b->style.pad.value == 8);
        CHECK(a->style.radius.value == 999);
    }

    // ── the input primitive is a real field ────────────────────────────────
    {
        auto n = input("hello") | placeholder("name") | on_input(2) | type("email");
        CHECK(n->kind == Kind::input);
        CHECK(n->text == "hello");
        CHECK(n->placeholder == "name");
        CHECK(n->on_input != -1);   // token assigned
        CHECK(n->input_type == "email");
        auto html = DomBackend{}.render(*n).html;
        CHECK(has(html, "<input type=\"email\""));
        CHECK(has(html, "value=\"hello\""));
        CHECK(has(html, "placeholder=\"name\""));
        CHECK(has(html, "data-input=\""));   // input wire token emitted
    }

    // ── delightful mods reach real css: gradient, blur, scale, truncate ─────
    {
        auto css = DomBackend{}.render(*(box(text("x"))
            | gradient(0x111111, 0x222222) | blur(4) | scale(1.05f) | aspect(1.5f) | scroll)).css;
        CHECK(has(css, "linear-gradient"));
        CHECK(has(css, "filter:blur(4px)"));
        CHECK(has(css, "transform:scale(1.05"));
        CHECK(has(css, "aspect-ratio:1.5"));
        CHECK(has(css, "overflow:auto"));
    }

    std::cout << "test_surface: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}
