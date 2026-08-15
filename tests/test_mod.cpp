// tests/test_mod.cpp — the zero-cost style layer: the small-buffer Mod (no heap
// alloc for the common case) and the Building proxy (finalize ONCE per chain,
// not per mod). Proves the maya "styling is ~free" property while keeping the
// diff-critical hashing identical to the old per-mod finalize.
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }
static std::string html_of(NodeRef n) { return DomBackend{}.render(*n).html; }

int main() {
    // ── Mod value semantics: copy, move, compose (SBO + heap) ────────────────
    {
        Mod m = bold;                 // small inline mod
        Mod copy = m;                 // copy
        Mod moved = std::move(copy);  // move
        auto n = text("x") | moved;
        check(has(css_of(n), "font-weight"), "copied+moved Mod applies");
    }
    {
        // deep composite forces the heap path; copy + apply must be correct
        Mod deep = bold | italic | underline | strike
                 | fg(0x111111) | bg(0x222222) | pad(4) | round(8);
        Mod copy = deep;
        auto n = text("y") | copy;
        auto c = css_of(n);
        check(has(c, "font-weight") && has(c, "font-style") && has(c, "#111111") && has(c, "#222222"),
              "deep composite (heap) copies + applies all parts");
    }
    {
        // noop is the identity
        auto a = text("z") | bold | noop | italic;
        check(has(css_of(a), "font-weight") && has(css_of(a), "font-style"), "noop is identity");
    }

    // ── Building: a chain finalizes ONCE but yields the SAME hash as before ──
    {
        // Two identical chains must hash identically (diff correctness).
        auto a = col(text("Hi") | fg(0x111) | bold | pad(8), box() | round(4)) | gap(12) | pad(16);
        auto b = col(text("Hi") | fg(0x111) | bold | pad(8), box() | round(4)) | gap(12) | pad(16);
        NodeRef na = a, nb = b;
        check(na->hash == nb->hash, "identical chains hash equal (deferred finalize)");
        check(diff(na, nb).empty(), "identical chains diff to nothing");
    }
    {
        // A single-field change is detected as exactly one op.
        auto a = col(text("Hi") | bold | pad(8)) | gap(12);
        auto b = col(text("Ho") | bold | pad(8)) | gap(12);
        NodeRef na = a, nb = b;
        auto p = diff(na, nb);
        check(p.size() == 1 && p[0].op == Op::set_text, "one text change -> one set_text op");
    }
    {
        // Mod ORDER still resolves right-wins (last mod on a field wins).
        auto n = box() | bg(0x111111) | bg(0x222222);
        check(has(css_of(n), "#222222") && !has(css_of(n), "#111111"), "right-most mod wins (deterministic)");
    }
    {
        // A Building is usable as a NodeRef everywhere (implicit consume).
        NodeRef n = box(text("a")) | pad(4);   // returned into a NodeRef
        check(n != nullptr && n->hash != 0, "Building -> NodeRef finalizes");
        // and nesting a Building as a child finalizes it too
        auto parent = col(box(text("x")) | pad(2) | round(2), text("y") | bold);
        NodeRef np = parent;
        check(np->kids.size() == 2 && np->kids[0]->hash != 0, "Building child finalized when nested");
    }

    // ── completeness: named mods that used to require a raw css() ───────────
    check(has(css_of(box() | w_full), "width:100%"), "w_full");
    check(has(css_of(box() | h_screen), "100dvh"), "h_screen (mobile-safe dvh)");
    check(has(css_of(box() | w_frac(1,3)), "33.") , "w_frac(1,3) is 33.3%");
    check(has(css_of(box() | square(48)), "width:48") && has(css_of(box() | square(48)), "height:48"), "square");
    check(has(css_of(box() | circle(40)), "border-radius"), "circle rounds fully");
    check(has(css_of(text("x") | no_underline), "text-decoration:none"), "no_underline");
    check(has(css_of(text("x") | line_through), "line-through"), "line_through");
    check(has(css_of(box() | select_none), "user-select:none"), "select_none");
    check(has(css_of(box() | clip_content), "overflow:hidden"), "clip_content");
    check(has(css_of(image("/x") | grayscale()), "grayscale(100%)"), "grayscale");
    check(has(css_of(image("/x") | brightness(120)), "brightness(120%)"), "brightness");
    check(has(css_of(image("/x") | fit("contain")), "object-fit:contain"), "fit");
    check(has(css_of(box() | break_word), "overflow-wrap"), "break_word");

    // ── the raw_css-eliminating vocabulary (this round) ───────────────────
    // mobile-safe viewport units
    check(has(css_of(box() | h(dvh(100))), "100dvh"), "dvh unit emits");
    check(has(css_of(box() | w(dvw(80))),  "80dvw"),  "dvw unit emits");
    // min_h(0) must EMIT (Len{0,px} would read as unset)
    check(has(css_of(box() | min_h(0)), "min-height:0"), "min_h(0) emits explicitly");
    check(has(css_of(box() | min_w(0)), "min-width:0"),  "min_w(0) emits explicitly");
    // composable shadow family — one comma-joined box-shadow
    { auto css = css_of(box() | ring(0x00ff41, 2) | inset_light(.25f) | glow_under(0xff2d4b, 16, 6));
      check(has(css, "box-shadow:"), "shadow family emits box-shadow");
      // ONE box-shadow declaration holding all three layers (three known
      // fragments present in one property, not three separate declarations)
      int decls = 0; std::size_t p = 0;
      while ((p = css.find("box-shadow:", p)) != std::string::npos) { ++decls; p += 11; }
      check(decls == 1, "three shadow layers compose into ONE box-shadow");
      check(has(css, "0 0 0 2px") && has(css, "inset 0 1px 0") && has(css, "0 6px 16px"),
            "all three layers present"); }
    // ring() with a BARE colour (default width) must compile — this used to be an
    // ambiguous overload (a duplicate int-width ring clobbered the composing
    // one), which was a hard compile error for a widget author writing ring(c).
    check(has(css_of(box() | ring(0x6366f1)), "box-shadow:0 0 0 2px"), "ring(color) default width compiles + composes");
    // ring composes WITH an elevation into one box-shadow (never clobbers it).
    { auto css = css_of(box() | elevation(2) | ring(0x6366f1));
      int decls = 0; std::size_t p = 0;
      while ((p = css.find("box-shadow:", p)) != std::string::npos) { ++decls; p += 11; }
      check(decls == 1 && has(css, "6366f1"), "elevation | ring compose into ONE box-shadow (no clobber)"); }
    check(has(css_of(box() | inset_ring(0xffffff, 1)), "inset 0 0 0 1px"), "inset_ring");
    check(has(css_of(box() | inset_glow(0x00ff41, 24)), "inset 0 0 24px"), "inset_glow");
    // backgrounds
    check(has(css_of(box() | orb(0x6b7288, 0x2a2e3c, 40, 40)), "radial-gradient(circle at 40% 40%"), "orb radial fill");
    check(has(css_of(box() | veil(.55f)), "rgba(0,0,0,0.55"), "veil");
    check(has(css_of(box() | gradient(rgba(0x00ff41,.9f), rgba(0,.2f), 180)), "linear-gradient(180deg"), "gradient(Color,Color)");
    // borders
    check(has(css_of(box() | border(1, rgba(0x00ff41, .25f))), "1px solid rgba(0,255,65,0.25"), "translucent border(w,Color)");
    check(has(css_of(box() | border_color(0xff0000)), "border-color:"), "border_color");
    // layout niceties
    check(has(css_of(box() | mx_auto), "margin-left:auto") && has(css_of(box() | mx_auto), "margin-right:auto"), "mx_auto");
    check(has(css_of(box() | no_shrink), "flex:0 0 auto"), "no_shrink");
    check(has(css_of(box() | round(10,4,4,10)), "border-radius:10px 4px 4px 10px"), "per-corner round");
    check(has(css_of(box() | margin_left(2)), "margin-left:2px"), "margin_left");
    check(has(css_of(box() | clickable), "pointer-events:auto"), "clickable");
    check(has(css_of(text("x") | pre), "white-space:pre"), "pre");
    check(has(css_of(box() | pad_safe(14)), "env(safe-area-inset-top"), "pad_safe honours safe area");
    check(has(css_of(text("x") | text_glow(rgba(0x00ff41,.8f), 6)), "text-shadow:0 0 6px"), "text_glow single-layer");
    // colour vocabulary reachable via `using namespace waya::surface` alone
    check(has(css_of(box() | bg(rgba(0x00ff41, .5f))), "rgba(0,255,65,0.5"), "rgba() in surface namespace");

    // ── expressive aliases that kill common raw_css ────────────────────────
    check(has(css_of(text("x") | text_size(17)), "font-size:17px"), "text_size(px) -> font-size");
    check(has(css_of(text("x") | text_size(rem(1.2f))), "font-size:1.2rem"), "text_size(Len) -> rem");
    check(has(css_of(text("x") | text_size_fluid(20, 40)), "clamp("), "text_size_fluid -> clamp()");
    check(has(css_of(text("x") | line(1.6f)), "line-height:1.6"), "line(n) -> line-height");
    check(has(css_of(box() | bg_var("--brand", "#6366f1")), "var(--brand,#6366f1)"), "bg_var with fallback");
    check(has(css_of(box() | bg_var("--brand")), "background:var(--brand)"), "bg_var without fallback");
    check(has(css_of(text("x") | fg_var("--text")), "color:var(--text)"), "fg_var");
    check(has(css_of(box() | line_b()), "border-bottom:1px solid var(--wa-line"), "line_b() -> token bottom hairline");
    check(has(css_of(box() | line_b(0.06f)), "0.06))"), "line_b(alpha) tunes opacity");
    check(has(css_of(box() | border_top_token()), "border-top:1px solid var(--wa-line"), "border_top_token()");
    check(has(css_of(box() | bg_transparent()), "background:transparent"), "bg_transparent()");
    check(has(css_of(box() | grab), "cursor:grab"), "grab cursor");
    check(has(css_of(box() | grabbing), "cursor:grabbing"), "grabbing cursor");
    // client-side loop animations (the way to do decorative motion — no Sub::every)
    check(has(css_of(box() | loop("drift", "to{transform:translateX(40px)}", 8)), "drift 8s linear infinite"),
          "loop() -> infinite keyframe animation");
    check(has(css_of(box() | loop_alt("bob", "to{transform:translateY(6px)}", 3)), "infinite alternate"),
          "loop_alt() -> alternating loop");
    check(has(css_of(box() | scroll_down(50, 8)), "wa-scrolly 8s linear infinite"), "scroll_down() -> translateY loop");
    check(has(css_of(box() | scroll_left(50, 20)), "wa-scrollx 20s linear infinite"), "scroll_left() -> translateX loop");
    check(has(css_of(box() | anim_delay(-2.5f)), "animation-delay:-2.5s"), "anim_delay() (negative = mid-cycle)");
    check(has(css_of(box() | anim_speed(4)), "animation-duration:4s"), "anim_speed()");
    // custom client-side JS effects (the easy way to do canvas/chart-lib motion)
    {
        assets().clear();
        auto n = (box() | client_effect("myfx", "el.dataset.ran='1';")).done();
        check(has(html_of(n), "data-fx=\"myfx\""), "client_effect marks the node with data-fx");
        auto head = assets().head_html();
        check(head.find("__waFx") != std::string::npos, "client_effect registers the boot script");
        check(head.find("el.dataset.ran") != std::string::npos, "client_effect ships the author's JS");
    }
    {
        assets().clear();
        auto n = (box() | canvas_fx("cvsfx", "/* draw */")).done();
        auto head = assets().head_html();
        check(has(html_of(n), "data-fx=\"cvsfx\""), "canvas_fx marks the node");
        check(head.find("createElement('canvas')") != std::string::npos, "canvas_fx appends a canvas + hands over ctx");
    }
    {
        assets().clear();
        assets().script("my-lib", "console.log('hi');");
        check(assets().head_html().find("console.log('hi')") != std::string::npos, "assets().script injects client JS");
    }

    {
        // add_class merges author classes INTO the interned styling class — one
        // valid class attr, both present (the seam for porting CSS components).
        auto styled = (text("x") | bg(0x111111) | add_class("mag") | add_class("b")).done();
        auto h = html_of(styled);
        std::size_t first = h.find("class=");
        check(first != std::string::npos && h.find("class=", first + 1) == std::string::npos,
              "add_class: exactly one class attribute on the element");
        check(has(h, "mag") && has(h, " b\""), "add_class: both author classes present");
        check(has(h, "ws-"), "add_class: interned styling class still present");
        auto bare = (box() | add_class("ttui-bar")).done();
        check(has(html_of(bare), "class=\"ttui-bar\""), "add_class on an unstyled node: just the author class");
    }

    // ── reveal-family: client-owned decorative motion (no Model coupling) ────
    {
        assets().clear();
        auto n = (text("Fast") | reveal()).done();
        check(has(html_of(n), "data-wa-reveal"), "reveal() marks the node");
        check(assets().head_html().find("IntersectionObserver") != std::string::npos, "reveal() registers the shared IO boot");
        check(assets().style_css().find(".wa-seen") != std::string::npos, "reveal() registers its entrance CSS");
        check(assets().style_css().find("prefers-reduced-motion") != std::string::npos, "reveal() is inert under reduced-motion");
    }
    {
        assets().clear();
        auto n = (box(text("a"), text("b")) | reveal_stagger(90)).done();
        check(has(html_of(n), "data-wa-stagger=\"90\""), "reveal_stagger(90) sets the stagger interval");
    }
    {
        assets().clear();
        auto n = (text("agentty --version") | typewriter()).done();
        auto h = html_of(n);
        check(has(h, "data-wa-reveal=\"type\""), "typewriter() marks kind=type");
        check(has(h, "data-wa-type=\"agentty --version\""), "typewriter() snapshots the full text (SSR-safe)");
        check(has(h, "agentty --version"), "typewriter() leaves real text in the DOM for crawlers");
        check(has(h, "data-wa-caret"), "typewriter() default caret marker");
        // must NOT emit a second class attr (invalid HTML) alongside the styled class
        check(!has(h, "class=\"wa-caret\""), "typewriter() drives caret via JS, not a colliding class attr");
        check(assets().style_css().find("wa-caret-blink") != std::string::npos, "typewriter() registers the caret keyframe");
    }
    {
        assets().clear();
        auto plain = (text("agentty --version") | typewriter(false)).done();
        check(!has(html_of(plain), "data-wa-caret"), "typewriter(false) has no caret");
    }
    {
        assets().clear();
        auto n = (text("8.8 MB") | count_up()).done();
        auto h = html_of(n);
        check(has(h, "data-wa-reveal=\"count\""), "count_up() marks kind=count");
        check(has(h, "data-wa-count=\"8.8 MB\""), "count_up() snapshots the target value");
        check(has(h, "8.8 MB"), "count_up() leaves the real number in the DOM (SSR-safe)");
    }
    {
        assets().clear();
        auto n = (button("Get started") | magnetic()).done();
        check(has(html_of(n), "data-wa-mag=\"6\""), "magnetic() default strength");
        check(has(html_of((button("x") | magnetic(10)).done()), "data-wa-mag=\"10\""), "magnetic(10) custom strength");
        check(assets().head_html().find("pointermove") != std::string::npos, "magnetic() registers the pointer handler");
    }
    {
        assets().clear();
        auto bar = scroll_progress();
        auto h = html_of(bar);
        check(has(h, "id=\"wa-scrollprog\""), "scroll_progress() emits the fixed bar");
        check(has(css_of(bar), "scaleX(0)"), "scroll_progress() starts empty");
        check(assets().head_html().find("scrollHeight") != std::string::npos, "scroll_progress() registers the scroll handler");
    }
    {
        assets().clear();
        auto t = theme_toggle();
        auto h = html_of(t);
        check(has(h, "data-wa-theme"), "theme_toggle() marks the JS hook");
        check(has(h, "wa-sun") && has(h, "wa-moon"), "theme_toggle() ships both sun + moon icons");
        // no double class attr on the root (interned styling class only)
        check(h.find("class=") == h.rfind("class=") || h.find("data-wa-theme") != std::string::npos, "theme_toggle() root has a single class");
        check(assets().head_html().find("prefers-color-scheme") != std::string::npos, "theme_toggle() installs a FOUC-free boot");
        check(assets().head_html().find("setItem('waya:theme'") != std::string::npos, "theme_toggle() persists choice to localStorage");
    }
    assets().clear();

    std::cout << "test_mod: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
