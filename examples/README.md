# waya/place — a live collaborative pixel canvas

A single-file [r/place](https://en.wikipedia.org/wiki/R/place)-style demo:
everyone paints on **one shared 32×32 grid**, and every stroke syncs to all
connected tabs in real time.

```sh
waya run pixels          # or: WAYA_PORT=8080 ./build/pixels
```

Open **two browser tabs** side by side and paint — each stroke appears in the
other instantly.

## What it shows off

- **Real-time multiplayer** — one `Cmd::broadcast("canvas", …)` on paint fans
  out to every session; each one's `Sub::on_topic("canvas", …)` pulls the new
  board. No sockets, no pub/sub library, no glue.
- **Server-side shared state** — the board is a mutex-guarded array on the
  server, so a brand-new tab sees the existing art on its **first paint** (real
  SSR HTML, crawlable before any JS runs).
- **O(changed) rendering** — the 1024 cells are `key`ed, so painting one pixel
  ships a **single DOM op** over the wire, not the whole grid.
- **No client code** — the entire app is a pure `update` + `view` in ~120 lines
  of C++. There's no JavaScript, no CSS file, and no HTML in the source.

## The whole idea in three messages

```cpp
[&](Paint p) {                         // click a cell
    board().px[p.cell] = m.color;      // write the shared board (server memory)
    return { m, Cmd::broadcast("canvas","paint") };  // tell everyone
},
[&](Synced) {                          // a broadcast arrived (from anyone)
    m.board_ = snapshot();             // re-read the shared board
    return { m, Cmd::none() };
},
```

`subscribe` wires the broadcast to `Synced`:

```cpp
Sub<Msg>::on_topic("canvas", [](std::string){ return Synced{}; });
```

Because a broadcast reaches **every** session including the sender, the shared
board is the single source of truth — everyone (painter included) just re-reads
it, so there's no double-application to reason about.

## Files

- `pixels.cpp` — the whole app: the shared `Board`, the `Model`/`Msg`,
  `update`/`view`/`subscribe`/`meta`, and `main`.
