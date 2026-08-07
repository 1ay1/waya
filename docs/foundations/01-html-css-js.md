# HTML, CSS & JavaScript

The browser understands exactly three languages. Every website — no matter how
complex — is built from these three, and only these three. Understanding what
each one does makes waya click, because **waya generates all three for you from
C++.** This page teaches each from zero.

---

## HTML — the content and structure

**HTML** (HyperText Markup Language) describes *what's on the page*: headings,
paragraphs, images, buttons, lists. It's not a programming language — it has no
logic, no variables. It's a way to **mark up** text so the browser knows what
each piece *is*.

HTML is made of **tags** wrapped in angle brackets. Most come in pairs — an
opening tag and a closing tag with content between:

```html
<h1>Welcome</h1>                        <!-- a big heading -->
<p>This is a paragraph of text.</p>     <!-- a paragraph -->
<img src="photo.jpg" alt="A photo">     <!-- an image (no closing tag needed) -->
<button>Click me</button>               <!-- a button -->
```

Tags **nest** to form a tree:

```html
<div>                          <!-- a generic container -->
  <h1>Title</h1>
  <p>Some <strong>bold</strong> text.</p>
</div>
```

That nesting is the key idea: **an HTML document is a tree of elements.** A `div`
contains an `h1` and a `p`; the `p` contains text and a `strong`. This tree is
what the browser draws, top to bottom.

Tags can carry **attributes** — extra info in the opening tag:

```html
<a href="https://example.com">a link</a>   <!-- href = where the link goes -->
<input type="password" placeholder="Password">
<div class="card" id="main">…</div>          <!-- class/id used for styling -->
```

### Semantic tags matter

Some tags mean something specific: `<nav>` (navigation), `<main>` (the main
content), `<header>`, `<footer>`, `<article>`, `<button>`, `<h1>`–`<h6>`
(headings in order). Using the *right* tag helps screen readers, search
engines, and keyboard users. Using `<div>` for everything works visually but is
worse for accessibility and SEO.

### How waya does HTML

**You never write HTML.** You describe the tree with waya's node builders, and
waya generates the HTML:

```cpp
col(                          //  →  <div style="…flex column…">
    text("Welcome") | as("h1"),   //  →  <h1>Welcome</h1>
    text("Some text")             //  →  the text
)
```

