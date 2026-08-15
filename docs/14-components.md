# The Component Library (`waya::ui`)

waya ships in two layers, and the split is the whole point.

- **The core** (`waya/surface/*`) is a complete, *unopinionated* substrate. It
  gives you primitives (`box`, `text`, `image`, `path`, inputs), a uniform mod
  vocabulary applied with `|`, layout, effects, the Elm runtime, and universal
  escape hatches (`css`, `attr`, `var`, `as`). It never picks a look for you.
  With it you can build **any** UI.

- **The component library** (`waya/ui.hpp`) is a batteries-included set of
  ready-made components — buttons, cards, dialogs, tabs, badges, and more —
  built **entirely on top of the public core**. No private API, no runtime
  hooks. Every component is a plain function you can read, copy, and own.

That mirrors maya: the core gives you a complete low-level vocabulary and dic­
tates nothing; the library is a good default so you don't rebuild the common
90% from scratch. If a component isn't quite what you want, open it — it's a
15-line function over the same `col`/`row`/`text` and `|` mods you already use.

## Using it

```cpp
#include <waya/surface/live.hpp>   // the framework
#include <waya/ui.hpp>             // the components

using namespace waya::surface;
using namespace waya::ui;

static NodeRef view(const Model& m) {
    return card(
        row(avatar("AB"), text("Ada Lovelace") | heading, push(),
            badge("pro", Tone::success), dot()),
        divider(),
        field("Email", input(m.email) | input_skin() | on_input(Edit{}),
              m.error),
        row(button("Save", Save{}),
            button("Cancel", Cancel{}, Variant::ghost))
          | gap(10)
    ) | theme(midnight());
}
```

Everything a component paints reads **theme tokens** (`var(--wa-*)`), so it
recolours automatically when you swap the theme at the root — and each carries
a sensible fallback so it looks right with no theme at all.

## The roster

### Layout & structure

- `card(children…)` — the ubiquitous panel: themed surface, border, padding,
  radius, soft elevation.
- `divider(bool vertical=false)` — a hairline rule.
- `link(label)` — an inline link look (primary colour, underline on hover).
  Pair with `tap(msg)`.

### Buttons

`Variant` picks emphasis: `primary` (filled brand), `secondary` (raised
surface), `ghost` (text-only until hover), `danger` (destructive).

- `button(label, msg, variant = primary)` — a themed button wired to a tap.
- `button_node(child, msg, variant)` — button chrome around any node (icon+text).
- `icon_button(glyph, msg, variant = ghost)` — a compact square button.

### Forms

- `field(label, control, hint = "")` — a labelled control with an optional
  helper/error line. Wrap any core input.
- `input_skin()` — a mod that applies the library's input chrome (themed,
  focus-ringed) to a raw `input()`/`textarea()`/`select()`, so all your fields
  match.

### Status & identity

`Tone` = `neutral | primary | success | warning | danger`.

- `badge(label, tone = neutral)` — a small pill.
- `dot(tone = success)` — a tiny status dot.
- `avatar(initials, d = 36)` / `avatar_img(url, d = 36)` — a circular avatar.

### Loading

- `spinner(d = 22, stroke = 0)` — a rotating ring. Registers its own keyframe
  through the [asset registry](15-assets.md) — works anywhere, no setup.
- `skeleton(w, h)` — a shimmering placeholder block for loading content.

### Async data — `RemoteData<T>`

Every screen that loads over the network has the same four states: not-asked,
loading, failed, loaded. Hand-rolled that's a fragile ladder of bools that can
reach illegal combinations (loading *and* failed?). `RemoteData<T>` makes it one
sum type — you store ONE in your model, so illegal states are unrepresentable:

```cpp
struct Model { RemoteData<std::vector<User>> users; };

// update: transition it with loading() / loaded() / failed()
[&](Load)     { return { {loading(m.users)}, fetchUsers() }; }
[&](Got r)    { m.users = r.ok() ? loaded(parse(r.body)) : failed<Users>(r.status_text(), m.users); }

// view: ONE line covers spinner + error-card-with-Retry + content
remote(m.users, [](const Users& us){ return user_list(us); }, Retry{})
```

