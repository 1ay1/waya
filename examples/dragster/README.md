# Dragster

A playable homage to **Activision's _Dragster_ (1980)** — the David Crane drag
racer that was one of Activision's very first Atari 2600 titles — rendered
entirely **server-side in C++** with waya.

```sh
cmake --build build --target dragster
./build/dragster            # http://localhost:8081
```

## The game

_Dragster_ isn't about steering — it's about the **tachometer**. The whole race
is over in a few seconds, and the skill is all in the throttle and the shifter:

1. **Stage** and wait out the christmas-tree countdown. Shift *before* the green
   and you draw a **red-light foul**.
2. On the green, feather the **throttle** to build RPM — but keep it **under the
   redline**. Sit past the redline too long and the engine **blows**.
3. **Shift up** through 4 gears at the right moment to keep the revs in the power
   band. Cross the strip in the **fewest seconds** — best time wins.

## Controls

| Key | Button | Action |
|-----|--------|--------|
| `Space` | **THROTTLE** | toggle the throttle on/off |
| `↑` / `Shift` / `W` | **SHIFT ▲** | shift up one gear |
| `Enter` / `R` | **START** | stage / restart |

The on-screen buttons dispatch the same messages as the keys, so it plays with
touch on a phone too.

## How it's built (the waya-idiomatic parts)

- **`store.hpp` / `store.cpp`** — the model + the pure `update(Model, Msg)` game
  loop. Every `Tick` (a 30 fps *server* clock via `Sub::every`) advances RPM,
  checks the redline/blow, computes speed and distance, and detects the finish.
  No logic lives in the view.
- **`screen.hpp` / `screen.cpp`** — the CRT picture: a side-view strip (2600-blue
  sky over a green track), scrolling distance stripes, the finish gantry drifting
  in, and the red dragster sliding across as it covers the quarter-mile — plus the
  **tachometer** bar (the instrument you actually play on). Built from waya
  layout/`aspect` primitives.
- **`console.hpp` / `console.cpp`** — the woodgrain console: timing readouts and
  the tactile buttons.
- **`theme.hpp`** — the flat TIA palette, blocky mono type, CRT scanlines.
- **`app.cpp`** — the `Program`: `init` / `update` / `subscribe` (the game clock
  only ticks while a countdown or race is live) / `view`, the phase overlays
  (staging tree, FINISH / BLOWN / FOUL), SSR `Meta`, and the global hotkeys.

The server holds the game state; the browser is a thin client that paints binary
diffs pushed over a WebSocket. First paint is real SSR HTML — it renders (and is
crawlable) before any JS runs.