- `text`, `box`, `image`, `input`… are the pieces (waya's version of tags).
- Nesting `col(text(...), text(...))` builds the same tree HTML would.
- Semantic tags are one mod away: `as_nav`, `as_main`, `as_article`, or
  `as("any-tag")`.
- **Everything is escaped automatically** — `text("<script>")` shows the literal
  characters, never runs. (In raw HTML, that's a security hole you have to guard
  by hand.)

See [The Vocabulary](../03-vocabulary.md) for every builder.

---

## CSS — the styling and layout

HTML says *what* things are; **CSS** (Cascading Style Sheets) says *what they
look like* — colours, fonts, sizes, spacing, positioning. Without CSS, a page is
black text on a white background, stacked top to bottom.

CSS is a list of **rules**. Each rule picks some elements (a **selector**) and
sets **properties** on them:

```css
h1 {                      /* selector: every <h1> */
  color: white;           /* property: value */
  font-size: 32px;
  margin-bottom: 16px;
}

.card {                   /* selector: every element with class="card" */
  background: #1e293b;
  padding: 20px;
  border-radius: 12px;
}
```

### The box model

Every element is a **box** with four layers, from inside out:

```
┌─────────────────────────────┐  ← margin (space OUTSIDE the box)
│  ┌───────────────────────┐  │  ← border
│  │  ┌─────────────────┐  │  │  ← padding (space INSIDE, around content)
│  │  │    content      │  │  │
│  │  └─────────────────┘  │  │
│  └───────────────────────┘  │
└─────────────────────────────┘
```

- **content** — the text/image itself.
- **padding** — space between the content and the border.
- **border** — a line around the padding.
- **margin** — space *outside*, pushing other boxes away.

This is why you'll see `pad(16)` (padding), `margin(8)`, `border(1, color)`, and
`round(12)` (border-radius) all over waya — they're the box model, named.

### Flexbox — arranging boxes

The modern way to lay out boxes is **flexbox**. You make a container a "flex
box" and it arranges its children in a **row** or **column**, with control over
spacing and alignment:

```css
.row {
  display: flex;
  flex-direction: row;      /* children left-to-right */
  gap: 12px;                /* space between children */
  justify-content: center;  /* alignment along the row */
  align-items: center;      /* alignment across the row */
}
```

In waya, `row(...)` and `col(...)` **are** flex boxes, and `gap`, `center`,
`between`, `grow`, `wrap` are the flex properties, named. You never write
`display:flex` — `row()` means it.

### The cascade (why it's "Cascading")

Multiple CSS rules can target the same element; the browser resolves conflicts
by **specificity** and **order**. And many properties (like `color`, `font`)
**inherit** — a child uses its parent's value unless it sets its own. This
"cascade" is powerful but is the source of endless "why is my text blue?"
debugging.

### How waya does CSS

**You never write CSS.** You apply **mods** with `|`, and waya generates the
stylesheet:

```cpp
text("Welcome")
    | fg(0xffffff)          //  →  color: white
    | font(32)              //  →  font-size: 32px
    | pad(20)               //  →  padding: 20px
    | round(12)             //  →  border-radius: 12px
    | bg(0x1e293b)          //  →  background: #1e293b
```

waya's advantages over raw CSS:

- **No cascade surprises.** A mod applies to the node you pipe it onto. There's
  no "some other rule won"; later mods win, predictably.
- **No selectors.** You don't name elements and target them from afar — you
  style them where you build them.
- **Deduplicated automatically.** waya interns identical styles into shared
  classes, so a page with 500 identical cards ships one rule, not 500.
- **Responsive by default.** Layout primitives adapt to space on their own (see
  [Layout](../05-layout.md)); you rarely write a media query.

Everything CSS can do has a named mod — see [Browser Parity](../14-browser-parity.md).
The `css("property", "value")` escape hatch is always there for the rare
uncovered case.

---

## JavaScript — the behaviour

HTML is static; CSS is static. **JavaScript** is the *programming language* that
runs inside the browser and makes pages **interactive**: respond to clicks,
change content, fetch data, animate.

```javascript
// find the button, and when it's clicked, change the heading
document.querySelector("button").addEventListener("click", function () {
  document.querySelector("h1").textContent = "Clicked!";
});
```

JavaScript can:

- **React to events** — clicks, typing, scrolling, key presses.
- **Change the page** — modify the HTML tree live (add/remove/update elements).
- **Talk to the server** — send background requests, open WebSockets.
- **Hold state** — remember things (a counter, a form's contents) in variables.

This is where web development gets *hard*. In a big app, JavaScript has to keep
the page in sync with a pile of state, wire up hundreds of event handlers, avoid
memory leaks, and manage the network — all in a language that started as a
10-day prototype. Entire frameworks (React, Vue, Angular) exist just to tame
this complexity.

### How waya does JavaScript

**You never write JavaScript.** This is waya's biggest simplification.

- **Events** → you attach a message: `text("Save") | tap(Save{})`. When clicked,
  waya's runtime sends `Save{}` to your `update` function *on the server*.
- **State** → lives in your `Model` struct, on the server. No client-side
  variables to keep in sync.
- **Changing the page** → you just return a new `view`; waya computes the
  difference and updates only what changed.
- **Talking to the server** → there's no separate server; your logic already
  runs there.

The only JavaScript in a waya app is a **small, fixed script** (the same for
every app) that opens the WebSocket, reports clicks, and applies updates. You
never see it, touch it, or ship a bundle.

```cpp
// The waya version of the JS example above — all C++, no browser code:
struct Model { std::string title = "Not clicked"; };
struct Click {};

static Model update(Model m, Click) { m.title = "Clicked!"; return m; }

static NodeRef view(const Model& m) {
    return col(
        text(m.title) | as("h1"),
        text("Click me") | as("button") | tap(Click{})
    );
}
```

Click the button → `update` runs on the server → the heading changes → waya
streams the one-word difference to the browser. No event listeners, no DOM code,
no state syncing.

---

## The whole picture

| The browser's language | What it does | How waya does it |
|---|---|---|
| **HTML** | content & structure (a tree of tags) | node builders (`box`, `text`, `image`…), generated automatically |
| **CSS** | styling & layout (rules on elements) | mods applied with `\|` (`fg`, `pad`, `row`, `gap`…), generated & deduped |
| **JavaScript** | behaviour, state, events, networking | your pure `Model`/`update`/`view` in C++, run on the server |

Three languages, three sources of complexity — replaced by one: **C++, on the
server.** That's the entire value proposition, and now you know exactly what it's
replacing.

Next: [The DOM & Rendering](02-dom-and-rendering.md) — how the browser turns
HTML+CSS+JS into pixels, and why keeping a page in sync is the hard problem waya
solves for you.