Or take all four branches yourself with the exhaustive form
`remote(rd, onLoading, onSuccess, onFailure)`. Two niceties are free:
`loading(previous)` and `failed(err, previous)` **retain the last-loaded value**,
so a re-poll shows stale-then-fresh instead of flashing a spinner over content
the user was reading (stale-while-revalidate). `rd.value()` reads the retained
value in any state; `rd.error()` the message.

### Navigation

- `tabs(active, {{id, "Label"}…}, to_msg)` — a themed tab bar. `to_msg` maps a
  tab id to a `Msg`; the active tab is underlined in the primary colour.

### Floating layers

Built on the core `overlay` / `anchored` primitives.

- `popover(open, trigger, panel, place = "bottom-right")` — an anchored dropdown
  with frosted-panel chrome and auto show/hide.
- `tooltip(trigger, text, place = "top")` — a hover tooltip. Uses a registered
  group-hover CSS rule, so it needs **no state** in your Model.
- `dialog(open, close_msg, panel_children…)` — a complete modal: dimmed backdrop
  that closes on click, a stopped panel so content clicks don't close it, plus
  frosted chrome and a pop-in entrance.
- `toast(message, tone = neutral)` and `toast_layer(nodes)` — a fixed,
  non-interactive top-right stack for toasts. For a real notification SYSTEM
  (auto-dismiss, stacking, close buttons), use the `Toasts` queue below.

### Notifications — the `Toasts` queue

`toast()` draws one card. A real notification system is a QUEUE: messages
arrive, stack, auto-dismiss after a timeout, and can be closed early. `Toasts`
makes that queue one value in your model — no per-item timer bookkeeping:

```cpp
struct Model { Toasts notes; };

// update:
[&](Saved)     { m.notes.success("Saved!");   return {m, Cmd::none()}; }
[&](Failed e)  { m.notes.error(e.why);        return {m, Cmd::none()}; }
[&](Close c)   { m.notes.dismiss(c.id);       return {m, Cmd::none()}; }
[&](Tick)      { m.notes.tick(100ms);         return {m, Cmd::none()}; }

// subscribe: run the clock ONLY while a toast is counting down
static Sub<Msg> subscribe(const Model& m){
    return m.notes.ticking() ? Sub<Msg>::every(100, Tick{}) : Sub<Msg>::none();
}

// view: one call renders the fixed, keyed, aria-live top-right stack
overlay(main_ui, toasts_layer(m.notes, [](int id){ return Close{id}; }))
```

`push(msg, tone, ttl)` returns a stable id; `success`/`error`/`info` are sugar.
A `ttl` of `0ms` makes a toast sticky (dismiss-only). Every card is keyed by id
so the diff moves/removes them precisely, carries an aria-labelled close button,
and the layer is an `aria-live` region so screen readers announce new messages.

### Optimistic updates — `Optimistic<T>`

For an instant-feeling UI you apply a change LOCALLY before the server confirms
it, then roll back if the request fails. `Optimistic<T>` is that value — it holds
the committed truth and an in-flight guess, and `value()` returns the right one:

```cpp
struct Model { Optimistic<bool> liked{false}; };

[&](Like)     { m.liked.apply(!m.liked.value());        // show it NOW
                return {m, postLike(m.liked.value())}; }
[&](LikeOk)   { m.liked.confirm();  return {m, Cmd::none()}; }   // it stuck
[&](LikeFail) { m.liked.rollback(); return {m, Cmd::none()}; }   // undo it

// view: value() is the optimistic guess while pending, else the committed truth
heart | (m.liked.value() ? filled : outline)
      | (m.liked.pending() ? opacity(.6f) : Mod{})
```

`confirm(authoritative)` takes a server-corrected value (a normalised string, a
server-assigned id); `settled()` reads past any in-flight guess.

### Stateful widgets

These take their state as a plain argument and emit wired messages, so the state
stays in your `Model` and the component stays a pure function — no hidden state.

