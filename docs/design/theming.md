# Theming in waya

Colour your whole app by *role*, not by hex — and switch themes live, in one
paint, with a smooth animation. Nothing but a few tokens and one root mod.

## 1. Use tokens, never raw hex

Colour with semantic **token mods** so a node obeys whatever theme is active:

```cpp
card()  | bg_surface | border_token()     // panel colour + border
text(t) | fg_text                          // primary text
text(s) | fg_muted                         // secondary text
btn()   | bg_primary | fg_on_primary       // brand button
```

Available: `fg_text` / `fg_muted` / `fg_primary` / `fg_accent` / `fg_on_primary`,
`bg_surface` / `bg_raised` / `bg_primary` / `bg_accent` / `bg_page`,
`border_token()`.

## 2. Set the theme once, at the root

```cpp
return page(t.bg, content) | theme(t) | themed();
```

`theme(t)` declares every token as a CSS variable on the root; `themed()` paints
the page background + text from those tokens *and* adds a smooth colour
transition (so a live switch animates).

## 3. Presets — one line

```cpp
theme(Theme::dark())      // default: slate + indigo
theme(Theme::light())
theme(Theme::midnight())  // near-black + violet
theme(Theme::ocean())     // deep teal + cyan
theme(Theme::rose())      // warm light + rose
theme(Theme::dark().tint(0x22c55e))   // any preset, recoloured accent
```

Or define your own — `Theme` is a plain struct of a dozen `uint32_t` roles.

## 4. Live theme switching — free and optimal

Store the `Theme` in your Model, change it in `update`, and the whole app
re-tints instantly:

```cpp
struct Model { Theme theme = Theme::dark(); };
struct SetTheme { Theme t; };

static Model update(Model m, Msg msg) {
    std::visit(overload{ [&](SetTheme s){ m.theme = s.t; } }, msg);
    return m;
}
static NodeRef view(const Model& m) {
    return page(m.theme.bg, ui(m)) | theme(m.theme) | themed();
}
// a swatch:  box() | bg(preview) | tap(SetTheme{ Theme::light() })
```

**Why it's optimal:** the theme variables live on ONE node (the root). Because
every other node references `var(--wa-*)` — not a concrete colour — its subtree
hash doesn't change when the theme does. So a theme switch diffs to **exactly one
op** (a `set_paint` on the root), no matter how large the app. The browser
updates the root's variables, every descendant re-tints by CSS inheritance, and
the `.3s` transition animates it. A 10,000-node app re-themes with one op.

Add `theme_transition()` to any card/border you want to ease along with the page.

See `examples/themed.cpp` for a live palette picker (dark / light / midnight /
ocean / rose / custom tint), all coloured with tokens, switching smoothly.
Verified: the switch is a single-op diff and the output is W3C-valid.
