# Testing

A waya app is pure — `init`/`update`/`view` are functions with no sockets, no
DOM, no globals — so you can test the *entire* thing in a plain unit test. No
browser, no server, no mocking. `<waya/surface/test.hpp>` makes it a one-liner.

## The harness

```cpp
#include <waya/surface/test.hpp>
using namespace waya::surface;

auto app = test::harness<Counter>();   // runs init(), holds the Model

app.send(Counter::Inc{});              // dispatch a Msg through the real update
app.send(Counter::Inc{});
assert(app.model().n == 2);            // inspect the resulting Model

assert(app.text_contains("count: 2")); // render view() and query the tree
assert(app.count(Kind::button) == 2);  // structural queries
```

`harness<P>()` constructs the app (calling its `init()`), then `send()` drives
the **real** `update` — all four update shapes work (value, effects, both, or
neither), so what you test is exactly what runs in production.

### Sending messages

```cpp
app.send(Msg);                 // a Msg with no input value
app.send(Msg, "hello");        // a Msg carrying an input value (as a form field would)
app.send_all({ A{}, B{}, C{} });   // a whole scenario, in order
app.send(A{}).send(B{});       // fluent chaining
```

### Inspecting state

```cpp
app.model();                   // const ref to the current Model
app.view();                    // NodeRef — the rendered tree for the current model
app.text();                    // all visible text, space-joined
app.text_contains("saved");    // substring test over the text
app.count(Kind::button);       // how many nodes of a kind
app.find_key("row-7");         // the node carrying a given key (or nullptr)
```

## Asserting effects — without running them

`Cmd` is value-comparable, and the harness records the `Cmd` every `update`
returns. So you assert on *what effect was requested* without performing any
I/O:

```cpp
app.send(Counter::Save{});
assert(app.last_cmd() == Cmd<Msg>::after(300, Counter::Saved{}));
```

This is the payoff of the effects-as-data design: an HTTP call, a timer, a
broadcast — all are plain values you can compare in a test.

## Asserting the view is well-formed

The harness folds in the [structural validator](16-safety.md):

```cpp
assert(app.valid());              // rendered view passes every WHATWG + waya rule
assert(app.validate().empty());   // or read the violation report as a string
```

Combined with a strict build (`-DWAYA_STRICT=ON`), this means a malformed UI
fails your test suite *before* it can ever fail a user.

## A complete example

```cpp
#include <waya/surface/test.hpp>
#include <cassert>
using namespace waya::surface;

int main() {
    auto app = test::harness<TodoApp>();

    app.send(TodoApp::Add{}, "Buy milk");
    app.send(TodoApp::Add{}, "Walk dog");
    assert(app.model().items.size() == 2);
    assert(app.text_contains("Buy milk"));

    app.send(TodoApp::Toggle{ 0 });
    assert(app.model().items[0].done);

    app.send(TodoApp::ClearDone{});
    assert(app.model().items.size() == 1);
    assert(app.valid());
}
```

No framework runner, no fixtures — it's just a `main()` you can wire into CTest,
Catch2, GoogleTest, or run directly. Because the app is pure, the test is fast,
deterministic, and covers the real logic your users hit.
