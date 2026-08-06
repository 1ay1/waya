# Building huge apps in waya

A big app doesn't have to become a 3000-line `update` and a string-compare
routing ladder. waya gives you three primitives that keep every feature small
and independent, no matter how many you add.

## 1. Routing — a table, not `if`-chains

`router()` maps URL patterns to your screen ids. `:name` captures a segment; a
trailing `*` captures the rest.

```cpp
enum Screen { Home, Users, UserView, Settings, NotFound };

static Router routes() {
    return router()
        .at("/",            Home)
        .at("/users",       Users)
        .at("/users/:id",   UserView)     // captures {id}
        .at("/settings",    Settings);
}
```

Match a path once, in your route handler:

```cpp
static Sub<Msg> subscribe(const Model&) {
    return Sub<Msg>::on_route([](std::string){ return 0; });   // "route changed"
}
// in update(), msg 0 = route change:
auto m = routes().match(path);
model.screen  = m.matched ? (Screen)m.value : NotFound;
model.user_id = m.param("id");
```

Every route **first-paints on the server** (SSR), so deep links and refreshes
land on the right screen instantly.

## 2. Screens — render the matching view

`screens()` replaces the `when(route==A, …, when(route==B, …))` ladder with a
flat table:

```cpp
NodeRef view(const Model& m) {
    auto body = screens((int)m.screen, {
        {Home,     [&]{ return home_screen(m); }},
        {Users,    [&]{ return users_screen(m); }},
        {UserView, [&]{ return user_view_screen(m); }},
        {NotFound, [&]{ return not_found(); }},
    });
    return page(bg0, centered(64, col(nav(m), body)));
}
```

Each `*_screen(m)` is a small function — a component. Add the 40th screen by
adding one line to the table and one function.

## 3. Feature modules — split `update` so nothing collides

The scale-killer is one giant `Msg` enum + one giant `update`. Split the app
into **features**, each owning a contiguous block of msg ids and a slice of the
model. `combine()` routes each message to its owner; a feature's reducer sees
message ids **relative to its base** (0, 1, 2…), so it's written once and drops
in at any base.

```cpp
enum : int { NavBase = 100, CartBase = 200, AuthBase = 300 };   // 100 ids each

// each feature's reducer is small and only knows its own slice:
Cmd<int> cart_update(Model& m, int local, std::string v) {
    if (local == 0) m.cart.add(v);
    if (local == 1) m.cart.clear();
    return Cmd<int>::none();
}

// the WHOLE top-level update is a table — add a feature, add a line:
std::pair<Model, Cmd<int>> update(Model m, int msg, std::string value) {
    if (msg == 0) { /* route change */ }
    return combine(std::move(m), msg, value,
        feature<Model>(NavBase,  nav_update),
        feature<Model>(CartBase, cart_update),
        feature<Model>(AuthBase, auth_update));
}
```

A button in the cart fires `CartBase + 0`; the cart reducer sees `local == 0`.
No feature can accidentally handle another's messages — the id blocks are
disjoint. Each reducer stays ~50 lines.

## 4. Composition is still just functions

Components are functions returning `NodeRef` (see COMPONENTS.md), effects/subs
`map()` between a child's and a parent's Msg type, and `broadcast`/`on_topic`
share state across independent sessions. Nothing about "big" changes the model:
it's the same uniform vocabulary, sliced into features.

See `examples/bigapp.cpp` for a full multi-screen SaaS (dashboard + users list +
user detail with a `:id` param + settings) whose every screen and feature is a
small function, wired by the router + `combine`. Every route SSRs its own screen
and the whole thing passes the W3C validators with zero errors.
