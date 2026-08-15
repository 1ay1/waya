#pragma once
/// \file effect.hpp
/// Effects and subscriptions for the surface runtime — maya's `Cmd`/`Sub`,
/// transposed onto the browser. This is what turns a pure `update` into a real
/// app: `update` returns a `Cmd` *describing* side effects, and the runtime
/// interprets them (timers, navigation, async fetch) and feeds the results back
/// as messages. `subscribe` declares standing event sources (intervals, so
/// live/real-time UIs work) as a function of the model.
///
///   static std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
///       if (msg == Tick)  return { m.advance(), Cmd<Msg>::after(1000ms, Tick) };  // clock
///       if (msg == Load)  return { m, Cmd<Msg>::fetch("/data.json", Loaded) };    // async
///       if (msg == Go)    return { m, Cmd<Msg>::navigate("/next") };              // route
///       return { m, Cmd<Msg>::none() };
///   }
///
///   static Sub<Msg> subscribe(const Model& m) {
///       return m.live ? Sub<Msg>::every(1000ms, Tick) : Sub<Msg>::none();
///   }
///
/// The type-theoretic insight (maya's words): a pure function cannot perform
/// I/O, but it CAN return a *description* of I/O to perform later. That keeps
/// `update` pure — same inputs, same outputs, testable with `==`, no server.
///
/// A Program may return just `Model` (no effects) OR `(Model, Cmd)` — the
/// runtime supports both, so simple apps stay simple.
///
/// Surface `Msg` is whatever the app chooses (typically `int`, an int enum, or
/// a small variant); it rides the WebSocket as an integer tap id. `Cmd`/`Sub`
/// carry `Msg` values and the runtime converts them at the wire boundary.

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <any>
#include <vector>

namespace waya::surface {

using namespace std::chrono_literals;

/// A message-with-value the runtime delivers to `update`: `msg` is the app's
/// message as an integer tap id; `value` is an optional string payload (input
/// value, fetch body, route path, broadcast payload). `topic` is set only for a
/// broadcast delivery, so the owner loop can find the matching on_topic handler.
/// A unit of work for the owner loop. Either:
///   • a WIRE message from the browser: `token` (looked up in the msg registry)
///     + `value` (an input's text, a dropped payload, a pressed key), or
///   • an EFFECT-produced message: `msg` holds the already-typed Msg (std::any),
///     no wire round-trip needed (Cmd::emit/after/task/fetch, broadcast).
/// `topic` is set only for a broadcast delivery. `route` marks a route change.
struct Deliver {
    int token = 0;              // wire token (0 = none / not a wire msg)
    std::string value;          // event value / route path / broadcast payload
    std::string topic;          // set for broadcast deliveries
    std::any msg;               // resolved typed Msg for effect-produced deliveries
    bool is_route = false;      // this is a route change (value = path)
    bool is_env = false;        // display report (value = "w|h|dark|tz")
    bool is_sync = false;       // tab became visible again: repaint if dirty
    bool is_storage = false;    // persisted value restored on connect (value = "key|val")
    bool has_msg() const { return msg.has_value(); }
};

/// What the display reported about itself — the browser's answer to a
/// terminal's (rows, cols) + SIGWINCH. Delivered through `Sub::on_viewport`
/// on connect, on every (debounced) resize, and when the user flips their
/// OS colour scheme. `tz` is the IANA zone ("America/New_York") so the server
/// can render local times without asking.
struct Viewport {
    int width  = 0;             ///< innerWidth, CSS px
    int height = 0;             ///< innerHeight, CSS px
    bool dark  = false;         ///< prefers-color-scheme: dark
    std::string tz;             ///< IANA timezone, may be empty
    bool operator==(const Viewport&) const = default;

