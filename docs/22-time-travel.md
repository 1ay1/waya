# Time-Travel Debugging

Because a waya app is **pure** — `update` has no hidden state and effects are
plain data — its entire history is just the list of messages. Replay them from
`init()` and you land in the exact same state, every time. That makes real
time-travel debugging almost free: `<waya/surface/timetravel.hpp>` records every
step and lets you scrub the timeline like a video.

## Record and scrub

```cpp
#include <waya/surface/timetravel.hpp>
using namespace waya::surface;

auto tl = debug::timeline<Counter>();

tl.send(Counter::Inc{});
tl.send(Counter::Inc{});
tl.send(Counter::Dec{});
assert(tl.model().n == 1);

tl.back();          // step to before the Dec  → n == 2
tl.back();          // → n == 1
tl.jump(0);         // all the way to init()    → n == 0
tl.forward();       // replay the next message  → n == 1
tl.latest();        // jump to the newest state → n == 1
```

Every jump **replays the pure `update` from `init()`** up to the cursor, so a
scrubbed state can never desync from what really happened — no stale snapshots,
no heisenbugs.

## Branching history

Stepping back and then sending a *new* message truncates the future and starts a
fresh branch, exactly like an editor's undo history:

```cpp
tl.jump(1);                    // rewind to after the first message
tl.send(Counter::SetTo{}, "42");   // the old "future" is dropped; a new branch begins
```

## See what each message changed on screen

The timeline can diff any two frames — giving you the exact surface patch the
runtime would have streamed to the browser:

```cpp
tl.send(Counter::Inc{});
Patch p = tl.last_patch();          // what this message changed in the DOM
Patch d = tl.diff_between(0, 3);    // cumulative change from init to step 3
```

## Preview any frame without moving the cursor

```cpp
Counter::Model m = tl.model_at(2);   // the model two steps in
NodeRef ui       = tl.view_at(2);    // its rendered view
```

## Find the exact frame a bug appeared

"Somewhere in this 200-message session the UI broke" becomes a single index.
`first_invalid()` replays the whole history and returns the first step whose
rendered view fails [structural validation](16-safety.md):

```cpp
std::size_t bad = tl.first_invalid();
if (bad <= tl.step_count())
    std::cerr << "UI first became invalid at step " << bad << "\n";
```

## Export a reproducible trace

`export_trace()` prints a human-readable script of the whole session — labels,
input values, and a marker on any frame whose view is invalid. Paste it into a
bug report; anyone with the app can replay the same messages and reproduce it:

```cpp
tl.send(Counter::Inc{}, "", "increment");
tl.send(Counter::Break{}, "", "trigger the bug");
std::cout << tl.export_trace();
// waya timeline (2 steps)
//   #0  init()
//   #1  increment
//   #2  trigger the bug   [INVALID VIEW]
// cursor at #2
```

## In tests

The timeline is a superset of the [test harness](21-testing.md) — anything you
assert with `harness<P>()` you can also assert at any point in history. A common
pattern is a regression test that captures a real user session and pins every
intermediate state:

```cpp
auto tl = debug::timeline<App>();
tl.send_all({ App::Login{}, App::OpenCart{}, App::Checkout{} });
assert(tl.first_invalid() > tl.step_count());   // no frame ever broke
assert(tl.model().stage == Stage::Done);
```

It's all pure, header-only, and dependency-free — no runtime, no browser, no
recording infrastructure to stand up. The message log *is* the recording.
