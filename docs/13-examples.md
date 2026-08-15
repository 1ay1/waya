# Build a Complete App

The fastest way to learn waya is to build one real app end to end. This page is
a complete, copy-pasteable todo application — Model, Msg, update, view, an
effect, a subscription, client-side persistence, and SSR metadata — in one file.
Every waya app is this same shape; once you can read this, you can build anything.

> New to the setup? Do [Getting Started](01-getting-started.md) first (install +
> `waya new`). For the guided version with explanations at each step, see the
> [Todo Tutorial](tutorial-todo.md).

## The whole app

```cpp
#include <waya/surface/live.hpp>
#include <waya/ui.hpp>
using namespace waya::surface;
using namespace waya::ui;

// ── 1. State ────────────────────────────────────────────────────────────────
struct Todo { int id; std::string text; bool done; };

struct Model {
    std::vector<Todo> todos;
    std::string       draft;      // the text being typed
    int               next_id = 1;
};

// ── 2. Messages — every way the app can change ──────────────────────────────
struct Typed   { std::string v; };   // the input changed
struct Add     {};                    // add the draft as a todo
struct Toggle  { int id; };           // flip a todo's done
struct Remove  { int id; };           // delete a todo
struct Restore { std::string json; }; // localStorage sent us the saved list

using Msg = std::variant<Typed, Add, Toggle, Remove, Restore>;

// ── 3. A tiny (un)serialiser so we can persist across reloads ───────────────
static std::string dump(const std::vector<Todo>& ts);           // Todo[] -> string
static std::vector<Todo> parse(const std::string& s);           // string -> Todo[]

// ── 4. The app ──────────────────────────────────────────────────────────────
struct App {
    using Model = ::Model;
    using Msg   = ::Msg;

    static Model init() { return {}; }   // (a Cmd could load initial data here)

    // update: pure. Returns the next model + any effects to run.
    static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
        return std::visit(overload{
            [&](Typed t) -> std::pair<Model, Cmd<Msg>> {
                m.draft = t.v;
                return { m, Cmd<Msg>::none() };
            },
            [&](Add) -> std::pair<Model, Cmd<Msg>> {
                if (m.draft.empty()) return { m, Cmd<Msg>::none() };
                m.todos.push_back({ m.next_id++, m.draft, false });
                m.draft.clear();
                return { m, Cmd<Msg>::store("todos", dump(m.todos)) };   // persist
            },
            [&](Toggle t) -> std::pair<Model, Cmd<Msg>> {
                for (auto& td : m.todos) if (td.id == t.id) td.done = !td.done;
                return { m, Cmd<Msg>::store("todos", dump(m.todos)) };
            },
            [&](Remove r) -> std::pair<Model, Cmd<Msg>> {
                std::erase_if(m.todos, [&](auto& td){ return td.id == r.id; });
                return { m, Cmd<Msg>::store("todos", dump(m.todos)) };
            },
            [&](Restore r) -> std::pair<Model, Cmd<Msg>> {
                m.todos = parse(r.json);
                for (auto& td : m.todos) m.next_id = std::max(m.next_id, td.id + 1);
                return { m, Cmd<Msg>::none() };
            },
        }, msg);
    }

    // subscribe: on connect, replay the saved "todos" key from localStorage.
    static Sub<Msg> subscribe(const Model&) {
        return Sub<Msg>::on_storage([](std::string key, std::string val) -> Msg {
            return key == "todos" ? Msg{Restore{val}} : Msg{Typed{""}};  // ignore others
        });
    }

    // view: pure function of the model. Rebuilt every frame; the diff makes it cheap.
    static NodeRef view(const Model& m) {
        int left = 0; for (auto& t : m.todos) if (!t.done) ++left;

        auto rows = each(m.todos, [](const Todo& t) {
            return row(
                checkbox(t.done) | on_change([id = t.id](std::string){ return Msg{Toggle{id}}; }),
                text(t.text) | (t.done ? (fg_muted | strike) : Mod{}) | grows,
                text("\xc3\x97") | fg_muted | pointer | tap(Remove{t.id}))
                | items_center | gap(10) | pad_y(8)
                | key("todo-" + std::to_string(t.id));   // keyed: reorders diff cleanly
        });

        return col(
            text("todos") | heading | mb(4),
            row(
                input(m.draft) | placeholder("What needs doing?") | grows
                    | on_input([](std::string v){ return Msg{Typed{v}}; })
                    | on_enter(Msg{Add{}}),
                button("Add", Add{}))
                | gap(8),
            col_(std::move(rows)) | mt(16),
            text(std::to_string(left) + " left") | fg_muted | mt(12))
            | max_w(520) | mx_auto | pad(32);
    }

    // SSR metadata — the crawler / social-share view of this route.
    static Meta meta(const Model& m) {
        return { .title = "todos (" + std::to_string(m.todos.size()) + ")",
                 .description = "A tiny waya todo app." };
    }
};

int main() { return live<App>({ .port = 8080, .title = "todos" }); }
```

That's a complete, production-shaped app: it renders on the server (crawlable
first paint), streams only diffs over a WebSocket, persists across reloads, and
its entire logic is a pure `update` you can unit-test without a browser.

## What each piece does

| Piece | Role |
|-------|------|
| **`Model`** | All the app's state, one plain struct. The only source of truth. |
| **`Msg`** | A `std::variant` of every event. Nothing changes state except through one. |
| **`init`** | The starting model (and an optional first `Cmd`). |
| **`update`** | Pure `(Model, Msg) -> (Model, Cmd)`. Your entire business logic. |
| **`view`** | Pure `Model -> NodeRef`. Rebuilt each frame; the diff ships only changes. |
| **`subscribe`** | Declares live inputs (timers, storage, routes, broadcasts) as data. |
| **`meta`** | Per-route `<head>` for SEO / social cards. |
| **`Cmd`** | A *description* of a side effect (here, `store`) the runtime performs. |

## Test it — no server needed

Because everything is pure, the whole app drives in a plain test:

```cpp
#include <waya/surface/test.hpp>

auto app = test::harness<App>();
app.fill("Buy milk").click("Add");        // by the rendered labels
assert(app.model().todos.size() == 1);
assert(app.text_contains("1 left"));
assert(app.last_cmd() != Cmd<Msg>::none()); // Add persisted to localStorage
```

## Where to go next

- **Add a feature** — a filter (all / active / done)? A `Filter` message + a
  `segmented(...)` control. Editing a todo inline? A `data_grid`. See
  [The Component Library](14-components.md) for what's built in.
- **Make it multiplayer** — `Cmd::broadcast` + `Sub::on_topic` sync every open
  tab. See [Effects](08-effects.md).
- **Make a reusable widget** — pull the todo row into its own widget with its own
  `Msg` and embed it with `map_msg` / `embed_update`. See
  [Reusable Components](20-components-reuse.md).
- **Ship it** — [Deployment](17-deployment.md): SSR, security headers, rate
  limits, `/healthz`, `/metrics`, Docker, reverse proxy.
