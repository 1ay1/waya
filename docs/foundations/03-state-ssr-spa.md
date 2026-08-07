# State, SSR & Single-Page Apps

The last foundations page. You now know how the browser works, the three
languages, and how the DOM renders. This page covers the two questions every web
architecture must answer — **where does the app's data live?** and **where does
the HTML get built?** — and shows why waya answers them differently from React
and friends, and why that makes it simpler.

---

## State: your app's memory

**State** is everything your app needs to remember to draw the current screen:
the list of items, which tab is open, the text in a search box, the logged-in
user, whether a dialog is showing. The UI is a *function of state* — change the
state, the screen must change to match.

The central architectural question is: **where does state live?**

- **On the server** — in your backend program's memory or database.
- **In the browser** — in JavaScript variables.
- **Both** — the usual, messy answer, where you constantly sync the two.

Most bugs in web apps are **state-sync bugs**: the browser thinks one thing, the
server thinks another, the screen shows a third. Keeping multiple copies of state
agreeing is genuinely hard.

!!! note "waya: one source of truth"
    In waya, **all state is one `Model` struct on the server.** There is no
    client-side copy to drift out of sync. The browser holds *no* app state — it
    only paints what the server sends and reports clicks. This single decision
    eliminates the entire category of state-sync bugs.

---

## The two eras of web apps

### 1. The multi-page app (MPA) — the original web

Every click is a fresh HTTP request; the server builds a **whole new HTML page**
and sends it; the browser throws away the old page and draws the new one. The
screen blanks and reloads on every action.

- ✅ Simple, and the server always has the full picture.
- ✅ Search engines love it — every URL returns complete HTML.
- ❌ Jarring and slow — a full reload for every interaction; loses scroll
  position and focus.

### 2. The single-page app (SPA) — React, Vue, Angular

The server sends **one** HTML page with a big **JavaScript bundle**. That JS
takes over: it runs the whole UI in the browser, holds the state client-side,
and when you click, it updates the DOM *without a reload* and fetches data from a
separate **API** (a server that returns JSON, not HTML).

- ✅ Feels smooth — no full reloads, instant interactions.
- ❌ You now maintain **two programs**: the front-end (JS) and the back-end
  (API), often in different languages, with duplicated types and logic.
- ❌ Ship a large JS bundle the user must download and the browser must run
  before anything is interactive.
- ❌ Bad for SEO by default — the first response is an near-empty shell; content
  appears only after the JS runs, which many crawlers don't wait for. (Fixed by
  bolting SSR back on — see below.)
- ❌ **State-sync everywhere** — client state vs. server state vs. the URL.

---

## SSR and hydration (the SPA's patch)

To fix the SPA's blank-first-response problem, teams add **SSR** (Server-Side
Rendering): the server runs the JavaScript app *once* to produce real HTML for
the first load, so the page shows content immediately and crawlers can read it.

But then the browser must **hydrate**: download the same JS bundle, re-run it,
and "attach" it to the server-rendered HTML so it becomes interactive. Hydration
is fiddly, error-prone (server and client HTML must match exactly), and means you
ship *and run* the JS anyway. It's a patch on a patch.

```
SPA + SSR + hydration:
  server renders HTML  →  browser shows it (not yet interactive)
                       →  browser downloads the JS bundle
                       →  browser re-runs the app to "hydrate"
                       →  now it's interactive   (a gap the user can feel)
```

---

## waya's model: server-rendered, live, no bundle

waya keeps the **good parts of both eras** and drops the bad:

1. **First load is real HTML** (like an MPA / SSR) — the server renders your
   `view` for the requested route and sends complete, styled, crawlable HTML.
   Instant paint, perfect SEO, no bundle to wait for.
2. **Then it stays live** (like an SPA) — but instead of shipping a JS app, waya
   opens **one WebSocket**. Interactions are messages to the server; the server
   updates the `Model`, re-renders, diffs, and pushes back only the **minimal
   change**. No reload, no jank.
3. **No hydration** — there's nothing to hydrate, because there's no client app.
   The tiny fixed script that opens the socket and applies patches is the same
   for every app and needs no matching or re-running of your logic.

```
waya:
  server renders HTML  →  browser shows it (instantly, crawlable)
                       →  browser opens ONE WebSocket + tiny fixed script
                       →  click → message → server update → diff → patch → applied
                          (no bundle, no hydration, no client state)
```

### The comparison

| | MPA | SPA (React) | **waya** |
|---|---|---|---|
| First response | full HTML | empty shell | **full HTML** |
| SEO | great | needs SSR+hydration | **great, built-in** |
| Interactions | full reload | smooth (client JS) | **smooth (server + WS)** |
| Where state lives | server | client (+ server) | **server only** |
| Client JS bundle | none | large | **tiny fixed script** |
| Programs to maintain | 1 | 2 (front + API) | **1** |
| Language | one | usually JS + backend | **just C++** |
| Sync bugs | few | many | **none by design** |

---

## "But isn't a round-trip slow?"

A fair question: if every click goes to the server, isn't that laggy compared to
a client-side app that responds locally?

In practice, no — and here's why:

- The round-trip is one **WebSocket message** on an already-open connection (no
  new HTTP handshake), carrying a few bytes each way. On a normal connection
  that's a few to a few dozen milliseconds — below the threshold most
  interactions need.
- Only the **delta** is sent, not a re-rendered page.
- waya offers **optimistic UI** for the rare case where you want instant local
  feedback before the server confirms (a button dims the moment it's clicked).
- And you avoid the SPA's *startup* cost entirely — no multi-hundred-KB bundle to
  download and execute before the app works.

For the vast majority of apps — dashboards, tools, content sites, forms,
CRUD — the server round-trip is imperceptible, and the simplicity is enormous.
For a 120fps game or an offline-first app, a client-side engine is the right
tool; waya is honest about that.

---

## Why this makes waya *simpler*

Every simplification in waya traces back to "state lives on the server, and the
server renders":

- **No API layer.** Your `update` *is* the backend logic; there's no separate
  JSON API to design, version, and call.
- **No client state management.** No Redux, no stores, no `useState` — one
  `Model` struct.
- **No build step for the front-end.** No bundler, no transpiler, no
  `node_modules`. You compile one C++ program.
- **No duplicated types.** The data is C++ structs, used directly in `view`. No
  hand-mirroring types between a backend and a frontend.
- **Testable logic.** `update` and `view` are pure functions of the `Model` —
  test them with `==`, no browser or server needed.

You get the smoothness of an SPA and the SEO of server rendering, with the
mental model of a single, ordinary program.

---

## Recap

- **State** is your app's memory; the screen is a function of it. Keeping
  multiple copies in sync is the top source of web bugs.
- **MPAs** reload the whole page (simple, great SEO, jarring). **SPAs** run a JS
  app in the browser (smooth, but two programs, big bundle, bad default SEO,
  sync bugs). **SSR + hydration** patches the SPA's SEO at the cost of
  complexity.
- **waya** sends real HTML first (great SEO, instant), then goes live over one
  WebSocket with server-side diffing — no bundle, no hydration, **one program,
  one source of truth, in C++.**

You now have the full foundation. Head to the [Mental Model](../02-mental-model.md)
to see how these ideas become the tiny, concrete API you'll actually write —
or jump straight into the [Tutorial](../tutorial-todo.md) and build something.
