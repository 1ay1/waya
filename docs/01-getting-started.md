# Getting Started

This chapter takes you from nothing to a running waya app, then explains every
line of it.

## Requirements

- A **C++26** compiler. GCC 15+ is the reference toolchain; recent Clang works
  too. (waya uses `std::variant`, concepts, and other modern features.)
- **CMake 3.28+**.
- A POSIX system for the runtime (Linux/macOS). The library is header-only.

waya is **header-only** — there is nothing to compile into a `.a`/`.so`. You
just add the include directory and link the system threads library.

## The `waya` command

The repo ships a single launcher — like `npm` or `cargo` — so you never have
to remember the underlying cmake invocations. It runs on Linux, macOS, the BSDs,
and Windows (Git-Bash / WSL, or `waya.cmd` from cmd/PowerShell):

```sh
./waya new my-app     # scaffold a fresh app
./waya dev            # watch, rebuild & live-reload in the browser
./waya run            # build then serve once  (alias: serve)
./waya build          # build a target without running it
./waya list           # list example targets
./waya clean          # remove the build directory
./waya doctor         # check your toolchain
```

Every command is spelled out in the **[CLI Reference](23-cli.md)**.

Put it on your PATH (`ln -s "$PWD/waya" ~/.local/bin/waya`) and drop the `./`.
Every command takes an optional target and flags like `--port` / `--no-open`:

```sh
waya dev palette
waya run aurora --port 9000 --no-open
```

## Install

The fastest start is the scaffolder — it generates a complete, building app:

```sh
./waya new my-app && cd my-app
waya run          # builds, then serves on http://localhost:8080
```

(Under the hood `waya new` calls `scripts/create-waya-app.sh`; you can run that
directly too.)

To add waya to an existing project, pick one of the following.

### As a subdirectory (simplest)

Clone waya into your project (e.g. under `third_party/`) and add it:

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

add_subdirectory(third_party/waya)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE waya::waya)
target_compile_features(my_app PRIVATE cxx_std_26)
```

The `waya::waya` target already carries the include path, the C++26 feature
requirement, and `Threads::Threads`, so linking it is all you need.

### With FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(waya
    GIT_REPOSITORY https://github.com/1ay1/waya.git
    GIT_TAG        master)
FetchContent_MakeAvailable(waya)

target_link_libraries(my_app PRIVATE waya::waya)
```

### Installed system-wide

From the waya checkout:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

Then in a consumer:

```cmake
find_package(waya REQUIRED)
target_link_libraries(my_app PRIVATE waya::waya)
```

## Your first app

Create `main.cpp`:

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Hello {
    struct Model {};
    using Msg = std::variant<std::monostate>;

    static Model init() { return {}; }
    static Model update(Model m, Msg) { return m; }

    static NodeRef view(const Model&) {
        return col(
            text("Hello, waya") | font(40) | weight(Weight::black),
            text("A surface, rendered from C++.") | fg(0x94a3b8)
        ) | gap(12) | pad(48) | center;
    }
};

int main() {
    return live<Hello>({ .port = 8080, .title = "Hello" });
}
```

Build and run:

```bash
cmake -S . -B build && cmake --build build
./build/my_app
```

Open <http://localhost:8080>. You should see the heading. The server auto-opens
your browser unless you set `WAYA_NO_OPEN=1`.

!!! tip "Hot reload while you build"
    Run `scripts/dev.sh <target>` instead of building by hand. It watches the
    source tree, rebuilds on every save, and restarts the server; your browser
    reconnects and reloads itself a moment later — no manual rebuild, no manual
    refresh. (The client detects a new build id over the same WebSocket and
    hard-reloads only when the server was actually rebuilt.)

## A real, interactive app

The `Hello` app is static. Here is a counter — the smallest app with state and
events. Every waya concept you need at the start is in these 30 lines.

```cpp
#include <waya/surface/live.hpp>
using namespace waya::surface;

struct Counter {
    // 1. The state.
    struct Model { int n = 0; };