- `toggle(on, msg)` — an iOS-style switch; sends `msg` on change.
- `progress(pct, tone = primary)` — a determinate bar, 0..100.
- `slider(value, min, max, msg, step = 1)` — a themed range; sends the new value
  as a string on input.
- `menu(open, trigger, items…)` + `menu_item(label, msg, icon = "")` — a dropdown
  whose open state lives in your Model.
- `accordion(open_id, {{title, body}…}, on_toggle)` — collapsible sections;
  `on_toggle(i)` maps a header click to a Msg.
- `data_table(rows, columns)` — a typed, aligned CSS-grid table. Each `Column`
  maps a row to a cell node, so it's fully generic:

```cpp
data_table<User>(users, {
    { "Name",  [](const User& u){ return text(u.name); } },
    { "Role",  [](const User& u){ return badge(u.role); } },
    { "",      [](const User& u){ return icon_button_i("edit", Edit{ u.id }); } },
});
```

For an INTERACTIVE table — click a header to sort, type to filter, page through
thousands of rows — hold a `TableState` in your model and describe columns with
`col<Row>`. Sort/filter/page are all model state; the derivation is pure.

```cpp
struct Model { std::vector<User> users; TableState table; };  // table.page_size = 25

std::vector<TableColumn<User>> columns(){
    return {
        col<User>("Name",  [](const User& u){ return text(u.name); })
            .sortable([](const User& a, const User& b){ return a.name < b.name; })
            .searchable([](const User& u){ return u.name; }),
        col<User>("Score", [](const User& u){ return text(std::to_string(u.score)); })
            .sortable([](const User& a, const User& b){ return a.score < b.score; }),
    };
}

// update: m.table.sort_by(col), .set_filter(q), .go_page(n)
// view:
data_table(m.users, columns(), m.table, SortBy{}, GoPage{})
```

A header is clickable only if its column is `.sortable()` (first click ascending,
second descending); filtering searches every `.searchable()` column
(case-insensitive); a pager renders when `page_size > 0`. `table_order(rows,
cols, state)` is the pure filter→sort→page pipeline — test it on plain data.

### Drag to reorder — `reorderable`

Drag & drop primitives (`draggable`/`on_drop`) exist; `reorderable` turns them
into a list you can reorder, computing the move for you.

```cpp
struct Model { std::vector<Task> tasks; };
struct Dropped { int from, to; };

[&](Dropped d){ apply_reorder(m.tasks, d.from, d.to); return {m, Cmd::none()}; }

// view: wrap each item; dragging A onto B fires onDrop(A_index, B_index)
for (int i = 0; i < (int)m.tasks.size(); ++i)
    rows.push_back(reorder_row(i,
        [](int from, int to){ return Dropped{from, to}; },
        task_card(m.tasks[i])));
```

`apply_reorder(vec, from, to)` is a pure vector move (out-of-range/same-index =
no-op) you can unit-test; a malformed drop payload safely resolves to indices
that no-op.

### Icons

`icon("name", size = 24)` returns an inline SVG that tints with `fg(…)` (it uses
`currentColor`) and scales with `size`. Only the icons you name end up in your
binary. `icon_button_i("trash", Delete{})` puts one in a themed button.

Available: `check` `x` `plus` `minus` `search` `menu` `chevron-{down,up,left,
right}` `arrow-{left,right}` `user` `settings` `trash` `edit` `heart` `star`
`bell` `home` `mail` `external` `info` `alert` `loader`.

### Charts

Data viz built on the `path` primitive — each is a node you colour and size:

- `line_chart(values, w, h)` / `sparkline(values, w, h)` — a polyline; tint with
  `stroke(hex, width)`.
- `area_chart(values, w, h)` — a filled line closed to the baseline.
- `bars(values, w, h)` — a bar chart; colour with `fg(hex)`.

```cpp
sparkline(cpu_history) | stroke(0x22d3ee, 2) | w(120) | h(32)
bars({4, 9, 2, 7}) | fg(0x8b5cf6)
```

### Keyboard shortcuts — `Keymap`