    /// The standard breakpoints, model-side: same thresholds as the CSS-side
    /// `at(Md,…)`/`below(Md,…)` mods, so both views of "phone" agree.
    bool phone()   const { return width > 0 && width < 768; }
    bool tablet()  const { return width >= 768 && width < 1024; }
    bool desktop() const { return width >= 1024; }
};

// ── overload helper (same trick maya uses for std::visit) ───────────────────
template <class... Ts> struct overload : Ts... { using Ts::operator()...; };
template <class... Ts> overload(Ts...) -> overload<Ts...>;

// ============================================================================
// Cmd<Msg> — a description of side effects to perform.
// ============================================================================
// A sum type: exactly one alternative. The runtime pattern-matches and performs
// the effect; user code never performs an effect directly — it returns a Cmd.
// Alternatives split into substrate-neutral (none/quit/batch/after/emit/task)
// and web effects (navigate/push_url/fetch).
template <typename Msg>
class Cmd {
public:
    // ── Alternatives (each describes one effect) ────────────────────────────
    struct None {};
    struct Quit {};
    struct Batch { std::vector<Cmd> cmds; };
    /// Deliver `msg` after `delay` (clocks, debounce, retry backoff).
    struct After { std::chrono::milliseconds delay; Msg msg; };
    /// Deliver `msg` immediately, next tick (chain reducers without a view pass
    /// in between — self-messaging).
    struct Emit  { Msg msg; };
    /// Run `work` off the render path; whatever Msg it produces is delivered.
    struct Task  { std::function<Msg()> work; };
    // ── Web effects ─────────────────────────────────────────────────────────
    /// Client-side navigation: change the URL and route to it. `replace` uses
    /// history.replaceState instead of pushState (no back-button entry).
    struct Navigate { std::string url; bool replace = false; };
    /// history.pushState the URL without triggering a route (deep-link sync).
    struct PushUrl  { std::string url; };
    /// The outcome of a `Fetch`, delivered to `on_response`. Lets an app tell a
    /// 404/500/timeout apart from a legitimately-empty 200 — `status == 0` means
    /// the request never completed (DNS/connect/timeout/TLS-not-built). `ok()`
    /// mirrors http::Response::ok() (2xx). This is the honest, no-silent-drop
    /// path; the string `on_done` overload stays as sugar for the happy case.
    struct Response {
        int status = 0;
        std::string body;
        std::vector<std::pair<std::string,std::string>> headers;
        bool ok() const { return status >= 200 && status < 300; }
        std::string header(std::string_view k) const {
            for (auto& [hk, hv] : headers) {
                if (hk.size() != k.size()) continue;
                bool eq = true;
                for (std::size_t i = 0; i < k.size(); ++i)
                    if ((hk[i]|32) != (k[i]|32)) { eq = false; break; }
                if (eq) return hv;
            }
            return {};
        }
    };
    /// An HTTP request. Exactly ONE of `on_done` (body only) / `on_response`
    /// (full status+headers+body) is set. GET by default; set method/headers/
    /// body for POST/PUT/auth'd JSON APIs. Runs on a worker thread. https://
    /// requires the runtime built with -DWAYA_TLS.
    struct Fetch    {
        std::string url;
        std::function<Msg(std::string)> on_done;
        std::string method = "GET";
        std::vector<std::pair<std::string,std::string>> headers;
        std::string body;
        std::function<Msg(Response)> on_response;   // richer: set instead of on_done
    };
    /// Publish `payload` to everyone subscribed to `topic` (incl. self). The
    /// sender doesn't know or care who's listening — each receiver's
    /// `Sub::on_topic(topic, fn)` turns the payload into ITS own Msg. This is
    /// how independent sessions share state (chat, presence, collaborative UIs).
    struct Broadcast { std::string topic; std::string payload; };
    /// Set the document title live (`document.title = text`) — unread counts,
    /// per-screen names — without waiting for a fresh SSR of Meta.
    struct SetTitle { std::string text; };
    /// Scroll the page: `target` is an `anchor("…")`/`name("…")` on a node, or
    /// the specials "top"/"bottom" for the whole page (chat scroll-to-bottom).
    struct ScrollTo { std::string target; bool smooth = true; };
    /// Move keyboard focus to the named control — or, with `off`, blur whatever
    /// currently holds it (open-dialog→focus-field, Esc→release).
    struct Focus { std::string target; bool off = false; };
    /// Write `text` to the user's clipboard ("Copy link" buttons).
    struct Copy { std::string text; };
    /// Offer `data` (raw bytes) as a browser file download named `filename`
    /// (export CSV/JSON/reports straight from the Model).
    struct Download { std::string filename; std::string mime; std::string data; };
    /// Persist `value` under `key` in the browser's localStorage (survives a
    /// reload / tab close). Empty value with `clear=true` removes the key.
    /// Read it back on connect via `Sub::on_storage`.
    struct Store { std::string key; std::string value; bool clear = false; };

