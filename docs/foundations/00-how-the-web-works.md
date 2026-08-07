# How the Web Works

New to web development? This page explains what actually happens when you open a
website — the browser, the server, and the conversation between them — so the
rest of the docs (and *why* waya is built the way it is) makes complete sense.
If you already know HTTP and the request/response cycle, skip to
[HTML, CSS & JavaScript](01-html-css-js.md).

No prior web experience is assumed. If you can write a C++ program, you can
follow every word here.

---

## The two computers

Every website involves **two programs on two computers** talking to each other:

1. **The browser** (Chrome, Firefox, Safari) — runs on the *user's* computer.
   Its job is to fetch content and **draw it on screen**, and to send the user's
   clicks and keystrokes back.
2. **The server** — a program running on *your* computer (or a rented one in a
   data centre). Its job is to **answer requests**: "give me the home page,"
   "here's a new comment, save it."

The browser and the server are usually on different machines connected by the
internet. They never share memory or variables — they can only send each other
**messages** over the network. Everything about web development follows from
that one fact: *two programs that can only pass messages.*

!!! note "waya is a server"
    A waya program **is the server.** You write it in C++, it runs on your
    machine, and browsers connect to it. waya's job is to decide what each
    browser should show and to keep it up to date. You never write a separate
    "front-end" and "back-end" — there's one program.

---

## What a URL is

When you type `https://example.com/blog/hello` you're giving the browser an
**address** with parts:

| Part | Example | Meaning |
|---|---|---|
| **scheme** | `https` | how to talk (HTTPS = encrypted HTTP) |
| **host** | `example.com` | which server (a name that resolves to an IP address) |
| **port** | `:443` (implied) | which "door" on that server (HTTP=80, HTTPS=443) |
| **path** | `/blog/hello` | which resource on that server |
| **query** | `?sort=new` | extra parameters |

The browser looks up the host's address (via DNS, the internet's phone book),
opens a connection to that server on that port, and asks for the path.

!!! note "waya and URLs"
    `live<App>({ .port = 8080 })` starts your server listening on port 8080, so
    the address is `http://localhost:8080/`. The **path** (`/blog/hello`) is how
    waya's [router](../09-routing-seo.md) decides which screen to show.
    `localhost` means "this same computer."

---

## HTTP: the request/response cycle

The browser and server talk in a language called **HTTP** (HyperText Transfer
Protocol). It is a strict back-and-forth: the browser sends **one request**, the
server sends back **one response**. That's it — one question, one answer, then
the connection is usually done.

### A request looks like this

```
GET /blog/hello HTTP/1.1
Host: example.com
Accept: text/html
```

- **`GET`** is the *method* — "fetch this." (Other methods: `POST` = "here's
  data to save," `PUT`, `DELETE`.)
- **`/blog/hello`** is the path.
- The lines below are **headers** — metadata (what formats the browser accepts,
  cookies, etc.).

### A response looks like this

```
HTTP/1.1 200 OK
Content-Type: text/html

<!DOCTYPE html><html><body><h1>Hello</h1></body></html>
```

- **`200 OK`** is the *status code* — success. (Others: `404 Not Found`,
  `500 Server Error`, `301 Moved`.)
- **`Content-Type`** tells the browser what the body is (HTML, an image, JSON…).
- After the blank line comes the **body** — here, the HTML the browser will draw.

That's the whole model: **the browser asks, the server answers with content.**

!!! tip "You never write HTTP by hand in waya"
    waya's runtime speaks HTTP for you. When a browser requests a route, the
    runtime renders your `view` to HTML and sends it as the `200 OK` body — all
    automatically. You just write `view`.

---

## The problem: HTTP is one-shot

Classic HTTP has a limitation: it's **request → response → done.** The server
can't later say "hey, something changed, update the page." Once the response is
sent, the conversation is over. If you want the page to change (a new chat
message arrives, a number ticks up), the browser has to **ask again**.

Early websites solved this by making the browser re-request the *entire page* on
every click — the screen would blank and reload. Slow and jarring.

To make pages feel alive, the web added two things:

1. **JavaScript** — a programming language that runs *inside the browser*, so it
   can change the page without a full reload and send small background requests.
   (See [HTML, CSS & JavaScript](01-html-css-js.md).)
2. **WebSockets** — a way to keep the connection **open** so the server *can*
   push updates to the browser at any time, and vice-versa. A two-way pipe
   instead of one-shot questions.

!!! note "How waya uses these"
    waya sends the first page over plain HTTP (so it loads instantly and search
    engines can read it), then opens **one WebSocket**. From then on, when your
    app's state changes, the server pushes the *minimal update* down that socket
    and a tiny built-in script applies it. You never write the JavaScript or
    manage the socket — but now you know what's happening underneath.

---

## Server-side vs client-side

A crucial distinction you'll see everywhere:

- **Server-side** = code that runs on the server (your C++ waya program). It has
  your data, your logic, your secrets. The user never sees it.
- **Client-side** = code that runs in the browser (JavaScript). It's fast to
  respond (no network trip) but it's *public* — anyone can read it — and it
  starts with no data until it asks the server.

Most modern frameworks (React, Vue) put a *lot* of logic client-side: they ship
a big JavaScript program that runs the whole UI in the browser and chats with a
separate server API.

**waya's bet is the opposite: keep the logic server-side.** Your `Model`,
`update`, and `view` all run on the server in C++. The browser only *paints* and
*reports clicks*. The benefits — no separate API, no client bundle to build, all
your logic in one testable place — come from this choice.

```
Traditional SPA:                    waya:

  browser                            browser
  ┌─────────────┐                    ┌─────────────┐
  │ big JS app  │  ⇄ API ⇄  server   │ paint +     │ ⇄ deltas ⇄  server
  │ (the logic) │                    │ report taps │             (the logic,
  └─────────────┘                    └─────────────┘              in C++)
```

---

## What "rendering" means

**Rendering** is turning your data into something on screen. It happens in
stages:

1. Your code produces a description of the UI (in waya: a `view` returning a
   tree of nodes).
2. That becomes **HTML** — the content and structure.
3. The browser reads the HTML, applies **CSS** — the styling — and computes where
   every box goes (**layout**), then draws pixels (**paint**).
4. **JavaScript** can later change the HTML, triggering re-layout and re-paint.

waya does step 1 in C++, generates the HTML+CSS for step 2 automatically, and
handles step 4's updates by streaming just the changed parts. You think only in
step 1.

---

## Recap

- A website is **two programs** (browser + server) that can only **pass
  messages** over the network.
- They talk in **HTTP**: the browser sends a **request**, the server sends a
  **response** with content.
- Plain HTTP is one-shot, so live pages use **JavaScript** (to change the page
  in place) and **WebSockets** (to keep the connection open for server pushes).
- Logic can live **client-side** (in the browser) or **server-side** (on the
  server). **waya keeps it server-side, in C++**, and handles the browser side
  for you.

Next: [HTML, CSS & JavaScript](01-html-css-js.md) — the three languages of the
browser, what each does, and how waya replaces all three with C++.