`on_shortcut("mod+k", Open{})` wires one shortcut on one node — but scatter them
across the view and there's no single source of truth, so you can't render a
help overlay or spot collisions. `Keymap<Msg>` makes the shortcut set one value:
declare bindings once, arm them all with `wire()`, and generate a help sheet
from the same data.

```cpp
static Keymap<Msg> keys(){
    return Keymap<Msg>{}
        .bind("mod+k", "Command palette", OpenPalette{})
        .bind("?",     "Toggle help",     ToggleHelp{})
        .bind("g h",   "Go home",         Nav{"/"},   "Navigation")
        .bind("g p",   "Go to profile",   Nav{"/me"}, "Navigation");
}

// view: one mod arms every binding; one call renders the grouped cheat-sheet
app_shell | wire(keys())
m.help_open ? modal(shortcut_help(keys()), CloseHelp{}) : nothing()
```

Because the help view is generated from the keymap, it can never drift from the
behaviour — add a binding and it appears in the sheet automatically. `"+"` splits
a chord (`mod+k`), a space splits a sequence (`g h`), each rendered as `kbd` caps.

### Command palette — Cmd+K, on the keymap

The same `Keymap` powers a fuzzy launcher. `command_palette` reads it, filters
by a subsequence match on the label ("usr" finds "Go to **us**e**r** settings"),
and renders a searchable list — so the palette and the keyboard always list the
same commands.

```cpp
struct Model { bool open=false; std::string q; int sel=0; };

[&](OpenPalette){ m.open=true; m.q=""; m.sel=0; return {m, Cmd::focus("cmdk")}; }
[&](Query e)    { m.q=e.value; m.sel=0;            return {m, Cmd::none()}; }
[&](Run c)      { m.open=false; /* re-dispatch c.msg */ return {m, Cmd::none()}; }

// view:
m.open ? command_palette(keys(), m.q, m.sel,
             Query{}, [](Msg cmd){ return Run{cmd}; }, ClosePalette{})
       : nothing()
```

You own the little bit of state (query + selected index); the palette is pure.
`palette_matches(keymap, query)` is the ranked list on its own if you want a
custom shell.

### Undo / redo — `History<T>`

Wrap any (sub)model value in `History<T>` and undo/redo come for free:

```cpp
struct Model { History<Doc> doc; };

[&](Edit e){ m.doc.push(edited(m.doc.get(), e)); return {m, Cmd::none()}; }
[&](Undo)  { m.doc.undo();  return {m, Cmd::none()}; }
[&](Redo)  { m.doc.redo();  return {m, Cmd::none()}; }

// view: read the present, gate the buttons
editor(m.doc.get());
button("Undo", Undo{}) | when_(!m.doc.can_undo(), disabled());
```

`push` records the current present onto the past and clears the redo branch
(editing after undo forks the timeline); pushing an unchanged value is a no-op;
a `limit` caps the past so a long session stays bounded. It's a plain value with
`==`, so an undoable model stays testable and time-travels cleanly.

### Virtualized lists — `virtual_list`

waya rebuilds the whole tree every frame, and the diff makes that cheap — but
building 100,000 rows still costs 100,000 allocations per frame when the user
sees ~20. `virtual_list` builds only the visible window plus a small overscan,
with spacers keeping the scrollbar honest:

```cpp
struct Model { int scroll_top = 0; std::vector<Row> rows; };

// update: the scroll container reports its scrollTop as the event value
[&](Scrolled s){ m.scroll_top = std::atoi(s.value.c_str()); return {m, Cmd::none()}; }

// view: a fixed-height scroll box; virtual_list windows the rows
scroll_window(360, Scrolled{},                      // 360px tall, reports scroll
    virtual_list(m.scroll_top, 360, /*row_h=*/48, (int)m.rows.size(),
        [&](int i){ return row_view(m.rows[i]); }))
```

A million-row table costs ~30 built nodes per frame, not a million. Rows must be
`row_h` tall for the math to line up (the helper sets it). The windowing math
(`virtual_range`) is pure and unit-testable; every row is keyed by index so the
diff reuses DOM as you scroll, and the client throttles scroll reporting to one
frame.

### Infinite scroll — `Paged<T>`

