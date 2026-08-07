# Events & Inputs

Interactions are wired with **event mods** — the same `|`-applied values as
styling. There are two shapes:

- **Message mods** (`tap(Msg{})`) send a fixed message.
- **Mapper mods** (`on_input([](std::string v){ return SetQ{v}; })`) receive the
  event's value (the typed text, the chosen option) and build a message from it.

waya registers each wired message and maps it to an opaque wire token — your app
stays fully type-safe; you never write an integer id.

## Taps

```cpp
template <typename Msg> Mod tap(Msg m);
```

Fire a message when the node is clicked/tapped:

```cpp
text("Delete") | pointer | tap(Delete{ id })
button("Save") | tap(Save{})
card(...) | pointer | tap(Open{ item.id })
```

Pair `tap` with `pointer` (cursor) and usually `interactive()` (hover/press
feedback) for a tappable non-button. For genuine buttons, use the `button`
primitive.

## Text input

```cpp
// fires on EVERY keystroke, with the field's current value
template <typename Fn> Mod on_input(Fn fn);       // Fn: std::string -> Msg
template <typename E>  Mod on_input(E msg);        // fixed msg, ignores value

// fires when the control's value is committed (blur / change)
template <typename Fn> Mod on_change(Fn fn);       // Fn: std::string -> Msg
```

```cpp
input(m.query)
    | placeholder("Search…")
    | on_input([](std::string v){ return SetQuery{ v }; });
```

The `on_input` mapper receives the live value; return a message carrying it.
Because the field's displayed value comes from `input(m.query)` (your model),
the input is a pure function of state — drive it from the model and it stays in
sync.

### Enter / Escape / arbitrary keys

```cpp
template <typename Msg> Mod on_enter(Msg m);      // Enter key
template <typename Msg> Mod on_escape(Msg m);     // Escape key
template <typename Msg> Mod on_key(std::string key, Msg m);  // any key by name
template <typename Fn>  Mod on_keydown(Fn fn);    // Fn: std::string(key) -> Msg
```

```cpp
input(m.draft)
    | on_input([](std::string v){ return SetDraft{v}; })
    | on_enter(Submit{})
    | on_key("ArrowUp",   HistoryPrev{})
    | on_key("ArrowDown", HistoryNext{});
```

`on_key` matches the browser's `KeyboardEvent.key` value (`"Enter"`,
`"Escape"`, `"ArrowUp"`, `"a"`, …). `on_keydown` gives you the key string and
lets you decide the message.

## Checkboxes, radios, selects

```cpp
checkbox(m.agreed)
    | on_change([](std::string v){ return SetAgreed{ v == "true" }; });

radio("plan", "pro", m.plan == "pro")
    | on_change([](std::string v){ return SetPlan{ v }; });   // v == "pro"

select({ option("us","US"), option("de","Germany") }, m.country)
    | on_change([](std::string v){ return SetCountry{ v }; });
```

`on_change` fires with:

- `"true"` / `"false"` for a checkbox,
- the option's `value` for a radio or select.

## Control attributes

```cpp
Mod placeholder(std::string p);   // input placeholder
Mod type(std::string t);          // input type: "password", "email", "number"…
Mod name(std::string n);          // form field name (for on_submit gathering)
Mod checked(bool = true);         // set a checkbox/radio checked
Mod disabled(bool = true);        // disable a control
```

```cpp
input() | type("password") | name("pw") | placeholder("Password")
button("Send") | disabled(m.sending)
```

### Validation & constraints

Every native input constraint is a first-class mod (`<waya/surface/forms.hpp>`,
included by the umbrella) — they drive the browser's own validation and `:invalid`
styling:

```cpp
Mod required(bool = true);         // must be filled to submit
Mod readonly(bool = true);         // shown, not editable
Mod min_val(v); Mod max_val(v);    // numeric/date bounds
Mod step_by(v); Mod step_any();    // numeric granularity
Mod pattern(std::string re);       // regex the value must match
Mod maxlength(int); Mod minlength(int);
Mod title_hint(std::string);       // message shown on validation failure
```

```cpp
email_input(m.email) | required() | autocomplete("email")
number_input(m.qty)  | min_val(1.0) | max_val(99.0) | step_by(1.0)
input(m.code)        | pattern("[0-9]{6}") | maxlength(6) | title_hint("6 digits")
```

### Mobile & assistive behaviour

```cpp
Mod inputmode(std::string);        // the phone keyboard: "numeric"/"decimal"/"email"/…
Mod enterkey(std::string);         // the mobile Enter-key label: "send"/"go"/"search"
Mod autocomplete(std::string);     // autofill hint: "email"/"current-password"/…
Mod spellcheck(bool);
Mod autocapitalize(std::string);   // "none"/"sentences"/"words"/"characters"
```

```cpp
tel_input(m.phone) | inputmode("tel") | autocomplete("tel")
search_input(m.q)  | inputmode("search") | enterkey("search")
input(m.user)      | spellcheck(false) | autocapitalize("none")
```

### Sizing, multi-value & association

```cpp
Mod rows(int); Mod cols(int);      // textarea dimensions
Mod size_attr(int);                // visible char width
Mod allow_multiple(bool = true);   // select / file: multiple values
Mod accepts(std::string);          // file accept filter ("image/*")
Mod capture(std::string);          // mobile camera: "user"/"environment"
Mod id(std::string);               // element id (pairs with label_for)
Mod default_value(std::string);    // uncontrolled initial value
Mod form_id(std::string);          // associate with a <form> by id (outside it)
```

