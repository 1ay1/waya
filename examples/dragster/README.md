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

Styled as a **handheld LCD game** (Game & Watch / Mattel era): an olive-green LCD
panel where everything — the car, the scrolling road, the tachometer — is drawn as
a grid of lit / faint-"ghost" segments. The whole browser window *is* the screen.

- **`store.hpp` / `store.cpp`** — the model + the pure `update(Model, Msg)` game
  loop. Every `Tick` (a 30 fps *server* clock via `Sub::every`) advances RPM,
  checks the redline/blow, computes speed and distance, and detects the finish.
- **`screen.hpp` / `screen.cpp`** — the LCD: a `grid_cols`/`grid_rows` matrix of
  segments. The dragster is a fixed pixel sprite; lane dashes and the road scroll
  toward you (keyed to `m.pos`) to sell speed. The tachometer is drawn in the
  same segment language, so it reads as one device. Built from waya's real style
  Mods — `raw_css` only for the LCD sheen (a gradient overlay).
- **`console.hpp`** — `hud_pill`, the glassy control button (tap = the same Msg as
  the key).
- **`theme.hpp`** — the LCD palette (olive panel, near-black lit segments, faint
  ghost segments) + the sheen.
- **`app.cpp`** — the `Program`: `init` / `update` / `subscribe` (the clock only
  ticks while a countdown or race is live) / `view`, the phase overlays (staging
  tree, FINISH / BLOWN / FOUL), the floating HUD, SSR `Meta`, and the hotkeys.

The server holds the game state; the browser is a thin client that paints binary
diffs pushed over a WebSocket. First paint is real SSR HTML — it renders (and is
crawlable) before any JS runs.