Pairs an `on_appear` sentinel with paginated fetching. `Paged<T>` accumulates
pages, tracks whether a fetch is in flight (so a fast scroll can't stampede the
server), and knows when the list is exhausted.

```cpp
struct Model { Paged<Post> feed; };

[&](LoadMore){ if (!m.feed.can_load()) return {m, Cmd::none()};
               m.feed.begin_load();
               return {m, fetchPage(m.feed.next_page())}; }
[&](Loaded r){ m.feed.append(parse(r.body), has_more(r)); return {m, Cmd::none()}; }

// view: items, then the sentinel that loads the next page
col(col_(map(m.feed.items(), post_card)),
    infinite_sentinel(m.feed, LoadMore{}))
```

`infinite_sentinel` renders an `on_appear` trigger while more pages exist, a
spinner while loading, and nothing once exhausted — so the list simply ends.
`can_load()` gates your fetch; `append(items, has_more)` records both.

### Multi-step flows — `Wizard`

A checkout, an onboarding, a multi-page form. `Wizard` is a step cursor
(`next`/`back`/`go`, `is_first`/`is_last`/`is_complete`) you gate in your own
update; `wizard_steps` renders the numbered progress header.

```cpp
struct Model { Wizard flow{3}; Form<> details; };

[&](Next){ if (step_valid(m)) m.flow.next(); return {m, Cmd::none()}; }  // gate the advance
[&](Back){ m.flow.back();                    return {m, Cmd::none()}; }

// view:
col(wizard_steps(m.flow, {"Account","Details","Review"}),
    step_body(m.flow.current()),
    row(button("Back", Back{}) | when_(m.flow.is_first(), disabled()),
        button(m.flow.is_last() ? "Finish" : "Next", Next{})
            | when_(!step_valid(m), disabled())))
```

The wizard stays agnostic about what "valid" means — gating lives in your update,
next to the step's state. `progress()` gives 0..1 for a bar.

### Tree view — `tree_view`

A file explorer, an outline, a nested thread. Your tree DATA is any shape; the
only UI state is which nodes are open, held in a `TreeState` (a set of ids).

```cpp
struct FileNode { std::string id, name; std::vector<FileNode> children; };
struct Model { FileNode root; TreeState tree; };

[&](Toggle t){ m.tree.toggle(t.id); return {m, Cmd::none()}; }

// view: adapt your node type to the four accessors
tree_view(m.root, m.tree,
    [](const FileNode& n){ return n.id; },                 // stable id
    [](const FileNode& n){ return row(icon("file"), text(n.name)); },  // row content
    [](const FileNode& n){ return n.children; },           // child nodes
    [](std::string id){ return Toggle{id}; })              // toggle msg
```

The caret shows only for nodes with children; clicking a branch toggles it.
Rendering is a pure walk of `(data + open set)`, with `role="tree"`/`treeitem`
and `aria-expanded` for free.

### Markdown — `markdown(src)`

Render Markdown to a **node tree**, not raw HTML — so there's no injection
surface at all. Chat messages, comments, README previews, LLM output: safe by
construction, because a `<script>` in the source renders as literal text.

```cpp
markdown(m.comment_body) | max_w(680)
```

Supports headings, **bold**/*italic*/`code` spans, `[links](url)` (url
sanitised), bullet + ordered lists, blockquotes, fenced code blocks, and `---`
rules — the 90% of real prose. Every span goes through waya's escaping, so it's
XSS-safe even on fully untrusted input, and the result styles + diffs like any
other subtree. (For your own trusted rich content, `markup()` injects raw HTML;
for untrusted, always `markdown()`.)

### Presence — who's online / typing

Built on the multiplayer layer (`Cmd::broadcast` / `Sub::on_topic`). `Presence`
is a live roster you fold broadcasts into and prune by heartbeat.

