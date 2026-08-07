# Web Terms Glossary

Every web/browser term you'll meet in these docs, in plain language, with how
waya relates to it. For waya's *own* vocabulary (Surface, Mod, Model, Cmd…) see
the [main Glossary](../glossary.md).

---

**API (Application Programming Interface)**
: In web terms, usually a server that returns *data* (JSON) rather than HTML —
  the thing an SPA's front-end calls. waya has **no separate API**: your
  `update` is the logic, and there's no data-only endpoint to build.

**Attribute**
: Extra info on an HTML tag, in the opening tag: `<a href="…">`, `<input
  type="password">`. In waya, set with `attr(name, value)` or a named mod.

**Backend**
: The server-side of an app — logic, data, secrets. A waya program *is* the
  backend (and the "frontend" too).

**Bubbling (event bubbling)**
: An event fires on the element it happened to, then each ancestor up to the
  document. Lets one high-up listener handle many children. `stop()` prevents a
  click bubbling to an outer `tap()`.

**Bundle**
: The single big JavaScript file an SPA ships to the browser. waya ships **no
  app bundle** — only a tiny fixed runtime script.

**Cache**
: Stored copies of responses so they don't have to be re-fetched. Browsers cache
  images, CSS, etc. Not something you manage by hand in waya.

**Cascade**
: CSS's rule for resolving conflicts (specificity + order + inheritance) when
  several rules target one element. The source of "why is this styled wrong?"
  debugging. waya has no cascade — a mod applies where you put it; later wins.

**Client / client-side**
: The user's browser, and code that runs in it (JavaScript). Fast to respond but
  public and stateless-until-it-asks. waya keeps logic **server-side** instead.

**Cookie**
: A small piece of data the server sets and the browser sends back on each
  request — used for sessions/login. waya can set/read them for auth.

**CORS (Cross-Origin Resource Sharing)**
: Browser rules about which sites a page may fetch from. Relevant when calling
  third-party APIs; not needed within a single waya app (same origin).

**CSS (Cascading Style Sheets)**
: The language for styling — colours, fonts, spacing, layout. waya generates it
  from mods (`fg`, `pad`, `row`…); see [Browser Parity](../14-browser-parity.md).

**DOM (Document Object Model)**
: The browser's live in-memory tree of the page's elements. JavaScript changes
  the DOM to change the screen. waya diffs on the server and patches the DOM for
  you; you never touch it.

**Element / Tag**
: A piece of HTML: `<div>`, `<p>`, `<button>`. Elements nest into a tree. waya's
  builders (`box`, `text`, `image`…) produce elements.

**Event**
: Something the user did — click, keypress, scroll — delivered to a DOM node.
  waya turns events into typed messages (`tap(Msg{})`, `on_input(…)`).

**Event loop**
: The browser's single-threaded queue that processes events, timers, and network
  callbacks one at a time. Slow JS freezes the page. waya's handlers run in C++
  on the server, off this loop.

**Flexbox**
: The modern CSS layout model — a container arranges children in a row/column
  with alignment and spacing. `row()`/`col()` in waya are flex boxes; `gap`,
  `center`, `grow` are its properties, named.

**Frontend**
: The part of an app the user sees and interacts with — traditionally HTML/CSS/JS
  in the browser. In waya it's your `view`, rendered server-side.

**Grid (CSS Grid)**
: A 2-D CSS layout model (rows *and* columns) for tables and dashboards. waya:
  `grid()`, `grid_cols`, `grid_areas`, `columns()`.

**Hydration**
: An SPA re-running its JS in the browser to make server-rendered HTML
  interactive. Fiddly and error-prone. waya needs **no hydration** — there's no
  client app to attach.

**HTML (HyperText Markup Language)**
: The language describing a page's content and structure as a tree of tags. waya
  generates it from node builders; user text is auto-escaped.

**HTTP (HyperText Transfer Protocol)**
: The request/response language between browser and server. waya's runtime speaks
  it for you (renders your `view` as the response body).

**HTTPS**
: HTTP encrypted with TLS. Standard for production. Terminate it at a reverse
  proxy in front of waya (see [Deployment](../17-deployment.md)).

**Inheritance (CSS)**
: Some properties (`color`, `font`) pass from a parent element to its children
  unless overridden. waya's `fg` cascades to descendant text, matching this.

**JavaScript (JS)**
: The browser's programming language — makes pages interactive. waya replaces it
  with C++ `Model`/`update`/`view`; the only JS is a tiny fixed runtime you never
  write.

