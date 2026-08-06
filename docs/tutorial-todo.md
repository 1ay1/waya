# Tutorial: Build a To-Do App

This is a hands-on, zero-to-finished walkthrough. By the end you'll have built a
complete to-do application — add items, check them off, filter, delete — and
you'll understand *every line*. No prior web-framework experience is assumed. If
you can write a `for` loop in C++, you can follow this.

We'll build it in small steps, running the app after each one so you always see
progress.

!!! tip "How to read this"
    Type the code yourself as you go — it sticks better than copy-paste. After
    each step, rebuild and refresh the browser to see the change.

---

## What we're building

A classic to-do list:

- a text box to type a new task,
- a list of tasks, each with a checkbox and a delete button,
- filter buttons: **All / Active / Done**,
- a footer showing how many are left.

Everything runs on the server; the browser just shows the result and sends
clicks back. You'll never write HTML, CSS, or JavaScript.

---

## Step 0: Project setup

Create a folder with two files.

**`CMakeLists.txt`:**

```cmake
cmake_minimum_required(VERSION 3.28)
project(todo LANGUAGES CXX)

add_subdirectory(third_party/waya)   # put waya here (git clone or FetchContent)

add_executable(todo main.cpp)
target_link_libraries(todo PRIVATE waya::waya)
target_compile_features(todo PRIVATE cxx_std_26)
```