```cpp
struct Model { Presence room; std::string me; };

// broadcast your status on join / heartbeat / keystroke:
Cmd::broadcast("room-42", me + "|typing")     // "<user>|<state>"

// update: fold each peer broadcast in; prune stale on a tick
[&](Peer p){ auto [u,s] = parse_peer(p.payload); m.room.mark(u, s); return {m, Cmd::none()}; }
[&](Tick) { m.room.prune(std::chrono::seconds{10});               return {m, Cmd::none()}; }

// view:
presence_bar(m.room, m.me);     // overlapped avatars of who's online (a typing ring)
typing_line(m.room, m.me);      // "Ada and Bob are typing…" beneath the input
```

`mark(user, state)` stamps a status + fresh timestamp ("left" removes
immediately); `prune(ttl)` drops peers whose heartbeat went silent, so a crashed
tab vanishes on its own. `typers(me)` is the list of who's typing, excluding you.

### Forms

`field(label, control, hint)` labels any control; `input_skin()` themes a raw
input. A validation error is just Model state — `field_invalid(label, control,
error)` (or the `error` last-arg on `text_field`/`email_field`/`password_field`)
renders the field's invalid state: a red ring, `aria-invalid`, and an
`role="alert"` message screen readers announce.

```cpp
text_field("Email", m.email, SetEmail{}, "you@example.com", /*hint*/"",
           /*type*/"email", /*error*/ m.email_error)   // empty error = valid
```

For a WHOLE form — validity gating submit, per-field errors, and "don't nag
before they type" — hold a `Form<>` in your model. It's one value over your
fields:

```cpp
struct Model { Form<> signup; };

// update:
[&](Edit e) { m.signup.set(e.field, e.value); return {m, Cmd::none()}; }
[&](Submit) {
    m.signup.touch_all();                          // reveal every error at once
    m.signup.validate({                            // rules are pure data
        {"email", rules::email("email")},
        {"pw",    rules::min_len("pw", 8)},
    });
    if (!m.signup.valid()) return {m, Cmd::none()};
    return {m, postSignup(m.signup.values())};
}

// view: error_for() only shows once the field is TOUCHED
email_field("Email", f.get("email"), Edit{"email"}, "", "", f.error_for("email"));
button("Sign up", Submit{}) | when_(!f.valid(), disabled());
```

A rule is `Form<>::Rule` = `std::string(const Form&)` (empty = ok), so you can
unit-test validation with no UI. Ready-made rules: `required`, `email`,
`min_len`, `matches` (password confirmation). Errors only render for touched
fields — `touch_all()` on submit reveals them all.

On submit of a NATIVE `<form>`, `FormData` (core) turns the gathered `"a=1&b=2"` string into a
keyed lookup so you read fields by name:

```cpp
form(
    field("Email",    input(m.email)    | name("email")    | input_skin()),
    field("Password", input("")          | name("password") | input_skin()
                                         | input_type("password")),
    button("Sign in", std::monostate{})
) | on_submit([](std::string body){
    auto f = FormData::parse(body);
    return SignIn{ f.get("email"), f.get("password") };
});
```

## Patterns — the page-shaped building blocks

The components above are the *parts*. **Patterns** (`waya/ui/patterns.hpp`, included
by `waya/ui.hpp`) are the page-shaped assemblies you'd otherwise hand-build from
them every time — a nav bar, a hero, a dashboard sidebar, a stat card, a form
field. Each is one call, reads the theme, and is a plain node so `| anymod` still
composes. A full product page is a dozen readable lines instead of hundreds.

### Page structure

- `page_header("Title", "subtitle", actions…)` — a big title + muted subtitle on
  the left, action nodes pushed right.
- `section("Heading", children…)` — a titled block: a small uppercase heading with
  a hairline, then the content.
- `nav_bar(brand, items…)` — a sticky, blurred top nav: brand left, links/actions
  right. Pair with `nav_link("Docs")` (muted, brightens on hover).
- `hero_section("Headline", "subhead", actions…)` — a centred hero: a fluid
  headline, a max-width subhead, and a row of CTAs. The top of a landing page.

### App shells

- `sidebar_shell(brand, {nav_items…}, content)` — a full dashboard layout: a
  fixed, **sticky** sidebar that **collapses on phones** (so the app goes
  full-width) beside a scrolling content column that's capped at a comfortable
  width. The pulse-style layout every dashboard rebuilds, in one call.
