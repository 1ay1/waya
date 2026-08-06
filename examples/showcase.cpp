// examples/showcase.cpp — a flagship tour of waya in one small app.
//
// A tiny "Signups" admin: two routed screens with query-param filtering, a real
// form with validation + a screen-reader-announced status, and an async fetch
// that handles HTTP status (not just a body). It shows off, in ~200 lines of
// three pure functions:
//
//   • ROUTING with :params AND ?query    (router.hpp — q()/has_q())
//   • FORMS: form(...) | on_submit → FormData, with validation + aria-live
//   • EFFECTS with STATUS: Cmd::fetch_full → Response{status, ok(), body}
//   • A11y: status()/alert() live regions announce every dynamic change
//   • LIVE UPDATES: minimal deltas stream on every keystroke/click, no JS
//
// Because it's pure, the WHOLE thing is unit-testable with test::harness<Showcase>
// and scrubbable with debug::timeline<Showcase> — no browser required.
//
//   cmake --build build --target showcase && ./build/showcase   # localhost:8080

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;
using namespace waya::surface::color;

struct Showcase {
    enum Screen { List, Add, NotFound };
    struct Person { std::string name, email, role; };

    struct Model {
        Screen screen = List;
        std::string filter;            // from ?q=… on the list screen
        std::string role_filter;       // from ?role=…
        std::string status;            // aria-live status line ("Added Ada", …)
        std::string error;             // form validation / fetch error
        bool submitting = false;       // an async "save" is in flight
        std::vector<Person> people{
            {"Ada Lovelace",   "ada@analytic.dev",  "Admin"},
            {"Grace Hopper",   "grace@navy.mil",     "Admin"},
            {"Linus Torvalds", "linus@kernel.org",   "Member"},
            {"Alan Turing",    "alan@bletchley.uk",  "Member"},
        };
        std::vector<Person> shown() const {
            std::vector<Person> out;
            auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; };
            std::string f = lc(filter), rf = role_filter;
            for (auto& p : people) {
                if (!f.empty() && lc(p.name).find(f) == std::string::npos &&
                                  lc(p.email).find(f) == std::string::npos) continue;
                if (!rf.empty() && p.role != rf) continue;
                out.push_back(p);
            }
            return out;
        }
    };

    // ── messages ─────────────────────────────────────────────────────────────
    struct Route { std::string path; };   // browser route changed
    struct Go    { std::string path; };   // request navigation
    struct Submit{ std::string body; };   // the add-form was submitted
    struct Saved { std::string name; };   // async "save" finished OK
    struct Failed{ std::string why;  };   // async "save" failed

    using Msg = std::variant<Route, Go, Submit, Saved, Failed>;

    static Model init() { return {}; }

    static Router routes() {
        return router().at("/", List).at("/add", Add).at("/*", NotFound);
    }

    // ── update ───────────────────────────────────────────────────────────────
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](const Go& g) -> std::pair<Model,Cmd<Msg>> {
                return { m, Cmd<Msg>::navigate(g.path) };
            },
            [&](const Route& r) -> std::pair<Model,Cmd<Msg>> {
                auto match = routes().match(r.path);
                m.screen      = match.matched ? (Screen)match.value : NotFound;
                m.filter      = match.q("q");         // ?q=…  → live filter
                m.role_filter = match.q("role");      // ?role=Admin → role facet
                m.error.clear();
                return { m, Cmd<Msg>::none() };
            },
            [&](const Submit& s) -> std::pair<Model,Cmd<Msg>> {
                auto f = FormData::parse(s.body);
                std::string nm = f.get("name"), email = f.get("email");
                // validation → an assertive alert() the screen reader announces
                if (nm.empty() || email.find('@') == std::string::npos) {
                    m.error = "Enter a name and a valid email.";
                    return { m, Cmd<Msg>::none() };
                }
                // optimistic add, then a real HTTP call whose STATUS matters
                m.submitting = true; m.error.clear(); m.status.clear();
                std::string role = f.checked("admin") ? "Admin" : "Member";
                m.people.push_back({ nm, email, role });
                return { m, Cmd<Msg>::fetch_full(
                    "https://httpbingo.org/status/200",
                    [nm](Cmd<Msg>::Response r) -> Msg {
                        if (r.status == 0)  return Failed{ "network error — is TLS built?" };
                        if (!r.ok())        return Failed{ "server said HTTP " + std::to_string(r.status) };
                        return Saved{ nm };
                    }) };
            },
            [&](const Saved& s) -> std::pair<Model,Cmd<Msg>> {
                m.submitting = false;
                m.status = "Added " + s.name + ".";
                return { m, Cmd<Msg>::navigate("/") };   // back to the list
            },
            [&](const Failed& f) -> std::pair<Model,Cmd<Msg>> {
                m.submitting = false;
                if (!m.people.empty()) m.people.pop_back();   // roll back the optimistic add
                m.error = f.why;
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_route([](std::string p){ return Route{ p }; });
    }

    // ── view ─────────────────────────────────────────────────────────────────
    static NodeRef navlink(std::string label, std::string path, bool active) {
        auto t = text(label) | tap(Go{ path })
            | fg(active ? ink : muted) | (active ? bold : medium)
            | pad_x(10) | pad_y(6) | round(8) | pointer;
        if (active) t = t | bg(0x1e293b);
        return t;
    }
    static NodeRef header(const Model& m) {
        return row(
            text("waya") | fg(brand) | bold | font(22),
            text("· signups") | fg(muted) | font(22),
            push(),
            navlink("People", "/",    m.screen == List),
            navlink("Add",    "/add", m.screen == Add)
        ) | as_nav | gap(12) | align(Align::center) | pad_y(14)
          | border_bottom(1, 0x1e293b);
    }

    static NodeRef person_row(const Person& p) {
        return row(
            avatar(std::string(1, p.name.empty() ? '?' : p.name[0])),
            col(
                text(p.name) | fg(ink) | semibold,
                text(p.email) | fg(muted) | font(13)
            ) | gap(2),
            push(),
            badge(p.role, p.role == "Admin" ? Tone::primary : Tone::neutral)
        ) | key(p.email) | gap(12) | align(Align::center) | pad(12) | round(12)
          | bg(0x0f172a) | border(1, 0x1e293b);
    }

    static NodeRef facet(std::string label, std::string path, bool active) {
        auto t = text(label) | tap(Go{ path }) | pointer
            | pad_x(12) | pad_y(6) | round(999) | font(13);
        return active ? (t | bg(brand) | fg(0xffffff)) : (t | bg(0x0f172a) | fg(muted));
    }
    static NodeRef list_screen(const Model& m) {
        auto rows = m.shown();
        std::vector<NodeRef> items;
        for (auto& p : rows) items.push_back(person_row(p));

        return col(
            row(
                text("People") | fg(ink) | bold | font(20),
                push(),
                text(std::to_string(rows.size()) + " shown") | fg(muted) | font(13)
            ) | align(Align::center),
            // role facets that just set ?role=… — deep-linkable filters
            row(
                facet("All",     "/",             m.role_filter.empty()),
                facet("Admins",  "/?role=Admin",  m.role_filter == "Admin"),
                facet("Members", "/?role=Member", m.role_filter == "Member")
            ) | gap(8),
            m.filter.empty() ? box()
                : text("filter: \"" + m.filter + "\"") | fg(muted) | font(13),
            rows.empty()
                ? (col(text("No matches.") | fg(muted)) | pad(24) | align(Align::center))
                : (col_(std::move(items)) | gap(10))
        ) | gap(16) | pad_y(20);
    }

    static NodeRef add_screen(const Model& m) {
        auto submit_btn = button(m.submitting ? "Saving…" : "Add person")
            | tap(0) | bg(brand) | fg(0xffffff) | pad_x(16) | pad_y(10) | round(10);
        if (m.submitting) submit_btn = submit_btn | disabled(true);

        return col(
            text("Add a person") | fg(ink) | bold | font(20),
            // a REAL <form>: named controls, gathered on submit into FormData
            form(
                field("Name",  input("") | name("name")  | placeholder("Ada Lovelace")),
                field("Email", input("") | name("email") | placeholder("ada@dev.io") | type("email")),
                row(checkbox(false) | name("admin"), text("Grant admin") | fg(muted))
                    | gap(8) | align(Align::center),
                row(
                    submit_btn,
                    text("Cancel") | tap(Go{ "/" }) | fg(muted) | pad(10) | pointer
                ) | gap(10) | align(Align::center)
            ) | on_submit([](std::string body){ return Submit{ body }; })
              | column | gap(14),
            // validation errors are announced ASSERTIVELY (role=alert)
            m.error.empty() ? box() : (alert(m.error) | fg(0xf87171))
        ) | gap(16) | pad_y(20) | max_w(460);
    }

    static NodeRef view(const Model& m) {
        auto body = screens((int)m.screen, {
            { List,     [&]{ return list_screen(m); } },
            { Add,      [&]{ return add_screen(m); } },
            { NotFound, [&]{ return col(
                                text("404 — no such page") | fg(ink) | font(20),
                                text("Back to people") | tap(Go{"/"}) | fg(brand) | pointer)
                                | gap(8) | pad_y(40); } },
        });

        // one polite live region carries every status update — a screen reader
        // hears "Added Ada." without the user doing anything.
        auto live = (m.status.empty() ? box() : (status(m.status) | fg(0x34d399) | font(14)))
                        | live_region();

        return col(
            header(m),
            body,
            live
        ) | pad_x(24) | max_w(760) | center_x
          | h_screen | bg(0x020617)
          | font_family("ui-sans-serif, system-ui, sans-serif");
    }
};

static_assert(SurfaceProgram<Showcase>);

int main() {
    return live<Showcase>({ .title = "waya · showcase" });
}