    using Alt = std::variant<None, Quit, Batch, After, Emit, Task,
                             Navigate, PushUrl, Fetch, Broadcast,
                             SetTitle, ScrollTo, Focus, Copy, Download, Store>;

    Cmd() : alt_(std::make_shared<Alt>(None{})) {}
    explicit Cmd(Alt a) : alt_(std::make_shared<Alt>(std::move(a))) {}

    // ── Factories (read like maya) ──────────────────────────────────────────
    static Cmd none() { return Cmd(Alt{None{}}); }
    static Cmd quit() { return Cmd(Alt{Quit{}}); }
    static Cmd after(std::chrono::milliseconds d, Msg m) { return Cmd(Alt{After{d, std::move(m)}}); }
    static Cmd after(long ms, Msg m) { return after(std::chrono::milliseconds{ms}, std::move(m)); }
    static Cmd emit(Msg m) { return Cmd(Alt{Emit{std::move(m)}}); }
    static Cmd task(std::function<Msg()> w) { return Cmd(Alt{Task{std::move(w)}}); }
    static Cmd navigate(std::string url, bool replace = false) {
        return Cmd(Alt{Navigate{std::move(url), replace}});
    }
    static Cmd push_url(std::string url) { return Cmd(Alt{PushUrl{std::move(url)}}); }
    static Cmd fetch(std::string url, std::function<Msg(std::string)> on_done) {
        return Cmd(Alt{Fetch{std::move(url), std::move(on_done)}});
    }
    /// POST `body` (default JSON) to `url`; the response body becomes a Msg.
    static Cmd post(std::string url, std::string body, std::function<Msg(std::string)> on_done,
                    std::string content_type = "application/json") {
        return Cmd(Alt{Fetch{std::move(url), std::move(on_done), "POST",
                             {{"Content-Type", std::move(content_type)}}, std::move(body)}});
    }
    /// A full request: any method, headers (auth, etc.), and body.
    static Cmd http(std::string method, std::string url,
                    std::vector<std::pair<std::string,std::string>> headers,
                    std::string body, std::function<Msg(std::string)> on_done) {
        return Cmd(Alt{Fetch{std::move(url), std::move(on_done), std::move(method),
                             std::move(headers), std::move(body)}});
    }
    /// `fetch_full(url, on)` — like fetch, but `on` receives the full Response
    /// (status, headers, body) so you can branch on 404/500/timeout instead of
    /// getting an empty string you can't distinguish from a real empty 200.
    static Cmd fetch_full(std::string url, std::function<Msg(Response)> on) {
        return Cmd(Alt{Fetch{std::move(url), {}, "GET", {}, {}, std::move(on)}});
    }
    /// `post_full(url, body, on)` — POST with a full-Response callback.
    static Cmd post_full(std::string url, std::string body, std::function<Msg(Response)> on,
                         std::string content_type = "application/json") {
        return Cmd(Alt{Fetch{std::move(url), {}, "POST",
                             {{"Content-Type", std::move(content_type)}}, std::move(body), std::move(on)}});
    }
    /// `http_full(method, url, headers, body, on)` — any request, full Response.
    static Cmd http_full(std::string method, std::string url,
                         std::vector<std::pair<std::string,std::string>> headers,
                         std::string body, std::function<Msg(Response)> on) {
        return Cmd(Alt{Fetch{std::move(url), {}, std::move(method),
                             std::move(headers), std::move(body), std::move(on)}});
    }
    /// Publish to a topic. Every session subscribed via Sub::on_topic(topic,..)
    /// — this one included — receives `payload` and maps it to its own Msg.
    static Cmd broadcast(std::string topic, std::string payload = {}) {
        return Cmd(Alt{Broadcast{std::move(topic), std::move(payload)}});
    }
    /// `set_title("(3) Inbox")` — set the live document title.
    static Cmd set_title(std::string text) { return Cmd(Alt{SetTitle{std::move(text)}}); }
    /// `scroll_to("chat-end")` / `scroll_to("bottom", false)` — scroll the named
    /// `anchor(..)`/`name(..)` node into view, or "top"/"bottom" of the page.
    static Cmd scroll_to(std::string target, bool smooth = true) {
        return Cmd(Alt{ScrollTo{std::move(target), smooth}});
    }
    /// `focus("email")` — focus the control named/anchored `email`.
    static Cmd focus(std::string target) { return Cmd(Alt{Focus{std::move(target), false}}); }
    /// `blur()` — drop focus from whatever currently holds it.
    static Cmd blur() { return Cmd(Alt{Focus{{}, true}}); }
    /// `copy(text)` — put `text` on the user's clipboard.
    static Cmd copy(std::string text) { return Cmd(Alt{Copy{std::move(text)}}); }
    /// `download("report.csv", bytes, "text/csv")` — offer a file download.
    static Cmd download(std::string filename, std::string data,
                        std::string mime = "application/octet-stream") {
        return Cmd(Alt{Download{std::move(filename), std::move(mime), std::move(data)}});
    }
    /// `store("theme", "dark")` — persist a value in localStorage across reloads.
    /// Read it back on connect with `Sub::on_storage`.
    static Cmd store(std::string key, std::string value) {
        return Cmd(Alt{Store{std::move(key), std::move(value), false}});
    }
    /// `store_clear("theme")` — remove a persisted key.
    static Cmd store_clear(std::string key) {
        return Cmd(Alt{Store{std::move(key), {}, true}});
    }

