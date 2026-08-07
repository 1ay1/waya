# THE MATRIX — a hacker terminal in the digital rain

Falling green glyphs, a fake breach-in-progress terminal that types itself out,
and a live HUD (breach meter, proxy-node grid, telemetry). Rendered server-side
in C++ with waya; the whole animation is state advanced ~15×/sec and streamed
as DOM deltas.

```sh
waya run matrix
```

## Highly component-based

Each piece is its own module — the app root just composes them:

| File | Component |
|------|-----------|
| `theme.hpp`        | palette + mono-font + phosphor-glow / pane style helpers |
| `store.hpp` / `.cpp` | `Model` + `Msg` + the reducer (one tick drives rain, typing, breach) |
| `rain.hpp` / `.cpp`  | the falling-glyph SVG canvas (bright head → fading trail) |
| `terminal.hpp` / `.cpp` | the typed hacker log + a blinking block cursor |
| `hud.hpp` / `.cpp`   | breach meter, proxy-node grid, telemetry, control buttons |
| `app.cpp`          | composes rain (z0) + scanline fx (z2) + overlay (z3), the tick, main |

CMake globs `examples/matrix/*.cpp` into one `matrix` target.

## What it does

- **Digital rain** — 48 columns of glyphs falling at random speeds; each column
  is a bright leading glyph with a green trail that fades out. Columns respawn
  at the top when they fall off. Full field on the very first paint.
- **Hacker terminal** — types out a scripted breach sequence line by line with a
  blinking cursor; alerts and "ACCESS GRANTED" lines glow brighter.
- **HUD** — a breach meter that climbs (green → amber → red as it fills), a
  7-node proxy grid that lights up as nodes are "pwned", live telemetry, and a
  pulsing alert badge.
- **Controls** — `[ PAUSE ]` stops the tick, `[ ESCALATE ]` jumps the breach,
  `[ REBOOT ]` restarts the whole sequence.
- **CRT feel** — a full-screen scanline + flicker overlay and a vignette, layered
  above the rain but below the UI (z-index composition).

Everything is composed from waya primitives + a couple of bespoke gradients /
keyframes (the documented escape hatch). No client JS.
