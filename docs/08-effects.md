# Effects & Subscriptions

A pure `update` can't perform I/O — but it can return a **description** of I/O
for the runtime to perform. Those descriptions are values: **`Cmd<Msg>`** for
one-shot effects, **`Sub<Msg>`** for standing ones. Both are ordinary data —
comparable, batchable, testable — so your app stays pure.

Include them via `<waya/surface/live.hpp>` (or `<waya/surface/effect.hpp>`).

## Returning a `Cmd` from `update`

Switch `update` to the effectful overload — it returns `(Model, Cmd<Msg>)`:

```cpp
static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
    return std::visit(overload{
        [&](Load) -> std::pair<Model,Cmd<Msg>> {
            return { m.loading(), Cmd<Msg>::fetch("/data.json", [](std::string body){
                return Loaded{ body };
            }) };
        },
        [&](const Loaded& l) -> std::pair<Model,Cmd<Msg>> {
            return { m.with_data(l.body), Cmd<Msg>::none() };
        },
    }, msg);
}
```

When you have nothing to do, return `Cmd<Msg>::none()`.

## `Cmd<Msg>` — one-shot effects

| Factory | What it does |
|---|---|
| `Cmd<Msg>::none()` | Do nothing. |
| `Cmd<Msg>::quit()` | Stop this session. |
| `Cmd<Msg>::emit(msg)` | Immediately feed `msg` back into `update` (chaining). |
| `Cmd<Msg>::after(ms, msg)` | Deliver `msg` after a delay (one-shot timer). |
| `Cmd<Msg>::task(fn)` | Run `fn` on a worker thread; its returned `Msg` is delivered. |
| `Cmd<Msg>::fetch(url, on_done)` | HTTP GET `url`; call `on_done(body)` → `Msg`. |
| `Cmd<Msg>::navigate(url, replace=false)` | Change route (pushes history, re-routes). |
| `Cmd<Msg>::push_url(url)` | Update the address bar only (deep-link sync, no route). |
| `Cmd<Msg>::broadcast(topic, payload)` | Publish to every session on `topic`. |
| `Cmd<Msg>::set_title(text)` | Set the live browser tab title (unread counts). |
| `Cmd<Msg>::scroll_to(target, smooth=true)` | Scroll to an `anchor("…")` node, or `"top"`/`"bottom"`. |
| `Cmd<Msg>::focus(target)` / `Cmd<Msg>::blur()` | Move keyboard focus to a named control / release it. |
| `Cmd<Msg>::copy(text)` | Put `text` on the user's clipboard. |
| `Cmd<Msg>::download(name, data, mime)` | Offer `data` as a browser file download. |
| `Cmd<Msg>::batch(cmds…)` | Run several commands (variadic or a `vector`). |

### `after` — a one-shot timer

```cpp
[&](Toast){ return { m.show_toast(), Cmd<Msg>::after(2500, HideToast{}) }; }
```

### `task` — background work

```cpp
[&](Compute){ return { m, Cmd<Msg>::task([input = m.n]{
    return Computed{ expensive(input) };   // runs off the loop; result delivered
}) }; }
```

`task` runs on a detached worker thread and delivers exactly one `Msg` when it
finishes. The model loop never blocks.

### Browser effects — title, scroll, focus, clipboard, download

The browser is waya's display, so a handful of display-side actions are Cmds
too — fired from `update`, executed by the fixed client, no JS written:

```cpp
// chat: append a message, then keep the log pinned to the end
[&](Sent s){ m.log.push_back(s.text);
             return { m, Cmd<Msg>::scroll_to("bottom") }; }

// open a dialog and put the cursor in its first field
[&](Compose){ m.dialog = true;
              return { m, Cmd<Msg>::focus("subject") }; }   // input(…) | name("subject")

// "copy link" + unread count in the tab title
[&](CopyLink){ return { m, Cmd<Msg>::copy(m.share_url) }; }
[&](Arrived){  m.unread++;
               return { m, Cmd<Msg>::set_title("(" + std::to_string(m.unread) + ") Inbox") }; }

// export straight from the Model — no endpoint needed
[&](Export){ return { m, Cmd<Msg>::download("report.csv", to_csv(m.rows), "text/csv") }; }
```

