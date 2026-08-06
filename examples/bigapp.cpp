/// examples/bigapp.cpp — proof that a HUGE, multi-screen app stays EASY in waya.
///
///   cmake --build build -j && ./build/bigapp     # http://localhost:8080
///
/// A little SaaS: a dashboard, a users list, a user detail page (with a URL
/// param), and a settings screen — wired with a real router, and an `update`
/// split into self-contained FEATURE MODULES via combine(). Each feature owns a
/// msg-id block and a slice of the model, so nothing collides and no function is
/// large. Add the 40th screen the same way — the top-level update stays a table.

#include <waya/surface/live.hpp>
#include <waya/surface/sugar.hpp>
#include <waya/surface/layout.hpp>
#include <waya/surface/router.hpp>
#include <waya/surface/scale.hpp>

#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::surface::color;

// ── Screens (route ids) + a router table. Add a screen = add one .at line. ────
enum Screen { Home, Users, UserView, Settings, NotFound };

static Router routes() {
    return router()
        .at("/",              Home)
        .at("/users",         Users)
        .at("/users/:id",     UserView)     // :id captured for the detail page
        .at("/settings",      Settings);
}

// ── Feature msg blocks — each feature owns 100 ids, so they never collide ─────
enum : int { NavBase = 100, SettingsBase = 200 };

struct App {
    struct User { int id; std::string name, role; };
    struct Model {
        Screen screen = Home;
        std::string user_id;                 // route param for /users/:id
        std::vector<User> users = {
            {1,"Ada Lovelace","Engineer"}, {2,"Alan Turing","Researcher"},
            {3,"Grace Hopper","Architect"}, {4,"Katherine Johnson","Analyst"},
        };
        bool dark = true, notify = true;     // settings feature's slice
    };
    using Msg = int;

    static Model init() { return {}; }

    // ── SEO: crawlers get real HTML (SSR) + the right <head> per route ────────
    static const char* site_url() { return "https://saas.example"; }
    static std::vector<std::string> sitemap() { return {"/", "/users", "/settings"}; }
    static Meta meta(const Model& m) {
        std::string base = site_url();
        switch (m.screen) {
            case Users:    return { .title="Team · SaaS", .description="Everyone on the team and their roles.",
                                    .canonical=base+"/users", .site_name="SaaS" };
            case UserView: {
                const User* u=nullptr; for(auto&x:m.users) if(std::to_string(x.id)==m.user_id) u=&x;
                std::string who = u? u->name : "User";
                return { .title=who+" · SaaS", .description=who+(u?", "+u->role:"")+" — team profile.",
                         .canonical=base+"/users/"+m.user_id, .type="profile", .site_name="SaaS",
                         .json_ld = u ? jsonld("Person", {{"name",u->name},{"jobTitle",u->role}}) : "" };
            }
            case Settings: return { .title="Settings · SaaS", .description="Manage your preferences.",
                                    .canonical=base+"/settings", .robots="noindex" };  // private page
            default:       return { .title="SaaS — ships software", .description="A tiny SaaS built with waya.",
                                    .canonical=base+"/", .site_name="SaaS",
                                    .json_ld = jsonld("Organization", {{"name","SaaS"},{"url",base}}) };
        }
    }

