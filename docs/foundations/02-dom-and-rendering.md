# The DOM & Rendering

You've seen HTML (content), CSS (style), and JavaScript (behaviour). This page
explains what the browser *does* with them — how text turns into pixels, what
the "DOM" is, how events work, and **why keeping a live page correct is the hard
problem** that most of web development's complexity comes from. waya solves this
problem for you; understanding it shows you *how*.

---

## From HTML text to pixels

When the browser receives HTML, it doesn't draw it directly. It goes through a
pipeline:

```
HTML text  ──parse──▶  the DOM tree  ──+CSS──▶  layout  ──▶  paint  ──▶  pixels
```

1. **Parse.** The browser reads the HTML text and builds an in-memory **tree of
   objects** — one object per element. This tree is the **DOM**.
2. **Style.** It matches CSS rules to each node, computing every element's final
   colour, font, size, etc.
3. **Layout** (aka *reflow*). It calculates *where every box goes and how big it
   is* — a surprisingly hard geometry problem (flexbox, text wrapping, nested
   sizes all interact).
4. **Paint.** It fills in pixels — text, backgrounds, borders, images.
5. **Composite.** It layers everything (things stacked with z-index, transforms)
   into the final image you see.

Steps 3–5 are expensive. A big part of making a fast web app is **doing them as
little as possible** — which, as you'll see, is exactly what waya optimises.

---

## The DOM: a live tree of objects

The **DOM** (Document Object Model) is the browser's in-memory representation of
the page — the parsed HTML, turned into a tree of objects that JavaScript can
read and change.

```
        document
           │
          html
         /    \
      head     body
                 │
                div (class="card")
               /    \
             h1      p
             │       │
          "Title"  "Text"
```

The critical thing: **the DOM is *live*.** If JavaScript changes a DOM node —
say it sets an `h1`'s text — the browser re-runs layout and paint for the
affected part, and the screen updates. This is how pages change without
reloading.

```javascript
// this line changes the actual pixels on screen:
document.querySelector("h1").textContent = "New title";
```

The DOM is also **slow to touch** relative to raw computation. Reading a node's
size, or changing many nodes, forces the browser to recompute layout. Naïve code
that updates the DOM in a loop can be janky. Managing DOM updates efficiently is
half of what front-end frameworks do.

!!! note "waya and the DOM"
    In waya, the DOM lives only in the browser, and **you never touch it.** waya
    keeps its own copy of your last surface *on the server*, figures out the
    minimal set of DOM changes needed, and sends just those. The browser's tiny
    built-in script applies them by node path. You get correct, minimal DOM
    updates without ever writing DOM code — see [How diffing works](#the-diff)
    below.

---

## Events: how the page hears the user

When the user clicks, types, scrolls, or presses a key, the browser creates an
**event** and delivers it to the DOM node where it happened. JavaScript can
**listen** for events and run code in response:

```javascript
button.addEventListener("click", () => { /* do something */ });
```

### Bubbling

Events **bubble**: a click on a button inside a card inside the page fires on the
button, then its parent, then *its* parent, up to the document. This lets you put
one listener high up and handle events from many children (**event delegation**)
— efficient, but a source of subtle bugs (a click meant for a child also
triggers a parent's handler).

### The event loop

JavaScript in the browser is **single-threaded**: one thing at a time. Events,
timers, and network responses queue up and are processed one by one on the
**event loop**. If your code does something slow, the whole page freezes — no
clicks register, no animations run — until it finishes. This is why heavy work
must be broken up or moved off the main thread.

!!! note "waya and events"
    You wire an event to a **message**: `text("Save") | tap(Save{})`. waya's
    built-in client script listens for the DOM event (using efficient
    delegation, handling bubbling correctly — the `stop()` mod is there for when
    you *don't* want a click to bubble to an outer `tap`), and sends the message
    to your `update` on the server. Your handlers run in C++, off the browser's
    single thread, so a slow handler never freezes the UI. See
    [Events & Inputs](../07-events.md).

---

## Why keeping a page correct is hard

Here's the crux. A real app's screen depends on a pile of **state**: the list of
todos, the current filter, whether a modal is open, the text in each field, the
logged-in user. Every time *any* of that changes, the screen must update to
match — and *only* the parts that actually changed, or the app feels slow and
loses things like scroll position and input focus.

Doing this by hand in JavaScript means, for every possible change, writing code
that finds the right DOM nodes and updates exactly them:

```javascript
// when a todo is checked, by hand:
todoElement.querySelector(".checkbox").checked = true;
todoElement.classList.add("done");
updateTheCounter();
maybeHideIfFilterIsActive();
// … and get every case right, forever
```

This is **error-prone and doesn't scale.** Miss a case and the screen shows
stale data. It's the single biggest source of front-end bugs and the reason
frameworks exist.

### Two solutions the industry uses

1. **The Virtual DOM (React).** You write a function from state to a *description*
   of the whole UI. On every change, the framework builds a new description,
   **diffs** it against the old one, and applies only the differences to the real
   DOM. You stop hand-writing DOM updates; you just describe *what the UI should
   be* for the current state.

2. **waya's approach.** The same core idea — *describe the whole UI as a function
   of state, let the framework diff and apply minimal changes* — but the
   description and the diff live **on the server, in C++**, and only the tiny
   resulting patch crosses the network.

---

## The diff

This is waya's engine. Each time your state changes:

1. waya calls your `view(model)` — a pure function returning a **surface** (a
   tree of nodes), the complete description of the UI *right now*.
2. It **diffs** that surface against the one from the previous frame — the tree
   it's currently showing.
3. It produces a **minimal patch**: "the text of node 3.1 changed to '5'",
   "insert this subtree at index 2", "remove node 0". Just the differences.
4. It sends the patch (a few bytes, in a compact binary form) down the WebSocket.
5. The browser's built-in script applies each change to the real DOM by node
   path — touching only what changed, so layout/paint stay cheap and focus/scroll
   are preserved.

```
your view(model)  →  new surface tree
                          │  diff against previous
                          ▼
              minimal patch: [set-text 3.1 "5"]
                          │  over the WebSocket
                          ▼
   browser applies it  →  one text node updates, nothing else re-renders
```

You write `view` naïvely — *rebuild the whole tree every time* — and still get
surgical DOM updates, because the diff figures out the difference. That's the
whole trick, and it's why you never write an event handler that pokes the DOM.

### Keyed lists

When a list reorders (drag a card to a new column), a naïve diff would rebuild
everything. Give repeated items a `key(...)` and waya matches them by key across
frames, emitting **move** operations — so a reordered list glides, and focus and
in-progress animations survive. (See the `nova` and `flow` examples.)

---

## Recap

- The browser turns HTML into a **DOM tree**, applies CSS, then does **layout**
  and **paint** to make pixels. Layout/paint are expensive.
- The **DOM is live**: change a node and the screen updates — but touching it a
  lot is slow.
- **Events** flow to nodes and **bubble** up; JavaScript is **single-threaded**,
  so slow handlers freeze the page.
- The hard problem is **keeping the DOM in sync with your state, minimally.**
  Frameworks solve it by *diffing a description of the UI*.
- **waya diffs on the server in C++** and streams a tiny patch, so you write a
  simple `view` and never touch the DOM, events, or the render pipeline by hand.

Next: [State, SSR & Single-Page Apps](03-state-ssr-spa.md) — where your app's
data lives, and why waya renders on the server instead of shipping a JavaScript
app.