## Forms

A `form(...)` groups named controls; `on_submit` gathers them into a single
query-string value when submitted (Enter in a field, or a button inside):

```cpp
template <typename Fn> Mod on_submit(Fn fn);   // Fn: std::string(body) -> Msg
```

```cpp
form(
    input() | name("email") | type("email") | placeholder("Email"),
    input() | name("pw")    | type("password") | placeholder("Password"),
    button("Sign in")
) | on_submit([](std::string body){ return SignIn{ body }; });
// body == "email=ada@x.com&pw=secret"
```

Parse `body` in your `update` however you like (split on `&` and `=`).

### Form fields the easy way

Wiring a labelled input + skin + `on_input` by hand for every field gets
repetitive. The component library's **form patterns** (`waya/ui.hpp`) do it in
one call each — a real `<label>`, the shared input chrome, and the value mapper,
all wired for you:

```cpp
using namespace waya::ui;

card(
  section("Account",
    email_field("Email", m.email, [](auto v){ return SetEmail{v}; }, "you@x.com"),
    password_field("Password", m.pw, [](auto v){ return SetPw{v}; }),
    select_field("Plan", { option("Free","free"), option("Pro","pro") }, m.plan,
                          [](auto v){ return SetPlan{v}; }),
    switch_field("Notifications", "email + push", m.notify, ToggleNotify{}),
    checkbox_field("I agree", m.agree, ToggleAgree{})),
  form_actions(button("Cancel", Cancel{}, Variant::secondary),
               button("Save", Save{})))
```

See [Components → Forms](14-components.md#forms) for the full list
(`text_field`, `textarea_field`, and the typed aliases).

## Focus & pointer events

```cpp
template <typename Msg> Mod on_focus(Msg m);
template <typename Msg> Mod on_blur(Msg m);
template <typename Msg> Mod on_enter_pointer(Msg m);   // pointer enters
template <typename Msg> Mod on_leave_pointer(Msg m);   // pointer leaves
template <typename Msg> Mod on_hover(Msg enter, Msg leave);  // both, in one mod
```

```cpp
card | on_hover(Preview{ id }, PreviewOff{})
input(...) | on_focus(Focused{}) | on_blur(Blurred{})
```

!!! tip "Visual hover doesn't need a message"
    To *style* on hover (change colour, lift), use `on(Hover, …)` from
    [Styling](04-styling.md#states-hover-focus-active-disabled) — it's pure CSS,
    no round-trip. Use `on_hover`/`on_enter_pointer` only when hovering must
    change your **model** (e.g. a live preview).

## Double-click

```cpp
template <typename Msg> Mod on_double(Msg m);   // dblclick
```

```cpp
photo | on_double(Like{ id })     // Instagram's heart-on-photo
```

## Drag & drop

```cpp
Mod draggable(std::string payload = {});          // make a node draggable
template <typename Fn> Mod on_drop(Fn fn);        // Fn: std::string -> Msg
Mod drop_arg(std::string id);                     // tag a target with its id
template <typename Fn> Mod drop_target(std::string id, Fn fn);  // both, in one mod
```

The dragged node's `payload` (its `name`) is delivered to the drop target's
mapper as `"<payload>:<id>"`, where `<id>` is the target's `drop_arg`. Use
`drop_target(id, fn)` to declare a drop zone and its id in one call:

```cpp
// draggable card carrying its id
card(item) | draggable(std::to_string(item.id))

// a column that accepts drops, tagged with which column it is
column | drop_target("todo", [](std::string s){
    // s == "<dragged-payload>:<id>", e.g. "42:todo"
    auto colon = s.rfind(':');
    int card_id = std::atoi(s.substr(0, colon).c_str());
    std::string col = s.substr(colon + 1);
    return MoveCard{ card_id, col };
});
```

That's a full Kanban column: drag a card onto it, and `update` gets both **what**
was dropped and **where**. (See the `nova` example for a complete drag-and-drop
issue tracker.)

## The general event mod

Any DOM event → a message (the escape hatch):

```cpp
template <typename Msg> Mod on(std::string event, Msg m, std::string arg = {});
template <typename Fn>  Mod on_ev(std::string event, Fn fn, std::string arg = {});
```

- `on("pointerdown", Grab{})` — fixed message on any event.
- `on_ev("scroll", [](std::string v){ return Scrolled{v}; })` — value-carrying.
- The optional `arg` filters keyboard events by key (that's how `on_key` is
  built) or tags a target for drops.

Prefer the named mods above; drop to `on`/`on_ev` only for events without a
dedicated helper.

## Stopping propagation

```cpp
Mod stop();   // clicks inside this node don't bubble to an outer tap()
```

Put `stop()` on modal content so clicking it doesn't trigger the backdrop's
close-`tap`:

```cpp
overlay(
    panel_content | stop(),          // clicks here stay here
    ...
) | tap(CloseModal{});               // clicking the backdrop closes
```

---

Next: [Effects & Subscriptions](08-effects.md) — timers, async fetch,
navigation, and multiplayer broadcast, all as pure data.