    /// Combine multiple Cmds. Flattens nested batches and strips Nones — so a
    /// `batch` of one real effect is that effect, and a `batch` of nothing is
    /// `none`. Same normalizing behavior as maya.
    static Cmd batch(std::vector<Cmd> cs) {
        std::vector<Cmd> flat;
        flat.reserve(cs.size());
        for (auto& c : cs) {
            if (std::holds_alternative<None>(*c.alt_)) continue;
            if (auto* b = std::get_if<Batch>(c.alt_.get()))
                for (auto& inner : b->cmds) flat.push_back(std::move(inner));
            else flat.push_back(std::move(c));
        }
        if (flat.empty()) return none();
        if (flat.size() == 1) return std::move(flat[0]);
        return Cmd(Alt{Batch{std::move(flat)}});
    }
    template <typename... Cs>
        requires (sizeof...(Cs) > 1 && (std::same_as<std::remove_cvref_t<Cs>, Cmd> && ...))
    static Cmd batch(Cs&&... cs) {
        std::vector<Cmd> v; v.reserve(sizeof...(Cs));
        (v.push_back(std::forward<Cs>(cs)), ...);
        return batch(std::move(v));
    }

    [[nodiscard]] const Alt& alt() const { return *alt_; }
    [[nodiscard]] bool is_none() const noexcept { return std::holds_alternative<None>(*alt_); }
    [[nodiscard]] bool is_quit() const noexcept { return std::holds_alternative<Quit>(*alt_); }