- `sidebar_item(icon, "Label", active, msg)` — a keyboard-reachable nav row,
  highlighted when `active`.

```cpp
sidebar_shell(
    row(logo, text("Acme") | semibold) | gap(10) | items_center,
    { sidebar_item("home",     "Overview", m.tab == 0, Nav{0}),
      sidebar_item("user",     "Team",     m.tab == 1, Nav{1}),
      sidebar_item("settings", "Settings", m.tab == 2, Nav{2}) },
    col(page_header("Overview", "Your dashboard"), /* … */))
) | theme(midnight()) | bg_page | fg_text
```

### Data display

- `stat("Label", "Value", "+12%", tone)` — a KPI cell: muted label over a big
  number, with an optional coloured delta chip.
- `metric_card(label, value, delta, tone, chart?)` — `stat` in a card, optionally
  with a chart/sparkline under it. `flex: 1 1 200px`, so a `row(…) | wrap` of them
  tiles responsively.
- `list_row(leading, "Title", "subtitle", trailing?)` — a list row: optional
  leading (avatar/icon), a title + subtitle, optional trailing (a time, badge,
  chevron). Absent leading/trailing add **no** node (no phantom gap).
- `key_value("Label", "Value")` — a label:value row (a definition list).

### Small chrome

- `tag("design")` — a subtle outlined chip (categories, filters); lighter than a
  `badge`. Add `tap` to make it a filter.
- `kbd("⌘")` — a keyboard-key cap for shortcut hints: `row(kbd("⌘"), kbd("K"))`.
- `banner("message", tone)` — a full-width inline alert bar with an icon and a
  tinted background (success/warning/danger/info).
- `empty_state("No results", "hint", action?, icon?)` — the friendly placeholder
  for an empty list/search: a centred icon, title, hint, and optional CTA.
- `code_block("code", "lang")` — a monospace code panel with a language tag.
- `feature_card(icon, "Title", "body", tone)` — a marketing feature cell: an
  accented icon tile over a title + paragraph.

### Forms

A labelled control in **one** call — each wraps `input + input_skin + a real
<label> + on_input`, and maps its live value to your `Msg` via a
`std::string -> Msg` mapper (or a bare `Msg` for toggles/checkboxes), so you
never wire `on_input` by hand:

```cpp
card(
  section("Account",
    email_field   ("Email",    m.email, [](auto v){ return SetEmail{v}; }, "you@x.com", "Never shared."),
    password_field("Password", m.pw,    [](auto v){ return SetPw{v};    }),
    textarea_field("Bio",      m.bio,   [](auto v){ return SetBio{v};   }),
    select_field  ("Plan", {option("Free","free"), option("Pro","pro")}, m.plan,
                            [](auto v){ return SetPlan{v}; }),
    switch_field  ("Notifications", "email + push", m.notify, ToggleNotify{}),
    checkbox_field("I agree to the terms", m.agree, ToggleAgree{})),
  form_actions(button("Cancel", Cancel{}, Variant::secondary),
               button("Save",   Save{})))
```

- `text_field(label, value, to_msg, placeholder?, hint?, kind?)` and the typed
  aliases `email_field` / `password_field` (right input type, mobile keyboard,
  masking).
- `textarea_field(…)` — a resizable multiline field.
- `select_field(label, {options}, chosen, to_msg, hint?)` — a labelled dropdown.
- `date_field` / `time_field` / `datetime_field` `(label, value, to_msg, hint?)`
  — labelled NATIVE pickers (the browser's real calendar/clock, correct on
  mobile); `to_msg` gets the ISO value (`"2024-03-15"`, `"14:30"`).
- `file_field(label, to_msg, accept?, hint?)` — a labelled file picker;
  `to_msg` maps the picked `FileData` (name/mime/decoded bytes) to a Msg. The
  same bytes-in-your-update path serves `on_paste_file(fn)` — paste a screenshot
  or copied image onto any focusable node and it arrives as a `FileData`, no
  dialog.
- `switch_field(title, desc, on, msg)` — a settings row: title + description on
  the left, a toggle on the right.
