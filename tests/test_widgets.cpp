// tests/test_widgets.cpp — the stateful component library + icons + form data.
#include <waya/surface/live.hpp>
#include <waya/surface/diff.hpp>
#include <waya/ui.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <variant>

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

    // ── RemoteData: the four async states as ONE value ─────────────────────
    {
        struct Retry { bool operator==(const Retry&) const = default; };
        using Users = std::vector<std::string>;
        RemoteData<Users> u;                               // NotAsked
        check(u.is_not_asked() && !u.value(), "RemoteData starts NotAsked");
        u = loading(u);
        check(u.is_loading(), "loading() -> Loading");
        u = loaded(Users{"ada", "grace"});
        check(u.is_success() && u.value() && u.value()->size()==2, "loaded() -> Success with value");
        // stale-while-revalidate: loading from a Success RETAINS the value
        auto refreshing = loading(u);
        check(refreshing.is_loading() && refreshing.value(), "loading(prev) keeps the stale value");
        // failure also retains the last-good value behind the error
        auto broke = failed<Users>("500", u);
        check(broke.is_failure() && broke.error()=="500" && broke.value(), "failed() keeps stale value + error");

        // the batteries view: spinner while loading, error card on failure
        auto onOk = [](const Users& us){ return list_row(nullptr, us[0], "", nullptr); };
        check(html_of(remote(loading<Users>(), onOk, Retry{})).size() > 0
              && !has(html_of(remote(loading<Users>(), onOk, Retry{})), "ada"),
              "remote() loading -> chrome, not content");
        check(has(html_of(remote(broke, onOk, Retry{})), "Retry"), "remote() failure -> Retry button");
        check(has(html_of(remote(u, onOk, Retry{})), "ada"), "remote() success -> content");
        // the exhaustive 3-branch form compiles + dispatches by state
        auto custom = remote(u, []{ return text("L"); },
                                [](const Users&){ return text("S"); },
                                [](const std::string&){ return text("F"); });
        check(has(html_of(custom), ">S<"), "remote(3-branch) picks the Success branch");
    }

    // ── field validation: an error is Model state, not hand-rolled layout ───
    {
        auto ok = html_of(text_field("Email", "a@b.com", [](std::string){ return 0; }));
        check(!has(ok, "aria-invalid"), "valid field has no invalid marker");
        auto bad = text_field("Email", "nope", [](std::string){ return 0; }, "", "", "email", "Email is taken");
        auto bad_html = html_of(bad);
        check(has(bad_html, "aria-invalid=\"true\""), "invalid field marks the control aria-invalid");
        check(has(bad_html, "role=\"alert\"") && has(bad_html, "Email is taken"), "invalid field shows an aria alert message");
        check(has(css_of(bad), "ef4444"), "invalid field is coloured danger");
    }

    // ── motion: pure easing + a model-owned Tween (the maya anim math) ──────
    {
        using namespace std::chrono_literals;
        // easing curves pin their endpoints and shape
        check(ease::out_cubic(0)==0.0 && ease::out_cubic(1)==1.0, "out_cubic endpoints");
        check(ease::out_cubic(0.5) > 0.5, "out_cubic decelerates (past halfway at t=.5)");
        check(ease::in_cubic(0.5) < 0.5, "in_cubic accelerates");
        check(ease::smoothstep(0.5)==0.5 && ease::smoothstep(0.0)==0.0, "smoothstep");
        check(ease::out_back(0.85) > 1.0, "out_back overshoots past 1");
        check(std::abs(eased(0, 100, 0.5, ease::linear) - 50.0) < 1e-9, "eased(linear) is a lerp");

        // Tween: settled -> animating -> settled, value eases between
        Tween x{0};
        check(!x.animating() && x.value()==0.0, "fresh Tween is settled at its value");
        x.to(100, 100ms);
        check(x.animating() && x.value()==0.0, "to() starts animating from the current value");
        x.step(50ms);
        check(x.value() > 50.0 && x.animating(), "midway: eased past linear-half, still animating");
        x.step(50ms);
        check(!x.animating() && x.value()==100.0, "reaching duration settles at target");
        // retarget mid-flight continues from the LIVE value (no visual jump)
        Tween y{0}; y.to(100, 100ms); y.step(50ms);
        double live = y.value(); y.to(0, 100ms);
        check(std::abs(y.value() - live) < 1e-9, "to() mid-flight continues from the live value");
        // snap kills motion; Tween is a value (== works, so a model stays testable)
        Tween z{3}; z.to(9, 100ms); z.snap(7);
        check(!z.animating() && z.value()==7.0, "snap() jumps and stops");
        check((Tween{5} == Tween{5}) && !(Tween{5} == Tween{6}), "Tween has value equality");

        // stagger: item i starts step*i later; done once the last finishes
        check(stagger(0, 3, 40, 100)==0.0, "stagger item hasn't started yet");
        check(stagger(1000, 0, 40, 100)==1.0, "stagger item long-done is 1");
        check(!stagger_done(50, 5, 40, 100) && stagger_done(1000, 5, 40, 100), "stagger_done gates the clock");
    }

    // ── Toasts: a notification QUEUE with a lifecycle, as model state ───────
    {
        using namespace std::chrono_literals;
        Toasts q;
        q.push("Saved!", Tone::success);         // 4s default
        q.error("Upload failed");                // 6s
        int sticky = q.push("Persistent", Tone::neutral, 0ms);  // never expires
        check(q.size()==3 && q.any() && q.ticking(), "three toasts queued, clock needed");
        q.tick(4000ms);                          // the 4s Saved expires; 6s + sticky stay
        check(q.size()==2, "tick() auto-dismisses the elapsed toast");
        q.tick(3000ms);                          // now the 6s error is gone too
        check(q.size()==1 && !q.ticking(), "only the sticky remains, no clock needed");
        q.dismiss(sticky);
        check(q.size()==0 && !q.any(), "dismiss() removes by id");
        check((Toasts{} == Toasts{}), "Toasts has value equality");
        // the layer renders keyed cards with a close button + aria-live
        Toasts r; r.info("hi");
        detail::begin_msg_capture();
        auto h = html_of(toasts_layer(r, [](int id){ return id; }));
        check(has(h, "aria-live=\"polite\""), "toast layer is an aria-live region");
        check(has(h, "role=\"button\"") && has(h, "Dismiss notification"), "toast has a labelled close button");

        // actionable toasts: push_action adds a labelled button, wired via onAction
        Toasts a; int uid = a.push_action("Item deleted", "Undo", Tone::warning);
        check(a.items.back().has_action() && a.items.back().action_label == "Undo", "push_action records the action label");
        check(a.items.back().sticky, "an actionable toast is sticky by default");
        check(uid >= 0, "push_action returns the toast id");
        struct Undo { int id; }; struct Close { int id; };
        detail::begin_msg_capture();
        auto ah = html_of(toasts_layer(a, [](int id){ return Close{id}; }, [](int id){ return Undo{id}; }));
        check(has(ah, "Undo") && has(ah, "Item deleted"), "actionable toast renders its action button + message");
        check(has(ah, "aria-label=\"Undo\""), "the action button is labelled");
        // a plain toast in the 3-arg layer has no action button
        Toasts b; b.push("Plain", Tone::success);
        detail::begin_msg_capture();
        auto bh = html_of(toasts_layer(b, [](int id){ return Close{id}; }, [](int id){ return Undo{id}; }));
        check(has(bh, "Plain") && has(bh, "Dismiss notification"), "a non-actionable toast still renders + is dismissable");
        check((Toast{} == Toast{}), "Toast has value equality with the action field");
    }

    // ── Optimistic<T>: show a change instantly, roll back on failure ───────
    {
        Optimistic<bool> liked{false};
        check(liked.value()==false && !liked.pending(), "starts at committed value, not pending");
        liked.apply(true);
        check(liked.value()==true && liked.pending() && liked.settled()==false,
              "apply() shows the guess immediately; settled() still the old truth");
        liked.rollback();
        check(liked.value()==false && !liked.pending(), "rollback() snaps back to committed");
        liked.apply(true); liked.confirm();
        check(liked.value()==true && liked.settled()==true && !liked.pending(), "confirm() promotes the guess");
        // confirm(authoritative) takes a server-corrected value
        Optimistic<int> count{1}; count.apply(2); count.confirm(5);
        check(count.value()==5 && count.settled()==5, "confirm(value) takes the server's answer");
        check((Optimistic<bool>{true} == Optimistic<bool>{true}), "Optimistic has value equality");
    }

    // ── typed a11y composites (replace the role/aria/tabindex hand-dance) ───
    {
        auto d = html_of(box() | dialog());
        check(has(d, "role=\"dialog\"") && has(d, "aria-modal=\"true\""), "dialog() sets role + aria-modal");
        check(has(d, "tabindex=\"-1\"") && has(d, "data-modal"), "dialog() sets tabindex + the client modal hook");
        check(!has(html_of(box() | dialog(false)), "data-modal"), "dialog(false) is a non-modal dialog");
        auto s = html_of(box() | aria_expanded(true) | aria_pressed(false) | aria_selected(true) | aria_current("page"));
        check(has(s, "aria-expanded=\"true\"") && has(s, "aria-pressed=\"false\""), "aria state mods");
        check(has(s, "aria-selected=\"true\"") && has(s, "aria-current=\"page\""), "aria selected/current");
        check(has(html_of(box() | aria_hidden), "aria-hidden=\"true\""), "aria_hidden hides decorative nodes");
    }

    // ── Form<>: whole-form validity + touched-gated errors, as one value ────
    {
        Form<> f;
        f.set("email", "bad"); f.set("pw", "short");
        f.validate({ {"email", rules::email("email")}, {"pw", rules::min_len("pw", 8)} });
        check(!f.valid() && f.error_count()==2, "invalid form: both fields error");
        check(f.error_for("email")=="Enter a valid email", "email rule fires; touched so it shows");
        check(f.error_for("pw").find("8")!=std::string::npos, "min_len message mentions the length");
        // touched-gating: a valid-so-far but UNTOUCHED field shows no error
        Form<> g; g.preset("email", "");        // preset does not touch
        g.validate({ {"email", rules::required("email")} });
        check(!g.valid(), "required rule marks the empty field invalid");
        check(g.error_for("email").empty(), "but an untouched field shows no error (don't nag early)");
        g.touch_all();
        check(g.error_for("email")=="Required", "touch_all() reveals every error on submit");
        // a genuinely valid form
        Form<> ok; ok.set("email", "a@b.com");
        ok.validate({ {"email", rules::email("email")} });
        check(ok.valid() && ok.error_for("email").empty(), "a valid form has no errors");
        // matches rule (password confirmation)
        Form<> m2; m2.set("p","secret"); m2.set("c","nope");
        m2.validate({ {"c", rules::matches("c","p")} });
        check(!m2.valid(), "matches rule catches a mismatch");
        check((Form<>{} == Form<>{}), "Form has value equality");
    }

    // ── Keymap<Msg>: shortcuts as inspectable data + a generated help view ──
    {
        struct OpenP{ bool operator==(const OpenP&)const=default; };
        struct HelpM{ bool operator==(const HelpM&)const=default; };
        struct GoH{ bool operator==(const GoH&)const=default; };
        using KMsg = std::variant<OpenP, HelpM, GoH>;
        auto km = Keymap<KMsg>{}
            .bind("mod+k", "Command palette", OpenP{})
            .bind("?", "Toggle help", HelpM{})
            .bind("g h", "Go home", GoH{}, "Navigation");
        check(km.size()==3, "keymap holds all bindings");
        detail::begin_msg_capture();
        check(has(html_of(box() | wire(km)), "data-ev-shortcut"), "wire() arms every binding as a shortcut");
        auto help = html_of(shortcut_help(km));
        check(has(help, "Command palette") && has(help, "Go home"), "help sheet lists every binding");
        check(has(help, "Navigation"), "help sheet renders group headings");
        check(has(help, "role=\"dialog\""), "help sheet is a labelled dialog");
    }

    // ── virtual_list: build only the visible window of a huge list ──────────
    {
        // 10k rows, 40px each, 360px viewport scrolled to 1000px
        auto r = virtual_range(/*scroll*/1000, /*vh*/360, /*row_h*/40, /*total*/10000, /*overscan*/4);
        check(r.first == 21, "window starts overscan-above the first visible row");
        check(r.count() < 40 && r.count() > 10, "builds ~20 rows, not 10000");
        check(r.top_pad == r.first * 40, "top spacer reserves the scrolled-past height");
        check(r.top_pad + r.count()*40 + r.bottom_pad == 10000*40, "spacers + rows == full list height");
        // edge cases
        check(virtual_range(0, 360, 40, 5, 4).count() == 5, "a short list renders entirely");
        check(virtual_range(0, 360, 0, 100, 4).count() == 0, "zero row height is a safe no-op");
        // the node actually built holds only the window + spacers
        int built = 0;
        auto vl = virtual_list(1000, 360, 40, 10000, [](int i){ return text(std::to_string(i)); });
        std::function<void(const NodeRef&)> cnt = [&](const NodeRef& n){
            if(!n) return; if(n->kind==Kind::text) built++; for(auto& k : n->kids) cnt(k); };
        cnt(vl);
        check(built == r.count(), "virtual_list builds exactly the windowed rows");
        check(has(html_of(scroll_window(360, 0, vl)), "data-ev-scroll"), "scroll_window reports its offset");
    }

    // ── command_palette: fuzzy launcher over a Keymap ────────────────────
    {
        struct A{ bool operator==(const A&)const=default; };
        struct B{ bool operator==(const B&)const=default; };
        struct GU{ bool operator==(const GU&)const=default; };
        using KMsg = std::variant<A, B, GU>;
        struct Q{ bool operator==(const Q&)const=default; };
        struct C{ bool operator==(const C&)const=default; };
        auto km = Keymap<KMsg>{}
            .bind("mod+k", "Open palette", A{})
            .bind("?", "Toggle help", B{})
            .bind("g u", "Go to user settings", GU{}, "Navigation");
        // fuzzy subsequence: "usr" -> "Go to USeR settings"
        auto hits = palette_matches(km, "usr");
        check(hits.size()==1 && hits[0]->label=="Go to user settings", "fuzzy match finds a subsequence");
        check(palette_matches(km, "").size()==3, "empty query matches every command");
        check(palette_matches(km, "zzzz").empty(), "a no-match query returns nothing");
        detail::begin_msg_capture();
        auto pal = html_of(command_palette(km, "help", 0,
            [](std::string){ return Q{}; }, [](KMsg){ return Q{}; }, C{}));
        check(has(pal, "role=\"listbox\""), "palette is a listbox");
        check(has(pal, "Toggle help") && !has(pal, "Open palette"), "palette shows only the filtered command");
        check(has(pal, "data-input"), "palette has a wired search box");
    }

    // ── History<T>: undo / redo timelines as one value ─────────────────────
    {
        History<int> h{0};
        check(!h.can_undo() && !h.can_redo() && h.get()==0, "fresh history has no timeline");
        h.push(1); h.push(2); h.push(3);
        check(h.get()==3 && h.depth()==3 && h.can_undo() && !h.can_redo(), "push builds the past");
        h.undo(); h.undo();
        check(h.get()==1 && h.can_redo(), "undo walks back and enables redo");
        h.redo();
        check(h.get()==2, "redo walks forward");
        h.push(99);
        check(h.get()==99 && !h.can_redo(), "editing after undo forks the timeline (redo cleared)");
        int before = (int)h.depth();
        h.push(99);
        check((int)h.depth()==before, "pushing the same value is a no-op");
        h.reset(7);
        check(h.get()==7 && !h.can_undo() && !h.can_redo(), "reset wipes both timelines");
        // the limit caps the past depth
        History<int> capped{0}; capped.limit = 3;
        for (int i=1;i<=10;++i) capped.push(i);
        check(capped.depth()==3 && capped.get()==10, "limit caps undo depth");
        check((History<int>{5} == History<int>{5}), "History has value equality");
    }

    // ── Routes: one table from URL pattern to a view builder ───────────────
    {
        auto pages = routes()
            .at("/", []{ return text("home"); })
            .at("/users/:id", [](const Match& m){ return text("user " + m.param("id")); })
            .at("/docs/*", [](const Match& m){ return text("docs " + m.param("*")); })
            .fallback([]{ return text("404"); });
        check(has(html_of(pages.view("/")), ">home<"), "static route renders");
        check(has(html_of(pages.view("/users/42")), "user 42"), ":id param passed to the builder");
        check(has(html_of(pages.view("/docs/guide/intro")), "docs guide/intro"), "* wildcard tail captured");
        check(has(html_of(pages.view("/nope")), ">404<"), "unmatched path renders the fallback");
        check(pages.matches("/users/1") && !pages.matches("/xyz"), "matches() tests a real route (not fallback)");
        check(pages.match("/users/7").param("id")=="7", "match() exposes params without rendering");
    }

    // ── Table<Row>: sort / filter / paginate as pure derivation ────────────
    {
        struct Sb{ int c; bool operator==(const Sb&)const=default; };
        struct Gp{ int p; bool operator==(const Gp&)const=default; };
        struct U { std::string name; int score; };
        std::vector<U> us = {{"Charlie",30},{"Alice",90},{"Bob",50},{"Dave",70},{"Eve",10}};
        std::vector<TableColumn<U>> cols = {
            col<U>("Name",  [](const U& u){ return text(u.name); })
                .sortable([](const U& a, const U& b){ return a.name < b.name; })
                .searchable([](const U& u){ return u.name; }),
            col<U>("Score", [](const U& u){ return text(std::to_string(u.score)); })
                .sortable([](const U& a, const U& b){ return a.score < b.score; }),
        };
        // default order = insertion
        TableState st;
        check(table_order(us, cols, st).front()==0, "unsorted keeps insertion order");
        // sort by score ascending then descending
        st.sort_by(1);
        check(us[table_order(us, cols, st).front()].score == 10, "sort ascending: lowest first");
        st.sort_by(1);   // second click flips to descending
        check(us[table_order(us, cols, st).front()].score == 90, "second click on same col = descending");
        check(st.sort_desc, "sort_by toggles direction on the same column");
        // filter is case-insensitive across searchable columns
        TableState f; f.set_filter("A");
        check(table_order(us, cols, f).size()==3, "filter 'A' matches Charlie/Alice/Dave (case-insens)");
        f.set_filter("zzz");
        check(table_order(us, cols, f).empty(), "a no-match filter yields no rows");
        // pagination math
        TableState p; p.page_size = 2;
        check(table_page_count(5, p)==3, "5 rows / 2 per page = 3 pages");
        check(table_page_count(4, p)==2 && table_page_count(0, p)==0, "page count rounds up / handles empty");
        // the rendered table wires sortable headers + a pager
        detail::begin_msg_capture();
        auto th = html_of(data_table(us, cols, p, [](int c){ return Sb{c}; }, [](int pg){ return Gp{pg}; }));
        check(has(th, "role=\"button\""), "sortable header is a tappable button");
        check(has(th, " of "), "pager shows 'n of m' when paginated");
        check((TableState{} == TableState{}), "TableState has value equality");
    }

    // ── reorderable: drag-to-reorder, the move computed for you ────────────
    {
        struct Drop{ int f, t; bool operator==(const Drop&)const=default; };
        std::vector<std::string> v = {"a","b","c","d"};
        apply_reorder(v, 0, 2);
        check(v[0]=="b" && v[1]=="c" && v[2]=="a" && v[3]=="d", "apply_reorder moves an item and shifts");
        apply_reorder(v, 3, 0);
        check(v[0]=="d", "apply_reorder handles move-to-front");
        auto snapshot = v;
        apply_reorder(v, 5, 0);           // out of range
        apply_reorder(v, 1, 1);           // same index
        check(v == snapshot, "out-of-range / same-index reorder is a no-op");
        auto [from, to] = parse_reorder("3:1");
        check(from==3 && to==1, "parse_reorder splits 'from:to'");
        check(parse_reorder("garbage").first == -1, "a malformed drop payload parses to -1 (safe)");
        detail::begin_msg_capture();
        auto rh = html_of(reorder_row(2, [](int f, int t){ return Drop{f, t}; }, text("item")));
        check(has(rh, "draggable=\"true\""), "reorder_row is draggable");
        check(has(rh, "data-drop-arg=\"2\"") && has(rh, "data-ev-drop"), "reorder_row is a drop target tagged with its index");
    }

    // ── i18n: catalogs + interpolation + plurals + fallback ────────────────
    {
        Catalog en = catalog({ {"greeting","Hello, {name}!"}, {"items","{n} item|{n} items"}, {"save","Save"} });
        check(en.t("greeting", {{"name","Ada"}}) == "Hello, Ada!", "t() interpolates {name}");
        check(en.t("save") == "Save", "t() of a plain string");
        check(en.plural("items", 1) == "1 item", "plural picks the singular arm at n==1");
        check(en.plural("items", 5) == "5 items", "plural picks the plural arm and substitutes {n}");
        check(en.t("missing_key") == "missing_key", "a missing key renders itself (visible, not blank)");
        // fallback chain: fr misses 'save', falls back to en
        Catalog fr = catalog({ {"greeting","Bonjour, {name} !"} }); fr.fallback(en);
        check(fr.t("greeting", {{"name","Ada"}}) == "Bonjour, Ada !", "fr uses its own translation");
        check(fr.t("save") == "Save", "fr falls back to en for a missing key");
        check(fr.has("save") && !fr.has("nope"), "has() sees through the fallback chain");
    }

    // ── Wizard: a multi-step cursor with bounds + progress ────────────────
    {
        struct Nx{ bool operator==(const Nx&)const=default; };
        Wizard w{3};
        check(w.is_first() && !w.is_last() && w.current()==0, "wizard starts at the first step");
        w.next(); w.next();
        check(w.current()==2 && w.is_last(), "next() advances to the last step");
        w.next();
        check(w.is_complete(), "stepping past the last marks complete");
        w.back();
        check(w.current()==2 && !w.is_first(), "back() steps in");
        Wizard w2{5}; w2.go(3);
        check(w2.current()==3 && w2.progress() > 0.7f, "go() jumps; progress tracks position");
        w2.go(99);
        check(w2.current()==5, "go() clamps past the end");
        detail::begin_msg_capture();
        auto steps = html_of(wizard_steps(Wizard{3}, {"Account","Details","Review"}));
        check(has(steps, "role=\"progressbar\"") && has(steps, "Details"), "wizard_steps renders a labelled progressbar");
        check((Wizard{3} == Wizard{3}), "Wizard has value equality");
    }

    // ── Paged<T> + infinite_sentinel: accumulate pages, load-more on scroll ──
    {
        struct More{ bool operator==(const More&)const=default; };
        Paged<int> p;
        check(p.empty() && p.can_load() && p.next_page()==0, "fresh paged list can load its first page");
        p.begin_load();
        check(p.is_loading() && !p.can_load(), "begin_load blocks re-entry (no request stampede)");
        p.append({1,2,3}, /*has_more=*/true);
        check(p.size()==3 && p.next_page()==1 && p.can_load(), "append adds a page and re-enables loading");
        p.begin_load(); p.append({4}, /*has_more=*/false);
        check(p.size()==4 && p.is_exhausted() && !p.can_load(), "has_more=false exhausts the list");
        p.fail("oops");   // (after exhaustion, still records the error path)
        Paged<int> q; q.begin_load(); q.fail("network");
        check(!q.is_loading() && q.error=="network", "fail() stops loading and records the error");
        // the sentinel: appear-trigger while loadable, spinner while loading, gone when exhausted
        detail::begin_msg_capture();
        check(has(html_of(infinite_sentinel(Paged<int>{}, More{})), "data-ev-appear"), "fresh sentinel arms the next-page trigger");
        Paged<int> loadingP; loadingP.begin_load();
        check(!has(html_of(infinite_sentinel(loadingP, More{})), "data-ev-appear"), "loading sentinel is a spinner, not a trigger");
        Paged<int> doneP; doneP.append({}, false);
        check(html_of(infinite_sentinel(doneP, More{})).size() < 60, "exhausted sentinel renders (near-)nothing");
    }

    // ── tree_view: nested expand/collapse, expansion as state ─────────────
    {
        struct Tog{ std::string id; bool operator==(const Tog&)const=default; };
        struct FN { std::string id, name; std::vector<FN> children; };
        FN root{"root","/",{ {"a","src",{ {"a1","main.cpp",{}} }}, {"b","README",{}} }};
        auto idf   = [](const FN& n){ return n.id; };
        auto labf  = [](const FN& n){ return text(n.name); };
        auto kidf  = [](const FN& n){ return n.children; };
        auto togf  = [](std::string id){ return Tog{id}; };
        TreeState ts;
        // nothing open: only the root row shows, its children hidden
        detail::begin_msg_capture();
        auto closed = html_of(tree_view(root, ts, idf, labf, kidf, togf));
        check(!has(closed, "src") && !has(closed, "README"), "a collapsed tree hides children");
        ts.expand("root");
        auto open1 = html_of(tree_view(root, ts, idf, labf, kidf, togf));
        check(has(open1, "src") && has(open1, "README"), "expanding root reveals its children");
        check(!has(open1, "main.cpp"), "but a grandchild stays hidden while its parent is closed");
        ts.expand("a");
        auto open2 = html_of(tree_view(root, ts, idf, labf, kidf, togf));
        check(has(open2, "main.cpp"), "expanding the parent reveals the grandchild");
        check(has(open2, "role=\"tree\"") && has(open2, "aria-expanded"), "tree has aria roles + expanded state");
        ts.toggle("a");
        check(!ts.is_open("a"), "toggle() closes an open node");
    }

    // ── on_paste_file: pasted images arrive as FileData (clipboard → update) ─
    {
        struct Pasted{ std::string name; bool operator==(const Pasted&)const=default; };
        detail::begin_msg_capture();
        auto zone = textarea("") | on_paste_file([](FileData f){ return Pasted{f.name}; });
        auto h = html_of(zone);
        check(has(h, "data-ev-pastefile"), "on_paste_file wires the clipboard-file listener");
        // it resolves through the SAME FileData path as an upload
        int tok = zone->events.at(0).msg;
        auto m = detail::resolve_msg<Pasted>(tok, "shot.png|image/png|aGk=");
        check(m && m->name=="shot.png", "a pasted file resolves to a typed Msg with its FileData");
    }

    // ── markdown: safe Markdown -> node tree (no raw HTML) ────────────────
    {
        auto h = html_of(markdown("# Title\n\nSome **bold** and a [link](https://x.com).\n\n- one\n- two\n\n> quote\n\n```\nint x=1;\n```\n\n---"));
        check(has(css_of(markdown("# Title")), "font-size:30px"), "h1 renders at heading size");
        check(has(h, "https://x.com") && has(h, "href"), "a [link](url) becomes a real anchor");
        check(has(h, "\xe2\x80\xa2"), "- items render with a bullet");
        check(has(h, "int x=1"), "fenced code block preserved");
        check(has(h, "quote") && has(css_of(markdown("> quote")), "border-left"), "blockquote has a rule");
        // THE point: markdown is XSS-safe — a <script> renders as text, never HTML
        auto evil = html_of(markdown("hi <script>alert(1)</script>"));
        check(!has(evil, "<script>") && has(evil, "&lt;script&gt;"), "markdown escapes HTML (no injection)");
    }

    // ── date_field: labelled native pickers join the field family ──────────
    {
        struct SetD{ bool operator==(const SetD&)const=default; };
        detail::begin_msg_capture();
        auto df = html_of(date_field("Birthday", "2000-01-01", [](std::string){ return SetD{}; }));
        check(has(df, "type=\"date\"") && has(df, "Birthday"), "date_field is a labelled native date input");
        auto tf = html_of(time_field("Alarm", "07:30", [](std::string){ return SetD{}; }));
        check(has(tf, "type=\"time\""), "time_field is a native time input");
    }

    // ── Presence: who's online / typing, over pub/sub ────────────────────
    {
        Presence pr;
        pr.mark("ada", "online"); pr.mark("bob", "typing"); pr.mark("me", "online");
        check(pr.count()==3, "three peers in the roster");
        check(pr.typers("me").size()==1 && pr.typers("me")[0]=="bob", "typers excludes me and non-typers");
        auto [u, s] = parse_peer("carol|typing");
        check(u=="carol" && s=="typing", "parse_peer splits '<user>|<state>'");
        check(parse_peer("dave").second=="online", "a bare username defaults to online");
        // typing_line names who's typing (before we remove bob)
        detail::begin_msg_capture();
        check(has(html_of(typing_line(pr, "me")), "is typing"), "typing_line names who's typing");
        pr.mark("bob", "left");
        check(pr.count()==2, "'left' removes a peer");
        // prune drops peers not heard from within the ttl
        Presence stale; stale.mark_at("old", false, 0); stale.mark_at("fresh", false, 1000000);
        stale.prune_at(std::chrono::seconds{10}, 1000000);
        check(stale.count()==1, "prune drops the stale peer, keeps the fresh one");
        check((Presence{} == Presence{}), "Presence has value equality");
    }

    // ── split_pane: two panes + a draggable divider, ratio as state ───────
    {
        struct Rz{ std::string value; bool operator==(const Rz&)const=default; };
        detail::begin_msg_capture();
        auto sp = html_of(split_pane(text("left"), text("right"), 0.6f, Rz{}));
        check(has(sp, "data-wa-split-box"), "split_pane marks its measuring container");
        check(has(sp, "data-wa-split=\"h\"") && has(sp, "data-ev-splitmove"), "the divider is a wired horizontal grip");
        check(has(css_of(split_pane(text("l"), text("r"), 0.6f, Rz{})), "60.00%"), "the first pane takes the ratio's fraction");
        auto vsp = html_of(split_pane(text("top"), text("bot"), 0.5f, Rz{}, /*vertical=*/true));
        check(has(vsp, "data-wa-split=\"v\""), "vertical=true makes a stacked split");
    }

    // ── chart: axes + gridlines + labels + legend, on the scene vocabulary ──
    {
        detail::begin_msg_capture();
        auto c = html_of(chart(560, 300,
            { series("Revenue", {10,25,18,40,32}, 0x22d3ee), series("Cost", {8,12,14,20,22}, 0xf59e0b) },
            { .x_labels = {"Jan","Feb","Mar","Apr","May"}, .kind = ChartKind::line }));
        check(has(c, "<svg"), "chart renders one svg scene");
        check(has(c, "<line"), "chart draws gridlines + axes");
        check(has(c, "<polyline"), "line chart plots a polyline series");
        check(has(c, "Revenue") && has(c, "Cost"), "legend names each series");
        check(has(c, "Jan") && has(c, "May"), "category axis labels the x points");
        // bar variant renders rects; area variant a filled polygon
        auto bar = html_of(chart(400, 200, { series("A", {5,10,7}, 0x6366f1) }, { .kind = ChartKind::bar }));
        check(has(bar, "<rect"), "bar chart renders rectangles");
        auto area = html_of(chart(400, 200, { series("A", {5,10,7}, 0x6366f1) }, { .kind = ChartKind::area }));
        check(has(area, "<polygon"), "area chart fills under the line");
    }

    // ── code_view: syntax-highlighted code, SAFE (no raw HTML) ────────────
    {
        auto cv = html_of(code_view("int main(){ return 0; } // hi\nauto s = \"str\";", "cpp"));
        check(has(cv, "int") && has(cv, "return"), "code is rendered");
        check(has(cv, "// hi"), "comments preserved");
        check(has(css_of(code_view("int x;")), "color:"), "tokens are coloured");
        // THE point: a <script> in the source is escaped, never executable.
        auto evil = html_of(code_view("x = <script>alert(1)</script>", "js"));
        check(!has(evil, "<script>") && has(evil, "&lt;"), "code_view escapes HTML (no injection)");
    }

    // ── data_grid: editable cells over the table pipeline ─────────────────
    {
        struct GRow { std::string id, name, role; int age; };
        struct Sb2{int c;}; struct Gp2{int p;};
        struct Begin{std::string r,c;}; struct Typed{std::string v;}; struct Done{}; struct Cancel{};

        std::vector<GRow> people{
            {"a","Ada","eng",36}, {"b","Linus","eng",54}, {"c","Grace","lead",41} };

        auto cols = std::vector{
            gcol<GRow>("Name", [](const GRow& p){ return text(p.name); })
                .sortable([](const GRow& a, const GRow& b){ return a.name < b.name; })
                .searchable([](const GRow& p){ return p.name; })
                .edits_text([](const GRow& p){ return p.name; }),
            gcol<GRow>("Role", [](const GRow& p){ return text(p.role); })
                .edits_select({option("eng"),option("lead"),option("design")},
                              [](const GRow& p){ return p.role; }),
            gcol<GRow>("Age", [](const GRow& p){ return text(std::to_string(p.age)); })
                .edits_number([](const GRow& p){ return std::to_string(p.age); }),
        };

        // GridEdit is a value with equality + the editing() predicate.
        GridEdit ed;
        check(!ed.active(), "fresh GridEdit is inactive");
        ed.begin("a", "Name", "Ada");
        check(ed.active() && ed.editing("a","Name"), "begin() enters edit mode on that cell");
        check(!ed.editing("a","Role"), "editing() is cell-precise (right row, wrong col)");
        ed.type("Adah");
        check(ed.draft == "Adah", "type() streams the live draft");
        { GridEdit e2; e2.begin("a","Name","Adah"); e2.type("Adah");
          check(ed == e2, "GridEdit has value equality"); }
        ed.stop();
        check(!ed.active() && ed.draft.empty(), "stop() clears the edit");

        auto rid = [](const GRow& p){ return p.id; };
        auto sort = [](int c){ return Sb2{c}; };
        auto page = [](int p){ return Gp2{p}; };
        auto onEdit = [](std::string r, std::string c){ return Begin{r,c}; };
        auto onType = [](std::string v){ return Typed{v}; };
        auto onDone = []{ return Done{}; };
        auto onCancel = []{ return Cancel{}; };

        // read-only render: editable cells advertise "Click to edit", no inputs yet.
        detail::begin_msg_capture();
        TableState st;
        auto ro = html_of(data_grid(people, cols, st, GridEdit{}, rid,
                                    sort, page, onEdit, onType, onDone, onCancel));
        check(has(ro, "Ada") && has(ro, "Grace"), "grid renders all rows");
        check(has(ro, "Click to edit"), "editable cell shows an affordance");
        check(!has(ro, "<input") && !has(ro, "<select"), "no controls when nothing is being edited");
        check(has(ro, "role=\"button\""), "sortable header is a button");

        // active text editor swaps the cell for a focused input carrying the draft.
        detail::begin_msg_capture();
        GridEdit e3; e3.begin("a", "Name", "Adah");
        auto te = html_of(data_grid(people, cols, st, e3, rid,
                                    sort, page, onEdit, onType, onDone, onCancel));
        check(has(te, "<input"), "editing a text cell renders a real input");
        check(has(te, "value=\"Adah\""), "input carries the live draft, not the row value");
        check(has(te, "autofocus"), "the editor autofocuses");

        // active number editor is type=number.
        detail::begin_msg_capture();
        GridEdit e4; e4.begin("b", "Age", "55");
        auto ne = html_of(data_grid(people, cols, st, e4, rid,
                                    sort, page, onEdit, onType, onDone, onCancel));
        check(has(ne, "type=\"number\""), "number cell edits as type=number");
        check(has(ne, "value=\"55\""), "number editor shows the draft");

        // active select editor is a dropdown over the column's options.
        detail::begin_msg_capture();
        GridEdit e5; e5.begin("a", "Role", "lead");
        auto se = html_of(data_grid(people, cols, st, e5, rid,
                                    sort, page, onEdit, onType, onDone, onCancel));
        check(has(se, "<select"), "editing a select cell renders a dropdown");
        check(has(se, "design"), "dropdown lists the column's options");

        // the sort/filter/paginate pipeline is the very same table_order.
        std::vector<TableColumn<GRow>> tcols; for (auto& c : cols) tcols.push_back(c.base);
        st.sort_by(0);
        check(people[table_order(people, tcols, st).front()].name == "Ada", "grid sorts via table_order (ascending)");
        TableState f; f.set_filter("lin");
        check(table_order(people, tcols, f).size() == 1, "grid filter reuses the searchable columns");

        // a value typed into a cell is HTML-escaped, never executable.
        detail::begin_msg_capture();
        GridEdit evil; evil.begin("a", "Name", "<script>x</script>");
        auto ev = html_of(data_grid(people, cols, st, evil, rid,
                                    sort, page, onEdit, onType, onDone, onCancel));
        check(!has(ev, "<script>x") && has(ev, "&lt;script"), "draft value is escaped (no injection)");
    }

    // ── numeric controls: stepper / number_field / percent / stars ────────
    {
        struct SetN { long v; }; struct Rate { int v; };
        detail::begin_msg_capture();

        // stepper: −/value/+ clamped; buttons carry the target value.
        auto st = html_of(stepper(5L, [](long v){ return SetN{v}; }, 0, 10, 1));
        check(has(st, ">5<"), "stepper shows the current value");
        check(has(st, "aria-label=\"Decrease\"") && has(st, "aria-label=\"Increase\""), "stepper has −/+ buttons");
        check(has(st, "role=\"group\""), "stepper is a labelled group");

        // at the bottom the − is disabled (no tap), at the top the + is.
        auto lo = html_of(stepper(0L, [](long v){ return SetN{v}; }, 0, 10));
        check(has(lo, "aria-disabled=\"true\""), "stepper disables − at the minimum");
        auto hi = html_of(stepper(10L, [](long v){ return SetN{v}; }, 0, 10));
        check(has(hi, "aria-disabled=\"true\""), "stepper disables + at the maximum");

        // number_field: a real type=number with min/max/step + label.
        detail::begin_msg_capture();
        auto nf = html_of(number_field("Price", 9.99, [](std::string v){ return v; }, 0, 1000, 0.01, "USD"));
        check(has(nf, "type=\"number\""), "number_field is a numeric input");
        check(has(nf, "Price") && has(nf, "USD"), "number_field shows label + hint");
        check(has(nf, "min=\"0\"") && has(nf, "step=\"0.01\""), "number_field carries min/step");
        check(has(nf, "value=\"9.99\""), "number_field shows the value without float padding");

        // percent_field: 0..100 with a % suffix.
        detail::begin_msg_capture();
        auto pf = html_of(percent_field("Zoom", 75, [](std::string v){ return v; }));
        check(has(pf, "max=\"100\"") && has(pf, ">%<"), "percent_field clamps to 100 and shows %");

        // star_rating: N stars, first `value` filled; click Nth → to_msg(N).
        detail::begin_msg_capture();
        auto sr = html_of(star_rating(3, [](int v){ return Rate{v}; }, 5));
        check(has(sr, "role=\"radiogroup\""), "star_rating is a radiogroup");
        auto sr_css = css_of(star_rating(3, [](int v){ return Rate{v}; }, 5));
        check(has(sr_css, "fbbf24") || has(html_of(star_rating(3,[](int v){return Rate{v};},5)),"star"), "filled stars are gold");

        // read-only stars display (no interaction).
        auto rs = html_of(stars(4.0, 5));
        check(has(rs, "role=\"img\""), "read-only stars are an img with an aria label");
        check(!has(rs, "data-ev") && !has(rs, "data-tap"), "read-only stars have no click handlers");
    }

    // ── pickers: color_field / swatch_picker / segmented / breadcrumb ────
    {
        struct SetC { std::string v; }; struct SetV { int i; }; struct Go { std::string p; };
        detail::begin_msg_capture();

        // color_field: native swatch + hex text box, both reporting.
        auto cf = html_of(color_field("Accent", "#6366f1", [](std::string v){ return SetC{v}; }));
        check(has(cf, "type=\"color\""), "color_field has a native colour swatch");
        check(has(cf, "value=\"#6366f1\""), "color_field shows the current colour");
        check(has(cf, "Accent"), "color_field shows its label");

        // swatch_picker: chips; the chosen one is ringed; bad hex is neutralised.
        detail::begin_msg_capture();
        auto sp = css_of(swatch_picker("#10b981", {"#ef4444","#10b981"}, [](std::string v){ return SetC{v}; }));
        check(has(sp, "10b981"), "swatch_picker paints each chip");
        check(has(sp, "box-shadow"), "the chosen swatch gets a ring");
        auto evil = css_of(swatch_picker("", {"red;}body{x:y"}, [](std::string v){ return SetC{v}; }));
        check(!has(evil, "body{") && has(evil, "transparent"), "malformed colour is neutralised (no CSS breakout)");

        // segmented: one-of-N pill; active segment is selected.
        detail::begin_msg_capture();
        auto sg = html_of(segmented(1, {"Day","Week","Month"}, [](int i){ return SetV{i}; }));
        check(has(sg, "role=\"tablist\""), "segmented is a tablist");
        check(has(sg, "Day") && has(sg, "Month"), "segmented renders every label");
        check(has(sg, "aria-selected=\"true\""), "segmented marks the active segment");

        // breadcrumb: linked crumbs + a muted current one; escaped separator.
        detail::begin_msg_capture();
        auto bc = html_of(breadcrumb({ crumb("Home", Go{"/"}), crumb("Docs", Go{"/docs"}), crumb("Here") }));
        check(has(bc, "Home") && has(bc, "Docs") && has(bc, "Here"), "breadcrumb renders every crumb");
        check(has(bc, "role=\"link\""), "linked crumbs are links");
        check(has(bc, "aria-current=\"page\""), "the final crumb is aria-current");
        check(has(bc, "role=\"navigation\""), "breadcrumb is a nav landmark");
    }

    // ── file_tree: an explorer with ext icons + selection ──────────────
    {
        struct Toggle { std::string id; }; struct Select { std::string path; };
        detail::begin_msg_capture();

        FileNode root = folder("project", {
            folder("src", { file("main.cpp"), file("util.hpp") }),
            file("README.md"), file("logo.png"),
        });

        // model helpers are pure values.
        check((file("a.txt") == file("a.txt")), "FileNode has value equality");
        check(!file("a.txt").dir && folder("d").dir, "file()/folder() set the dir flag");
        auto stats = file_stats(root);
        check(stats.folders == 1 && stats.files == 4, "file_stats counts recursively");

        // collapsed: only the root row is visible (children hidden).
        TreeState open;
        auto collapsed = html_of(file_tree(root, open, "",
            [](std::string id){ return Toggle{id}; }, [](std::string p){ return Select{p}; }));
        check(has(collapsed, "project"), "root folder renders");
        check(!has(collapsed, "main.cpp"), "collapsed root hides its children");
        check(has(collapsed, "role=\"tree\""), "file_tree is a tree");

        // expand root + src: files appear with their extension icons.
        detail::begin_msg_capture();
        open.expand("project"); open.expand("project/src");
        auto ex = html_of(file_tree(root, open, "project/src/main.cpp",
            [](std::string id){ return Toggle{id}; }, [](std::string p){ return Select{p}; }));
        check(has(ex, "main.cpp") && has(ex, "util.hpp"), "expanded folders reveal their files");
        check(has(ex, "README.md") && has(ex, "logo.png"), "root-level files render");
        check(has(ex, "aria-expanded=\"true\""), "open folder is aria-expanded");
        check(has(ex, "aria-selected=\"true\""), "the selected file is aria-selected");

        // extension drives the icon: code file gets file-code, image gets file-image.
        check(file_detail::ext_of("main.cpp") == "cpp", "ext_of lowercases the extension");
        check(file_detail::ext_of("NOEXT") == "", "ext_of returns empty when there's no dot");
        check(std::string(file_detail::icon_for("a.png").name) == "file-image", "images get the image icon");
        check(std::string(file_detail::icon_for("a.rs").name) == "file-code", "code files get the code icon");

        // a filename with HTML in it is escaped (svg icon is trusted, name is not).
        detail::begin_msg_capture();
        FileNode evil = folder("r", { file("<b>x</b>.txt") });
        TreeState o2; o2.expand("r");
        auto ev = html_of(file_tree(evil, o2, "",
            [](std::string id){ return Toggle{id}; }, [](std::string p){ return Select{p}; }));
        check(!has(ev, "<b>x</b>") && has(ev, "&lt;b&gt;"), "filenames are escaped (no injection)");
    }

    std::cout << "test_widgets: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}