    // ── Functor map — embed a child's Cmd into a parent's Msg type ───────────
    template <std::invocable<Msg> F>
    [[nodiscard]] auto map(F&& f) const -> Cmd<std::invoke_result_t<F, Msg>> {
        using B = std::invoke_result_t<F, Msg>;
        return std::visit(overload{
            [](const None&) -> Cmd<B> { return Cmd<B>::none(); },
            [](const Quit&) -> Cmd<B> { return Cmd<B>::quit(); },
            [&](const After& a) -> Cmd<B> { return Cmd<B>::after(a.delay, f(a.msg)); },
            [&](const Emit&  e) -> Cmd<B> { return Cmd<B>::emit(f(e.msg)); },
            [](const Navigate& n) -> Cmd<B> { return Cmd<B>::navigate(n.url, n.replace); },
            [](const PushUrl&  p) -> Cmd<B> { return Cmd<B>::push_url(p.url); },
            [](const Broadcast& b) -> Cmd<B> { return Cmd<B>::broadcast(b.topic, b.payload); },
            [](const SetTitle& t) -> Cmd<B> { return Cmd<B>::set_title(t.text); },
            [](const ScrollTo& sc) -> Cmd<B> { return Cmd<B>::scroll_to(sc.target, sc.smooth); },
            [](const Focus& fo) -> Cmd<B> { return fo.off ? Cmd<B>::blur() : Cmd<B>::focus(fo.target); },
            [](const Copy& c) -> Cmd<B> { return Cmd<B>::copy(c.text); },
            [](const Download& d) -> Cmd<B> { return Cmd<B>::download(d.filename, d.data, d.mime); },
            [](const Store& st) -> Cmd<B> { return st.clear ? Cmd<B>::store_clear(st.key) : Cmd<B>::store(st.key, st.value); },
            [&](const Batch& b) -> Cmd<B> {
                std::vector<Cmd<B>> mapped; mapped.reserve(b.cmds.size());
                for (auto& c : b.cmds) mapped.push_back(c.map(f));
                return Cmd<B>::batch(std::move(mapped));
            },
            [&](const Task& t) -> Cmd<B> {
                return Cmd<B>::task([w = t.work, mapper = std::forward<F>(f)]() -> B {
                    return mapper(w());
                });
            },
            [&](const Fetch& ft) -> Cmd<B> {
                if (ft.on_response) {
                    return Cmd<B>::http_full(ft.method, ft.url, ft.headers, ft.body,
                        [on = ft.on_response, mapper = f](typename Cmd<B>::Response r) -> B {
                            // translate our Response into the child's Response shape
                            typename Cmd<Msg>::Response cr{r.status, std::move(r.body), std::move(r.headers)};
                            return mapper(on(std::move(cr)));
                        });
                }
                return Cmd<B>::http(ft.method, ft.url, ft.headers, ft.body,
                    [on = ft.on_done, mapper = f](std::string body) -> B {
                        return mapper(on(std::move(body)));
                    });
            },
        }, *alt_);
    }