**`main.cpp`** (the smallest possible waya app — we'll grow it):

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Todo {
    struct Model {};
    using Msg = std::variant<std::monostate>;

    static Model init() { return {}; }
    static Model update(Model m, Msg) { return m; }

    static NodeRef view(const Model&) {
        return text("To-Do") | font(32) | pad(40);
    }
};

int main() { return live<Todo>({ .port = 8080, .title = "To-Do" }); }
```

Build and run:

```bash
cmake -S . -B build && cmake --build build
./build/todo
```

Open <http://localhost:8080>. You should see the heading "To-Do". Leave the
server running in one terminal; rebuild in another as you make changes (or use
`scripts/dev.sh` for auto-reload).

!!! note "What just happened"
    `live<Todo>(...)` started a web server. It called your `view` function,
    turned the returned node into HTML, and sent it to the browser. That's the
    whole model — you'll just make `view` (and the state behind it) richer.

---

## Step 1: Model the data

A to-do app is a *list of tasks*. Let's define what a task is and hold a list of
them in the model. In waya, **the `Model` is a plain struct that holds all your
state** — nothing special.

```cpp
#include <waya/surface/live.hpp>
#include <string>
#include <vector>
using namespace waya::surface;

// One task.
struct Task {
    int id;               // a unique number, so we can refer to it later
    std::string text;     // what the task says
    bool done = false;    // checked off or not
};

struct Todo {
    struct Model {
        std::vector<Task> tasks;   // all our tasks
        int next_id = 1;           // the id to give the next new task
    };

    // …
};
```

Give the app a couple of starter tasks so there's something to see. Replace
`init`:

```cpp
static Model init() {
    return {
        .tasks = {
            { 1, "Learn waya", false },
            { 2, "Build a to-do app", false },
        },
        .next_id = 3,
    };
}
```

!!! info "Designated initializers"
    `{ .tasks = {...}, .next_id = 3 }` sets struct fields by name. It's a normal
    C++ feature; waya uses it a lot because it reads clearly.

---

## Step 2: Show the list

Now make `view` render the tasks. We build a **column** (`col`) with a heading
and one row per task.

```cpp
static NodeRef view(const Model& m) {
    // Build a node for each task.
    std::vector<NodeRef> rows;
    for (const auto& t : m.tasks) {
        rows.push_back(
            text(t.text) | font(18) | pad_y(8)
        );
    }

    return col(
        text("To-Do") | font(32) | weight(Weight::bold),
        col_(rows) | gap(4)            // col_ takes a vector of nodes
    ) | gap(20) | pad(40) | max_w(px(480));
}
```

Rebuild, refresh. You now see both starter tasks listed.

!!! note "`col` vs `col_`"
    `col(a, b, c)` takes children as **arguments**. `col_(vec)` takes a
    **vector** of children — use it when you build the children in a loop, like
    here. Same for `row`/`row_` and `box`/`box_`.

Let's understand `view` piece by piece:

- `text(t.text)` — a text node showing the task.
- `| font(18) | pad_y(8)` — modifiers: font size 18, 8px of vertical padding.
- `col_(rows) | gap(4)` — stack the rows vertically with 4px between them.
- The outer `col(...) | gap(20) | pad(40) | max_w(px(480))` — the page: 20px
  between the heading and the list, 40px padding around everything, capped at
  480px wide.

Every `|` adds one modifier. That's *all* of styling in waya.

---

## Step 3: Make it interactive — messages

So far the app is static. To *change* state, waya uses **messages**. A message
is a small struct describing "something happened." You list every possible
message in a `std::variant` called `Msg`.

Let's add the ability to toggle a task done/undone. We need a message that
carries *which* task:

```cpp
struct Todo {
    struct Model { /* … */ };

    // ── Messages: everything that can happen ──
    struct Toggle { int id; };            // check/uncheck task `id`
    using Msg = std::variant<Toggle>;

    // …
};
```

Now handle that message in `update`. **`update` takes the current state and a
message and returns the new state.** It's a pure function — no surprises, no I/O.

```cpp
static Model update(Model m, Msg msg) {
    std::visit(overload{
        [&](const Toggle& t) {
            for (auto& task : m.tasks)
                if (task.id == t.id)
                    task.done = !task.done;   // flip it
        },
    }, msg);
    return m;
}
```

!!! info "`std::visit` + `overload`"
    `Msg` is a variant (it can be *one of* several message types). `std::visit`
    with `overload{...}` runs the matching lambda for whichever message arrived.
    Right now there's only `Toggle`, but you add one lambda per message type as
    you grow. `overload` is a helper waya provides.

Finally, wire it up in `view`: render a checkbox and tell it to send `Toggle`
when changed.

```cpp
for (const auto& t : m.tasks) {
    rows.push_back(
        row(
            checkbox(t.done)
                | on_change([id = t.id](std::string){ return Toggle{ id }; }),
            text(t.text) | font(18)
                | when_(t.done, strike | fg(0x94a3b8))    // dim + strike if done
        ) | gap(10) | center | pad_y(6)
    );
}
```

Rebuild, refresh, click a checkbox — the task toggles, and done tasks get a
strike-through. **You just made a reactive app.**

What happened on that click:

1. The browser told the server "the checkbox for task 1 changed."
2. waya ran `update(model, Toggle{1})`, flipping `done`.
3. waya ran `view(new_model)`, producing an updated surface.
4. waya compared it to the previous surface and sent back only the changed bits.
5. The browser updated. No page reload, no flicker.

!!! note "`when_(condition, mods…)`"
    `when_(t.done, strike | fg(0x94a3b8))` applies those mods **only if** the
    task is done. It's how you style conditionally.

---

## Step 4: Add new tasks

We need a text box and an "add" action. First, the state for what the user is
typing, and two new messages:

```cpp
struct Model {
    std::vector<Task> tasks;
    int next_id = 1;
    std::string draft;        // what's currently typed in the box
};

struct Toggle { int id; };
struct SetDraft { std::string text; };   // the box's text changed
struct Add {};                           // the user pressed Enter / clicked Add
using Msg = std::variant<Toggle, SetDraft, Add>;
```

Handle them in `update` (add two lambdas):

```cpp
static Model update(Model m, Msg msg) {
    std::visit(overload{
        [&](const Toggle& t) {
            for (auto& task : m.tasks)
                if (task.id == t.id) task.done = !task.done;
        },
        [&](const SetDraft& s) {
            m.draft = s.text;              // remember what's typed
        },
        [&](const Add&) {
            if (m.draft.empty()) return;   // ignore empty
            m.tasks.push_back({ m.next_id++, m.draft, false });
            m.draft.clear();               // reset the box
        },
    }, msg);
    return m;
}
```

Add the input box at the top of `view`, above the list:

```cpp
auto box_input = input(m.draft)
    | placeholder("What needs doing?")
    | on_input([](std::string v){ return SetDraft{ v }; })   // every keystroke
    | on_enter(Add{})                                        // Enter = add
    | pad_x(14) | pad_y(11) | round(10)
    | bg(0x1e293b) | fg(0xe2e8f0) | w(pct(100));

return col(
    text("To-Do") | font(32) | weight(Weight::bold),
    box_input,
    col_(rows) | gap(4)
) | gap(20) | pad(40) | max_w(px(480));
```

Rebuild, refresh. Type a task, press Enter — it appears in the list and the box
clears.

Two things to notice:

- **`input(m.draft)`** — the box's contents come *from the model*. The model is
  the single source of truth; the box just reflects it. This is why clearing
  `m.draft` in `Add` empties the box.
- **`on_input(...)`** fires on every keystroke with the current text; we store it
  in `draft`. **`on_enter(Add{})`** fires when Enter is pressed.

---

## Step 5: Delete tasks

Add a delete message:

```cpp
struct Remove { int id; };
using Msg = std::variant<Toggle, SetDraft, Add, Remove>;
```

Handle it (erase the matching task):

```cpp
[&](const Remove& r) {
    std::erase_if(m.tasks, [&](const Task& t){ return t.id == r.id; });
},
```

Add a delete button to each row. Update the row:

```cpp
row(
    checkbox(t.done)
        | on_change([id = t.id](std::string){ return Toggle{ id }; }),
    text(t.text) | font(18) | grow(1)
        | when_(t.done, strike | fg(0x94a3b8)),
    text("✕") | fg(0xf87171) | pad(6) | pointer
        | tap(Remove{ t.id })                    // click ✕ to delete
) | gap(10) | center | pad_y(6);
```

Rebuild, refresh, delete a task with the ✕. Done.

- `grow(1)` on the text makes it take the leftover width, pushing the ✕ to the
  right edge.
- `tap(Remove{ t.id })` sends the delete message when the ✕ is clicked;
  `pointer` shows the hand cursor.

---

## Step 6: Filters

Let's add **All / Active / Done** filters. Track the current filter in the model:

```cpp
enum class Filter { All, Active, Done };

struct Model {
    std::vector<Task> tasks;
    int next_id = 1;
    std::string draft;
    Filter filter = Filter::All;
};

struct SetFilter { Filter f; };
using Msg = std::variant<Toggle, SetDraft, Add, Remove, SetFilter>;
```

Handle it:

```cpp
[&](const SetFilter& f) { m.filter = f.f; },
```

Filter the rows when building them:

```cpp
for (const auto& t : m.tasks) {
    bool show = m.filter == Filter::All
             || (m.filter == Filter::Active && !t.done)
             || (m.filter == Filter::Done   &&  t.done);
    if (!show) continue;
    rows.push_back(/* the row from before */);
}
```

Add a filter bar. A small helper keeps it tidy:

```cpp
static NodeRef filter_btn(std::string label, Filter f, Filter active) {
    bool on = f == active;
    return text(label)
        | pad_x(12) | pad_y(6) | round(8)
        | fg(on ? 0xffffff : 0x94a3b8)
        | when_(on, bg(0x6366f1))
        | pointer | tap(SetFilter{ f });
}
```

Add the bar to `view` (below the list):

```cpp
row(
    filter_btn("All",    Filter::All,    m.filter),
    filter_btn("Active", Filter::Active, m.filter),
    filter_btn("Done",   Filter::Done,   m.filter)
) | gap(8)
```

Rebuild, refresh — the filters work, and the active one is highlighted.

---

## Step 7: A footer count

Show how many tasks are left. Compute it in `view` and add a footer line:

```cpp
int left = 0;
for (const auto& t : m.tasks) if (!t.done) ++left;

// …in the returned col, at the bottom:
text(std::to_string(left) + " left") | fg(0x94a3b8) | font(14)
```

---

## The finished app

Here it is in full — a complete to-do app in one file, ~80 lines, no HTML/CSS/JS:

```cpp
#include <waya/surface/live.hpp>
#include <string>
#include <vector>
using namespace waya::surface;

struct Task { int id; std::string text; bool done = false; };
enum class Filter { All, Active, Done };

struct Todo {
    struct Model {
        std::vector<Task> tasks;
        int next_id = 1;
        std::string draft;
        Filter filter = Filter::All;
    };

    struct Toggle { int id; };
    struct SetDraft { std::string text; };
    struct Add {};
    struct Remove { int id; };
    struct SetFilter { Filter f; };
    using Msg = std::variant<Toggle, SetDraft, Add, Remove, SetFilter>;

    static Model init() {
        return { .tasks = { {1,"Learn waya",false}, {2,"Build a to-do app",false} },
                 .next_id = 3 };
    }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](const Toggle& t){ for (auto& x : m.tasks) if (x.id==t.id) x.done=!x.done; },
            [&](const SetDraft& s){ m.draft = s.text; },
            [&](const Add&){ if(!m.draft.empty()){ m.tasks.push_back({m.next_id++,m.draft,false}); m.draft.clear(); } },
            [&](const Remove& r){ std::erase_if(m.tasks, [&](const Task& t){ return t.id==r.id; }); },
            [&](const SetFilter& f){ m.filter = f.f; },
        }, msg);
        return m;
    }

    static NodeRef filter_btn(std::string label, Filter f, Filter active) {
        bool on = f == active;
        return text(label) | pad_x(12) | pad_y(6) | round(8)
            | fg(on ? 0xffffff : 0x94a3b8) | when_(on, bg(0x6366f1))
            | pointer | tap(SetFilter{ f });
    }

    static NodeRef view(const Model& m) {
        std::vector<NodeRef> rows;
        int left = 0;
        for (const auto& t : m.tasks) {
            if (!t.done) ++left;
            bool show = m.filter == Filter::All
                     || (m.filter == Filter::Active && !t.done)
                     || (m.filter == Filter::Done   &&  t.done);
            if (!show) continue;
            rows.push_back(
                row(
                    checkbox(t.done) | on_change([id=t.id](std::string){ return Toggle{id}; }),
                    text(t.text) | font(18) | grow(1) | when_(t.done, strike | fg(0x94a3b8)),
                    text("✕") | fg(0xf87171) | pad(6) | pointer | tap(Remove{ t.id })
                ) | gap(10) | center | pad_y(6)
            );
        }

        return col(
            text("To-Do") | font(32) | weight(Weight::bold),
            input(m.draft) | placeholder("What needs doing?")
                | on_input([](std::string v){ return SetDraft{v}; }) | on_enter(Add{})
                | pad_x(14) | pad_y(11) | round(10) | bg(0x1e293b) | fg(0xe2e8f0) | w(pct(100)),
            col_(rows) | gap(4),
            row(
                filter_btn("All", Filter::All, m.filter),
                filter_btn("Active", Filter::Active, m.filter),
                filter_btn("Done", Filter::Done, m.filter),
                push(),
                text(std::to_string(left) + " left") | fg(0x94a3b8) | font(14)
            ) | gap(8) | center
        ) | gap(20) | pad(40) | max_w(px(480)) | fg(0xe2e8f0);
    }
};

int main() { return live<Todo>({ .port = 8080, .page_bg = 0x0b1020, .title = "To-Do" }); }
```

## What you learned

You now know the whole shape of a waya app:

- **`Model`** — a struct with all your state.
- **`Msg`** — a variant of small structs, one per thing that can happen.
- **`init`** — the starting state.
- **`update`** — pure `(Model, Msg) → Model`, matched with `std::visit`.
- **`view`** — pure `(Model) → NodeRef`, built from primitives + `|` mods.
- **Events** — `tap`, `on_change`, `on_input`, `on_enter` turn interactions into
  messages.
- **`col`/`row`/`col_`**, `gap`, `pad`, `fg`, `bg`, `round`, `when_`, `grow`,
  `push`, `checkbox`, `input`, `text` — the everyday vocabulary.

Every waya app, no matter how big, is this same loop. From here:

- **[The Mental Model](02-mental-model.md)** — the ideas behind what you just did.
- **[Styling](04-styling.md)** and **[Layout](05-layout.md)** — make it beautiful.
- **[Effects & Subscriptions](08-effects.md)** — timers, async data, multiplayer.
- **[The API Reference](11-api-reference.md)** — every tool available.
