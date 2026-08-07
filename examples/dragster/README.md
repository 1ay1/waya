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

Faithful to the **original Activision screen**: two horizontal lanes seen from
the side (you on top, a rival below), a purple sky over mint-green ground with
distance ticks rushing past, a black blocky dragster sliding down each lane, the
signature **green tachometer with a red redline**, big blocky **TIME / GEAR**
digits, and the ACTIVISION wordmark — cleaned up, no CRT noise. The whole browser
window is the screen.

- **`store.hpp` / `store.cpp`** — the model + the pure `update(Model, Msg)` game
  loop. Every `Tick` (a 30 fps *server* clock via `Sub::every`) advances RPM,
  checks the redline/blow, moves you and the opponent down the strip.
- **`screen.hpp` / `screen.cpp`** — the two-lane picture: each lane is a sky+ground
  band with scrolling ticks and a side-view **pixel-sprite** dragster (a
  `grid_cols`/`grid_rows` bitmap) that slides across by distance; plus the
  green/red tachometer. Built from waya's real style Mods.
- **`console.hpp`** — `hud_pill`, the control button (tap = the same Msg as a key).
- **`theme.hpp`** — the Dragster/TIA palette (purple, mint, black, tach green/red).
- **`app.cpp`** — the `Program`: `init` / `update` / `subscribe` / `view`, the big
  digit HUD + ACTIVISION footer, the phase overlays (staging tree, FINISH /
  BLOWN / FOUL), SSR `Meta`, and the hotkeys.

The server holds the game state; the browser is a thin client that paints binary
diffs pushed over a WebSocket. First paint is real SSR HTML — it renders (and is
crawlable) before any JS runs.