    /// Equality — makes `update` testable. Value-carrying alternatives compare
    /// their values; those that carry callables compare only the discriminant
    /// (functions aren't comparable). Msg is compared when it's comparable.
    bool operator==(const Cmd& o) const {
        if (alt_->index() != o.alt_->index()) return false;
        return std::visit([&](const auto& a) -> bool {
            using A = std::decay_t<decltype(a)>;
            const auto& b = std::get<A>(*o.alt_);
            if constexpr (std::is_same_v<A, None> || std::is_same_v<A, Quit>) return true;
            else if constexpr (std::is_same_v<A, After>) {
                if (a.delay != b.delay) return false;
                if constexpr (std::equality_comparable<Msg>) return a.msg == b.msg; else return true;
            }
            else if constexpr (std::is_same_v<A, Emit>) {
                if constexpr (std::equality_comparable<Msg>) return a.msg == b.msg; else return true;
            }
            else if constexpr (std::is_same_v<A, Navigate>) return a.url == b.url && a.replace == b.replace;
            else if constexpr (std::is_same_v<A, PushUrl>)  return a.url == b.url;
            else if constexpr (std::is_same_v<A, Broadcast>) return a.topic == b.topic && a.payload == b.payload;
            else if constexpr (std::is_same_v<A, SetTitle>) return a.text == b.text;
            else if constexpr (std::is_same_v<A, ScrollTo>) return a.target == b.target && a.smooth == b.smooth;
            else if constexpr (std::is_same_v<A, Focus>)    return a.target == b.target && a.off == b.off;
            else if constexpr (std::is_same_v<A, Copy>)     return a.text == b.text;
            else if constexpr (std::is_same_v<A, Download>) return a.filename == b.filename && a.mime == b.mime && a.data == b.data;
            else if constexpr (std::is_same_v<A, Store>)    return a.key == b.key && a.value == b.value && a.clear == b.clear;
            else if constexpr (std::is_same_v<A, Batch>) {
                if (a.cmds.size() != b.cmds.size()) return false;
                for (std::size_t i = 0; i < a.cmds.size(); ++i)
                    if (!(a.cmds[i] == b.cmds[i])) return false;
                return true;
            }
            else return true;  // callables (Task/Fetch): discriminant match is enough
        }, *alt_);
    }

private:
    std::shared_ptr<Alt> alt_;   // shared_ptr keeps Cmd cheaply copyable
};

// ============================================================================
// Sub<Msg> — declarative event subscriptions (the input side).
// ============================================================================
//   Cmd<Msg>  effects going OUT  (timers, navigation, fetch)
//   Sub<Msg>  events coming IN   (interval ticks, route changes)
// Both are data. The runtime diffs Subs between frames, starting/stopping
// listeners as the model changes — the foundation for clocks and live polling.
template <typename Msg>
class Sub {
public:
    struct None {};
    struct Batch { std::vector<Sub> subs; };
    /// Emit `msg` every `interval` (animations, polling, live data).
    struct Every { std::chrono::milliseconds interval; Msg msg; };
    /// Map the current URL path into a Msg on every route change.
    struct OnRoute { std::function<Msg(std::string)> route; };
    /// Map the display's self-report (size, colour scheme, timezone) into a Msg
    /// — on connect, on resize (debounced), and on scheme flips. The browser
    /// analogue of handling SIGWINCH: layout DECISIONS (not just styling) can
    /// live in the Model — collapse a sidebar into a drawer, page a table by
    /// what fits, pick a chart's point density.
    struct OnViewport { std::function<Msg(Viewport)> on; };
    /// Restore a persisted value on connect: for each `waya:`-key the browser has
    /// in localStorage, `on(key, value)` becomes a Msg. Pair with `Cmd::store` to
    /// round-trip state across reloads (theme, collapsed panels, a draft).
    struct OnStorage { std::function<Msg(std::string, std::string)> on; };
    /// Join a pub/sub `topic`; each broadcast `payload` becomes a Msg via `on`.
    /// The runtime registers/unregisters this session as the subscription set
    /// changes, so joining or leaving a room is just a model-driven Sub.
    struct OnTopic { std::string topic; std::function<Msg(std::string)> on; };

    using Alt = std::variant<None, Batch, Every, OnRoute, OnViewport, OnStorage, OnTopic>;

    Sub() : alt_(std::make_shared<Alt>(None{})) {}
    explicit Sub(Alt a) : alt_(std::make_shared<Alt>(std::move(a))) {}

    static Sub none() { return Sub{}; }
    static Sub every(std::chrono::milliseconds interval, Msg msg) { return Sub(Alt{Every{interval, std::move(msg)}}); }
    static Sub every(long ms, Msg msg) { return every(std::chrono::milliseconds{ms}, std::move(msg)); }
    static Sub on_route(std::function<Msg(std::string)> f) { return Sub(Alt{OnRoute{std::move(f)}}); }
    static Sub on_viewport(std::function<Msg(Viewport)> f) { return Sub(Alt{OnViewport{std::move(f)}}); }
    static Sub on_storage(std::function<Msg(std::string, std::string)> f) { return Sub(Alt{OnStorage{std::move(f)}}); }
    static Sub on_topic(std::string topic, std::function<Msg(std::string)> on) {
        return Sub(Alt{OnTopic{std::move(topic), std::move(on)}});
    }

    static Sub batch(std::vector<Sub> subs) {
        std::vector<Sub> flat;
        flat.reserve(subs.size());
        for (auto& s : subs) {
            if (std::holds_alternative<None>(*s.alt_)) continue;
            if (auto* b = std::get_if<Batch>(s.alt_.get()))
                for (auto& inner : b->subs) flat.push_back(std::move(inner));
            else flat.push_back(std::move(s));
        }
        if (flat.empty()) return none();
        if (flat.size() == 1) return std::move(flat[0]);
        return Sub(Alt{Batch{std::move(flat)}});
    }
    template <typename... Ss>
        requires (sizeof...(Ss) > 1 && (std::same_as<std::remove_cvref_t<Ss>, Sub> && ...))
    static Sub batch(Ss&&... ss) {
        std::vector<Sub> v; v.reserve(sizeof...(Ss));
        (v.push_back(std::forward<Ss>(ss)), ...);
        return batch(std::move(v));
    }