    // 2. The messages — what can happen.
    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    // 3. The initial state.
    static Model init() { return {}; }

    // 4. How each message changes the state (pure — no side effects).
    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Inc){   m.n++; },
            [&](Dec){   m.n--; },
            [&](Reset){ m.n = 0; },
        }, msg);
        return m;
    }

    // 5. How the state looks (pure — a function of the model only).
    static NodeRef view(const Model& m) {
        return col(
            text(m.n) | font(64) | weight(Weight::black)
                       | fg(m.n < 0 ? 0xf87171 : 0xe2e8f0),
            row(
                btn("−", Dec{}, 0x1e293b),
                btn("reset", Reset{}, 0x334155),
                btn("+", Inc{}, 0x6366f1)
            ) | gap(10)
        ) | gap(24) | pad(48) | center;
    }

    // A tiny local "component": just a function that returns a node.
    static NodeRef btn(std::string label, Msg m, std::uint32_t bg_) {
        return text(label)
            | pad_x(18) | pad_y(12) | round(12) | bg(bg_) | fg(0xffffff)
            | pointer | tap(m);
    }
};

int main() { return live<Counter>({ .port = 8080, .title = "Counter" }); }
```

### Walking through it

- **`Model`** is a plain struct — your entire application state. Here it's a
  single `int`.
- **`Msg`** is a `std::variant` of small structs, one per thing that can
  happen. This is the *only* way state changes.
- **`init`** returns the starting state.
- **`update`** takes the current state and a message, and returns the new
  state. It is **pure**: no I/O, no globals — just `(Model, Msg) → Model`.
  `overload{...}` + `std::visit` is the idiomatic way to switch on the variant;
  it's provided by waya.
- **`view`** takes the state and returns a **surface** — a `NodeRef` tree. It
  too is pure: the same model always produces the same surface. Notice there's
  no HTML, no CSS, no `onclick` — `tap(Inc{})` says "when tapped, send the
  `Inc` message," and waya wires the rest.
- **`btn`** shows the whole component story: a component is just a function
  returning a node. No base class, no macro, no registration.

When you click `+`, the browser sends a tiny token over the socket. The
runtime calls `update(model, Inc{})`, calls `view(new_model)`, diffs the new
surface against the old one, and streams back only the changed text node. The
number updates; nothing else re-renders.

!!! tip "Batteries, when you want them"
    You just built a button from primitives — that's the point: the core lets
    you build anything. When you'd rather not, `#include <waya/ui.hpp>` gives
    you a ready-made component library (`button`, `card`, `field`, `dialog`,
    `tabs`, `badge`, theme presets…), all built on the same core. See
    [The Component Library](14-components.md).

## The `main` entry point

```cpp
int main() {
    return live<MyApp>({ .port = 8080, .title = "My App" });
}
```

`live<P>(config)` starts the server and blocks until Ctrl-C. The config
(`LiveConfig`) fields:

| Field | Type | Default | Meaning |
|---|---|---|---|
| `port` | `int` | `8080` | Listen port (also overridable by `WAYA_PORT`). |
| `host` | `const char*` | `"0.0.0.0"` | Bind address; default = all interfaces (LAN-reachable). |
| `open` | `bool` | `true` | Auto-open the browser on start. |
| `page_bg` | `std::uint32_t` | `0x0b1020` | Page background colour behind the app. |
| `title` | `const char*` | `"waya"` | Default `<title>`. |

Environment overrides: `WAYA_PORT`, `WAYA_HOST`, and `WAYA_NO_OPEN`.

## Add `static_assert` for good errors

Put this above `main` so a mistake in your `Program` type surfaces as one clear
line instead of a template error:

```cpp
static_assert(SurfaceProgram<Counter>);
```

## Where to next

- [The Mental Model](02-mental-model.md) — the whole idea in one page.
- [The Vocabulary](03-vocabulary.md) — every primitive.
- [Styling](04-styling.md) — make it beautiful.
- [Events & Inputs](07-events.md) — forms, keys, drag & drop.