- `checkbox_field(label, on, msg)` — a checkbox + clickable inline label.
- `form_actions(buttons…)` — a right-aligned button bar for a form footer.

### Dialogs

- `confirm_dialog(open, "Title", "message", "Confirm", on_confirm, on_cancel,
  variant?)` — a ready-made yes/no modal (title, body, Cancel + primary/danger
  action bar). Renders **nothing** when closed. Built on the core `dialog()`.

## Spacing scale

`waya/ui/space.hpp` adds a 4px design-token scale so an app reads on a consistent
rhythm without ad-hoc pixel numbers. `sp(step)` is the scale (`sp(4)` == 16px);
these mods apply it by *step*:

| Mod | Meaning | `p(4)` = |
|-----|---------|----------|
| `p(n)` | padding, all sides | `pad(16)` |
| `px_(n)` | padding inline (l+r) | — |
| `py(n)` | padding block (t+b) | — |
| `gx(n)` | gap between children | `gap(16)` |
| `ma(n)` | margin, all sides | — |
| `mt(n)` / `mb(n)` | margin top / bottom | — |

```cpp
card(…) | p(6) | gx(5)          // padding 24px, gap 20px
row(…)  | gx(3) | py(4)         // gap 12px, vertical padding 16px
```

The scale is the convenient default, never a cage — reach for the raw pixel mods
(`pad(14)`) any time you need an off-scale value. (The shorthands are named to
avoid clashes: `px_` because the core `px` is a `Len` constructor, and `ma` for
margin because a bare `m` would shadow a `Model` named `m` — the universal
variable.)

## Themes

The core ships the token *mechanism* (`theme()`, `fg_*`/`bg_*` token mods, the
`Theme` struct, and the neutral `Theme::dark()`). `waya/ui.hpp` adds
ready-made palettes:

```cpp
root | theme(light());       // crisp light
root | theme(midnight());    // near-black, violet accent
root | theme(ocean());       // deep teal / cyan
root | theme(rose());        // warm light, rose accent
```

These are just `Theme` values — nothing here is privileged over a theme you
write yourself. To recolour just the accent of any theme:

```cpp
root | theme(Theme::dark().tint(0x22c55e));
```

## Writing your own

A component is a function that returns a `NodeRef`. Read any library component
as a worked example — here's `badge`, verbatim:

```cpp
inline NodeRef badge(std::string label, Tone tone = Tone::neutral) {
    auto [bg, fg] = tone_colors(tone);
    return text(std::move(label))
         | pad_x(9) | pad_y(3) | round(999)
         | css("background", bg) | css("color", fg)
         | css("font-size", "12px") | semibold;
}
```

No base class, no macro, no registration. Take arguments, return a node, chain
mods on the result. That's the whole model — and it's why the library needs no
special privileges the core doesn't already give you.

## Internationalization — `Catalog` + `t(key)`

A waya view is a pure function of state, so translation is just a lookup the
view does: swap the active `Catalog` and every `t("save")` renders in the new
language on the next frame — no framework magic, because the whole UI is already
rebuilt from the model each frame.

```cpp
Catalog en = catalog({
    {"greeting", "Hello, {name}!"},
    {"items",    "{n} item|{n} items"},   // singular | plural
    {"save",     "Save"},
});
Catalog fr = catalog({ {"greeting", "Bonjour, {name} !"} });
fr.fallback(en);                          // missing keys fall through to English

// hold the active catalog in your model; the view reads it
text(loc.t("greeting", {{"name", user.name}}));   // "Hello, Ada!"
text(loc.plural("items", cart.size()));           // "3 items"
text(loc.t("save"));                              // "Save" (fr falls back)
```

`t(key, args)` interpolates `{name}` placeholders; `plural(key, n)` picks the
`singular|plural` arm by `|n|==1` and substitutes `{n}`. A missing key renders
the KEY itself (visible, not blank) so untranslated strings are obvious, and
`fallback()` chains catalogs so a partially-translated locale degrades to the
default. It's pure lookup — no macro, no codegen, no global state.