    [[nodiscard]] const Alt& alt() const { return *alt_; }
    [[nodiscard]] bool is_none() const noexcept { return std::holds_alternative<None>(*alt_); }

    /// Collect every interval timer this Sub declares (flattening batches). The
    /// runtime reconciles these against the ones already running each frame.
    [[nodiscard]] std::vector<Every> timers() const {
        std::vector<Every> out; collect_timers(out); return out;
    }
    /// The route handler, if any (last one wins in a batch).
    [[nodiscard]] const OnRoute* route() const {
        const OnRoute* r = nullptr; collect_route(r); return r;
    }
    /// The viewport handler, if any (last one wins in a batch).
    [[nodiscard]] const OnViewport* viewport() const {
        const OnViewport* v = nullptr; collect_viewport(v); return v;
    }
    /// The storage-restore handler, if any (last one wins in a batch).
    [[nodiscard]] const OnStorage* storage() const {
        const OnStorage* s = nullptr; collect_storage(s); return s;
    }
    /// Every topic subscription this Sub declares (flattening batches). The
    /// runtime reconciles these against the topics the session is joined to.
    [[nodiscard]] std::vector<const OnTopic*> topics() const {
        std::vector<const OnTopic*> out; collect_topics(out); return out;
    }

    template <std::invocable<Msg> F>
    [[nodiscard]] auto map(F&& f) const -> Sub<std::invoke_result_t<F, Msg>> {
        using B = std::invoke_result_t<F, Msg>;
        return std::visit(overload{
            [](const None&) -> Sub<B> { return Sub<B>::none(); },
            [&](const Every& e) -> Sub<B> { return Sub<B>::every(e.interval, f(e.msg)); },
            [&](const OnRoute& r) -> Sub<B> {
                return Sub<B>::on_route([rt = r.route, mapper = std::forward<F>(f)](std::string p) -> B {
                    return mapper(rt(std::move(p)));
                });
            },
            [&](const OnViewport& v) -> Sub<B> {
                return Sub<B>::on_viewport([on = v.on, mapper = std::forward<F>(f)](Viewport vp) -> B {
                    return mapper(on(std::move(vp)));
                });
            },
            [&](const OnStorage& s) -> Sub<B> {
                return Sub<B>::on_storage([on = s.on, mapper = std::forward<F>(f)](std::string k, std::string v) -> B {
                    return mapper(on(std::move(k), std::move(v)));
                });
            },
            [&](const OnTopic& t) -> Sub<B> {
                return Sub<B>::on_topic(t.topic, [on = t.on, mapper = std::forward<F>(f)](std::string p) -> B {
                    return mapper(on(std::move(p)));
                });
            },
            [&](const Batch& b) -> Sub<B> {
                std::vector<Sub<B>> mapped; mapped.reserve(b.subs.size());
                for (auto& s : b.subs) mapped.push_back(s.map(f));
                return Sub<B>::batch(std::move(mapped));
            },
        }, *alt_);
    }

private:
    void collect_timers(std::vector<Every>& out) const {
        std::visit(overload{
            [&](const Every& e) { out.push_back(e); },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_timers(out); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_route(const OnRoute*& r) const {
        std::visit(overload{
            [&](const OnRoute& o) { r = &o; },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_route(r); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_viewport(const OnViewport*& v) const {
        std::visit(overload{
            [&](const OnViewport& o) { v = &o; },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_viewport(v); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_storage(const OnStorage*& s) const {
        std::visit(overload{
            [&](const OnStorage& o) { s = &o; },
            [&](const Batch& b) { for (auto& sub : b.subs) sub.collect_storage(s); },
            [](const auto&) {},
        }, *alt_);
    }
    void collect_topics(std::vector<const OnTopic*>& out) const {
        std::visit(overload{
            [&](const OnTopic& o) { out.push_back(&o); },
            [&](const Batch& b) { for (auto& s : b.subs) s.collect_topics(out); },
            [](const auto&) {},
        }, *alt_);
    }
    std::shared_ptr<Alt> alt_;
};

} // namespace waya::surface