**JSON (JavaScript Object Notation)**
: A text format for data (`{"name": "Ada"}`). What most APIs speak. waya reads
  and writes it via `<waya/json.hpp>` for fetches and request bodies.

**Layout (reflow)**
: The browser computing where every box goes and how big it is. Expensive; done
  after any DOM/CSS change. waya minimises it by patching only what changed.

**Margin / Padding / Border**
: The box-model spacing layers — margin (outside), border (the line), padding
  (inside). waya: `margin`, `border(w,c)`, `pad`/`pad_x`/`pad_y`.

**Media query**
: A CSS rule that applies only at certain screen sizes (`@media (max-width: …)`).
  waya's layout is responsive without them; use `at`/`below` when you want
  explicit breakpoints.

**MPA (Multi-Page App)**
: The original web model — every click is a full page reload from the server.
  Simple and SEO-friendly but jarring. waya keeps the SEO, drops the reload.

**Origin**
: The scheme + host + port of a URL (`https://example.com:443`). The browser's
  security boundary. A waya app is a single origin.

**Paint**
: The browser filling in pixels after layout. Combined with layout, this is the
  expensive part waya minimises via delta patches.

**Parsing**
: Turning text (HTML, CSS, JSON) into structured data the program can use. The
  browser parses HTML into the DOM.

**Port**
: A numbered "door" on a server (HTTP=80, HTTPS=443). `live({.port=8080})` picks
  waya's.

**Reflow**
: See **Layout**.

**Rendering**
: Turning data into on-screen output. In waya you render in C++ (`view`); the
  runtime turns it into HTML/CSS and keeps the browser in sync.

**Request / Response**
: The two halves of an HTTP exchange — the browser asks, the server answers with
  content and a status code (`200 OK`, `404`).

**Responsive design**
: A UI that adapts to any screen size. waya's layout primitives are responsive by
  default (they reflow on their own); breakpoints (`at`/`below`) are optional.

**Reverse proxy**
: A server (nginx, Caddy) that sits in front of your app to terminate HTTPS,
  serve static files, and route traffic. Recommended in front of waya in
  production.

**Route / Routing**
: Mapping a URL path to a screen (`/post/:slug` → a post view). waya has a
  built-in [router](../09-routing-seo.md).

**Selector (CSS)**
: The part of a CSS rule that picks which elements it styles (`.card`, `h1`,
  `#main`). waya has no selectors — you style nodes where you build them.

**Semantic HTML**
: Using meaningful tags (`<nav>`, `<main>`, `<article>`, headings) instead of
  `<div>` for everything — better for accessibility and SEO. waya: `as_nav`,
  `as_main`, `as_article`, `as("…")`.

**Server / server-side**
: The machine (and code) that answers requests. A waya program is the server; its
  logic runs here, not in the browser.

**Session**
: One user's connection and its associated state. Each browser connected to waya
  gets its **own `Model`**, isolated from others.

**SPA (Single-Page App)**
: An app that loads one HTML shell + a JS bundle that runs the whole UI in the
  browser and calls a separate API. Smooth but complex; waya achieves the
  smoothness without the bundle or the second program.

**SSR (Server-Side Rendering)**
: Building the HTML on the server so the first response is complete content.
  waya is SSR **by construction** — every route server-renders.

**State**
: Everything an app must remember to draw the current screen. In waya it's one
  `Model` struct on the server — the single source of truth.

**Status code**
: The number in an HTTP response saying how it went: `200` OK, `301` redirect,
  `404` not found, `500` server error.

**Tag**
: See **Element**.

**URL (Uniform Resource Locator)**
: A web address: scheme + host + port + path + query. The path drives waya's
  router.

**Viewport**
: The visible area of the page in the browser window. `vw`/`vh` units and
  `100vh`/`100dvh` heights are relative to it; waya's `viewport()` layout fills
  it.

**Virtual DOM**
: React's technique — describe the whole UI, diff descriptions, apply the
  difference to the real DOM. waya uses the same *idea* but diffs **on the
  server in C++** and streams only the patch.

**WebSocket**
: A persistent two-way connection between browser and server (unlike one-shot
  HTTP). Lets the server push updates any time. waya opens exactly one per
  session and streams UI deltas over it.

**XSS (Cross-Site Scripting)**
: A security hole where attacker-supplied text runs as code in a victim's
  browser. waya auto-escapes all text and sanitises URL schemes, so the common
  cases are safe by default; `markup()` is the one place you must supply trusted
  HTML.

---

Ready to see how all of this becomes a tiny, concrete API? Continue to
[The Mental Model](../02-mental-model.md).