    // Route the URL → screen + params. One place, driven by the router table.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_route([](std::string){ return 0; });  // 0 = "route changed"
    }

    // ── feature reducers: each small, self-contained, sees LOCAL msg ids ──────
    static Cmd<Msg> nav_update(Model& m, int local, const std::string&) {
        // local 0..2 = top-nav links; local 20+i = open user row i (→ /users/:id)
        const char* paths[] = {"/", "/users", "/settings"};
        if (local >= 0 && local < 3) return Cmd<Msg>::navigate(paths[local]);
        if (local >= 20) {
            std::size_t i = (std::size_t)(local - 20);
            if (i < m.users.size()) return Cmd<Msg>::navigate("/users/" + std::to_string(m.users[i].id));
        }
        return Cmd<Msg>::none();
    }
    static Cmd<Msg> settings_update(Model& m, int local, const std::string&) {
        if (local == 0) m.dark = !m.dark;
        if (local == 1) m.notify = !m.notify;
        return Cmd<Msg>::none();
    }

    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg, std::string value) {
        // msg 0 = route change: match the path → screen + param. One line.
        if (msg == 0) {
            auto match = routes().match(value);
            m.screen  = match.matched ? (Screen)match.value : NotFound;
            m.user_id = match.param("id");
            return { m, Cmd<Msg>::none() };
        }
        // everything else: route to the owning feature. The whole top-level
        // update is this table — add a feature, add a line.
        return combine(std::move(m), msg, value,
            feature<Model>(NavBase,      nav_update),
            feature<Model>(SettingsBase, settings_update));
    }

    // ── screen views: each a small function ───────────────────────────────────
    static NodeRef nav(const Model& m) {
        auto link = [&](std::string t, int local, Screen active) {
            return text(std::move(t)) | (m.screen==active ? fg(ink) : fg(muted))
                 | (m.screen==active ? semibold : noop) | tap(NavBase + local)
                 | pad_x(10) | pad_y(8) | round(8) | transition() | on(Hover, fg(ink));
        };
        return row(
            text("SaaS") | fg_primary | heading,
            row(link("Home",0,Home), link("Users",1,Users), link("Settings",2,Settings)) | gap(4)
        ) | between | wrap | pad_y(8) | as_nav | css("border-bottom","1px solid #1f2937");
    }

    static NodeRef home_screen(const Model& m) {
        return col(
            text("Dashboard") | display | fg(ink) | heading_level(1) | fade_up(),
            row(
                card_stat("Users", std::to_string(m.users.size())),
                card_stat("Plan", "Pro"),
                card_stat("Status", "Live")
            ) | gap(16) | wrap
        ) | gap(20);
    }
    static NodeRef card_stat(std::string label, std::string value) {
        return col(text(std::move(label)) | fg(muted) | caption,
                   text(std::move(value)) | fg(ink) | heading)
             | gap(4) | pad(20) | round(16) | bg(bg1) | border(1,line) | grow(1) | elevation(2);
    }

    static NodeRef users_screen(const Model& m) {
        return col(
            text("Users") | display | fg(ink) | heading_level(1),
            col_(each_i(m.users, [](const User& u, std::size_t i){
                return row(
                    text(u.name) | fg(ink) | body | grow(1),
                    text(u.role) | fg(muted) | caption,
                    text("view →") | fg_primary | caption
                ) | gap(12) | pad(14) | round(12) | bg(bg1) | border(1,line)
                  | tap(NavBase + 20 + (int)i)          // open /users/<id>
                  | css("cursor","pointer") | transition() | on(Hover, border(1, brand));
            })) | gap(8)
        ) | gap(20);
    }

    static NodeRef user_view_screen(const Model& m) {
        const User* u = nullptr;
        for (auto& x : m.users) if (std::to_string(x.id) == m.user_id) u = &x;
        return col(
            text(u ? u->name : "Unknown user") | display | fg(ink) | pop_in(),
            text(u ? u->role : "\u2014") | fg(muted) | subtitle,
            text("id: " + m.user_id) | fg(faint) | caption | mono
        ) | gap(12) | pad(24) | round(16) | bg(bg1) | border(1,line);
    }

    static NodeRef settings_screen(const Model& m) {
        auto toggle = [](std::string label, bool on, int local){
            return row(text(std::move(label)) | fg(ink) | body | grow(1),
                       checkbox(on) | on_change(SettingsBase+local) | size(px(20)))
                 | gap(16) | pad_y(12);
        };
        return col(
            text("Settings") | display | fg(ink) | heading_level(1),
            card( toggle("Dark mode", m.dark, 0), toggle("Notifications", m.notify, 1) )
        ) | gap(20);
    }
    template <typename... Cs> static NodeRef card(Cs... cs){ return col(std::move(cs)...) | gap(4) | pad(20) | round(16) | bg(bg1) | border(1,line); }

    static NodeRef view(const Model& m) {
        auto body = screens((int)m.screen, {
            {Home,     [&]{ return home_screen(m); }},
            {Users,    [&]{ return users_screen(m); }},
            {UserView, [&]{ return user_view_screen(m); }},
            {Settings, [&]{ return settings_screen(m); }},
            {NotFound, [&]{ return text("404 \u2014 not found") | fg(muted) | display; }},
        });
        return page(bg0, centered(64, col(nav(m), col(body) | as_main) | gap(28)))
             | theme(Theme::dark());   // resolve the fg_primary/etc tokens
    }
};

int main() {
    static_assert(SurfaceProgram<App>);
    return live<App>({.port = 8080, .title = "waya SaaS"});
}