`scroll_to`/`focus` name a node via `anchor("id")` (or a control's `name("…")`),
and they run **after** the same update's repaint has applied — so scrolling to a
row you just inserted works. Like every Cmd they are plain data: equality-
comparable in tests, carried through `map` unchanged.

### File uploads — `file_input` + `on_file`

Uploads are the same shape in reverse — the picked file arrives in `update` as
decoded bytes, no multipart endpoint:

```cpp
file_input(on_file([](FileData f){ return Import{ f.name, f.content }; }),
           accept(".csv,.json"), multiple())
```

`FileData` carries `name`, `mime`, and `content` (the raw bytes, already
decoded). The client caps a file at 8 MB — beyond that, serve a dedicated
endpoint.


### `fetch` / `post` / `http` — real async HTTP

```cpp
// GET
[&](Load){ return { m.loading(), Cmd<Msg>::fetch("https://api.dev/items",
    [](std::string body){ return Loaded{ body }; }) }; }

// POST JSON
[&](Save){ return { m, Cmd<Msg>::post("https://api.dev/save", payload,
    [](std::string body){ return Saved{ body }; }) }; }

// Any method + headers (auth, content-type, etc.)
[&](Pay){ return { m, Cmd<Msg>::http("POST", "https://api.dev/pay",
    {{"Authorization", "Bearer " + m.token}, {"Content-Type", "application/json"}},
    body, [](std::string r){ return Paid{ r }; }) }; }
```

The client runs on a bounded worker pool (never the model loop) and does real
DNS, arbitrary methods, request headers + body, and a timeout. The mapper gets
the response body (empty on error, so your handler always fires).

#### Need the status? Use the `_full` variants

`fetch`/`post`/`http` hand you the body only — which means a `404`, a `500`, or a
timeout arrives as an empty string you can't tell apart from a legitimate empty
`200`. When that distinction matters, use `fetch_full` / `post_full` /
`http_full`: your callback receives the whole `Response` — `status`, `headers`,
`body`, `ok()`, and a case-insensitive `header(name)`:

```cpp
[&](Load){ return { m.loading(), Cmd<Msg>::fetch_full("https://api.dev/items",
    [](Cmd<Msg>::Response r) -> Msg {
        if (r.status == 0)   return LoadFailed{ "network error" };   // never completed
        if (!r.ok())         return LoadFailed{ "HTTP " + std::to_string(r.status) };
        return Loaded{ r.body };
    }) }; }
```

`status == 0` means the request never completed (DNS/connect/timeout, or an
`https://` URL without `-DWAYA_TLS`). No outcome is ever silently dropped.

**HTTPS** requires the runtime built with `-DWAYA_TLS=ON` (links OpenSSL);
without it, `https://` URLs fail cleanly (empty body) rather than talking
plaintext.

### Parsing JSON

Real APIs speak JSON — `<waya/json.hpp>` reads and writes it with no dependency:

```cpp
#include <waya/json.hpp>
using namespace waya;

[&](const Loaded& l) -> std::pair<Model,Cmd<Msg>> {
    auto j = json::parse(l.body);
    Model n = m;
    n.user = j["user"]["name"].str();
    n.credits = j["user"]["credits"].as_int();
    for (auto& item : j["items"].arr())
        n.items.push_back({ item["id"].as_int(), item["title"].str() });
    return { n, Cmd<Msg>::none() };
}

// build a request body
std::string body = json::object({
    {"amount", json::number(m.total)},
    {"currency", json::string("usd")},
}).dump();
```

Accessors are safe: a missing key or wrong type returns a default
(`""`/`0`/`false`), so a malformed response never crashes `update`.

### `emit` — chain messages

```cpp
[&](SaveAndClose){ return { m.save(), Cmd<Msg>::batch(
    Cmd<Msg>::emit(Saved{}),
    Cmd<Msg>::navigate("/list")
) }; }
```

### `batch` — several at once

```cpp
Cmd<Msg>::batch(
    Cmd<Msg>::after(1000, Tick{}),
    Cmd<Msg>::broadcast("room", "joined")
)
// or: Cmd<Msg>::batch(std::vector<Cmd<Msg>>{ ... })
```

## `Sub<Msg>` — standing effects

`subscribe(const Model&) -> Sub<Msg>` declares effects that run **as long as the
model says so**. The runtime reconciles them each frame — starting new ones,
stopping ones no longer present.

```cpp
static Sub<Msg> subscribe(const Model& m) {
    return m.running ? Sub<Msg>::every(1000, Tick{}) : Sub<Msg>::none();
}
```

| Factory | What it does |
|---|---|
| `Sub<Msg>::none()` | No subscriptions. |
| `Sub<Msg>::every(ms, msg)` | Deliver `msg` on a repeating interval (a clock). |
| `Sub<Msg>::on_route(fn)` | On route change, call `fn(path)` → `Msg`. |
| `Sub<Msg>::on_topic(topic, fn)` | On a broadcast to `topic`, call `fn(payload)` → `Msg`. |
| `Sub<Msg>::batch(subs…)` | Combine several subscriptions. |

### `every` — a repeating timer

```cpp
static Sub<Msg> subscribe(const Model& m) {
    return Sub<Msg>::batch(
        Sub<Msg>::every(1000, Tick{}),          // 1s clock
        m.polling ? Sub<Msg>::every(5000, Poll{}) : Sub<Msg>::none()
    );
}
```

Because subscriptions are a function of the model, you *turn effects on and off
by changing state*. Set `m.running = false` and the clock stops — no timer
handle to manage.

## Broadcast — multiplayer

Sessions are isolated by default (each has its own model). To make them
influence each other — live chat, presence, a shared counter — publish and
subscribe on a **topic**:

```cpp
// publish from update:
[&](Send){ return { m, Cmd<Msg>::broadcast("room-42", m.draft) }; }

// receive in subscribe:
static Sub<Msg> subscribe(const Model& m) {
    return Sub<Msg>::on_topic("room-42", [](std::string payload){
        return Received{ payload };
    });
}
```

A `broadcast` fans out to **every** session subscribed to that topic —
**including the sender**. Design for that: either don't apply the change
locally and let the broadcast round-trip be the single source of truth, or make
the local application idempotent.

!!! warning "The sender receives its own broadcast"
    If `Send` both appends a message locally *and* broadcasts it, and the
    session also subscribes to that topic, the sender will see the message
    twice. Pick one source of truth.

## Routing effects

- `Cmd<Msg>::navigate("/post/42")` — go to a route (updates history + re-routes).
- `Sub<Msg>::on_route(fn)` — react to route changes (parse the path into state).

See [Routing & SEO](09-routing-seo.md) for the full routing story.

## Testing effects

Because `Cmd` and `Sub` are comparable values, you can assert on them:

```cpp
auto [m2, cmd] = App::update(m, App::Load{});
assert(m2.loading);
assert(cmd == Cmd<App::Msg>::fetch("/data.json", /*…*/));  // effect described, not run
```

No network, no timers, no runtime — the effect is *data* until the runtime
interprets it.

---

Next: [Routing & SEO](09-routing-seo.md) — multi-page apps that server-render
every route and rank well.
