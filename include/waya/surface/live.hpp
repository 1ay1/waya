#pragma once
/// \file live.hpp
/// Run a Surface app live in the browser over a WebSocket. A Surface Program is
/// the Elm shape you already know, but `view` returns a Surface instead of DOM:
///
///   struct App {
///       struct Model { int n = 0; };
///       using Msg = int;                       // tap messages
///       static Model init();
///       static Model update(Model, Msg);
///       static NodeRef view(const Model&);     // returns a surface
///   };
///   waya::surface::live<App>({.port = 8080});
///
/// The user describes the surface; waya renders it (DOM backend here), streams
/// only the diff on each tap, and the client applies it. No HTML, CSS, or event
/// wiring in the app code.

#include "node.hpp"
#include "sugar.hpp"     // col_/row_/push/screens/color — batteries-included
#include "complete.hpp"  // browser-parity mods (flex/grid/transform/scroll/text/…)
#include "forms.hpp"     // every native input type + fieldset/datalist/progress
#include "media.hpp"     // capability elements: video/audio opts, embed, svg, canvas
#include "layout.hpp"
#include "dom.hpp"
#include "diff.hpp"
#include "wire.hpp"
#include "binary.hpp"
#include "effect.hpp"
#include "meta.hpp"
#include "assets.hpp"
#include "validate.hpp"
#include "component.hpp"
#include "client.hpp"
#include "program.hpp"
#include "http_util.hpp"  // non-templated HTTP/socket helpers (compiled into waya_runtime)

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <csignal>
#include <cerrno>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// SIGPIPE avoidance is platform-specific. Linux offers the MSG_NOSIGNAL send
// flag; macOS/BSD historically don't (they use the SO_NOSIGPIPE socket option
// and/or ignoring SIGPIPE). We ignore SIGPIPE globally in live() AND pass this
// flag where available, so a client vanishing mid-send never kills the process.
#ifndef MSG_NOSIGNAL
#define WAYA_MSG_NOSIGNAL 0
#else
#define WAYA_MSG_NOSIGNAL MSG_NOSIGNAL
#endif

#ifdef WAYA_GZIP
#include <zlib.h>
#endif

// Reuse the WebSocket codec from the DOM live runtime.
#include "../net/ws.hpp"
#include "../net/http.hpp"
// Non-templated runtime classes (Session/Hub/Pool/SessionStore) and the
// net/http helpers, compiled once into waya_runtime.
#include "http_util.hpp"
#include "session.hpp"
#include "runtime.hpp"

namespace waya::surface {

/// Runtime config. `host` defaults to 0.0.0.0 — the app listens on ALL network
/// interfaces, so other devices on your LAN (a phone, another laptop) can reach
/// it at http://<this-machine-ip>:<port>/, not just localhost. Set host to
/// "127.0.0.1" (or WAYA_HOST=127.0.0.1) to bind loopback-only.
///
/// `page_bg` is the color painted on html/body behind the app — set it to your
/// app root's background so overscroll bounce, safe-area insets and the pre-paint
/// flash all match (default: a dark slate). Also drives the mobile theme-color.
struct LiveConfig { int port = 8080; const char* host = "0.0.0.0"; bool open = true; std::uint32_t page_bg = 0x0b1020; const char* title = "waya"; };

/// A Surface Program: Model + Msg + init/update/view(->NodeRef). `update` may
/// be `update(Model, Msg)` (taps) OR `update(Model, Msg, std::string value)`
/// (inputs carry a value) — the runtime calls whichever you define.
///
/// Surface `Msg` must be an integer or an integer-backed enum: taps travel over
/// the WebSocket as integers, so the runtime converts Msg <-> int at the wire.
/// (Use a `std::variant` Msg with the DOM `waya::app` runtime, not this one.)
/// Surface `Msg` is the Program's own type — typically a `std::variant` of message
/// structs (maya/Elm), carrying payloads and matched with std::visit. The runtime
/// registers each wired Msg and maps it to an opaque wire token internally, so
/// the app is fully type-safe; you never write an int message id.
/// The Program concept + hooks live in surface/program.hpp (the transport-free
/// "ideas" half); this file is the serving runtime that drives them.

namespace detail {

/// The Program hooks — dispatch/init_of/subs_of/meta_of (which shape of the
/// app's update/init/subscribe/meta it uses) — live in surface/program.hpp.
/// Below are the runtime-only helpers: error boundaries, the build id, etc.

/// Render P::view(model) with an ERROR BOUNDARY: if the app's view throws, we
/// return an error card node instead of letting the exception unwind into the
/// detached thread (which would std::terminate the whole process). Keeps the
/// server and every other session alive.
template <typename P, typename Model>
NodeRef safe_view(const Model& m){
    try {
        NodeRef r = P::view(m);
#if defined(WAYA_STRICT)
        // Strict builds REFUSE to render a structurally invalid surface: the
        // guarantee is literal, not advisory. assert_valid prints every
        // violation and aborts, so a bad tree never reaches diff/wire.
        if (r) assert_valid(r);
#elif !defined(NDEBUG)
        // Debug builds catch malformed trees LOUDLY on first render (WHATWG
        // content-model violations: unnamed form controls, nested interactive
        // nodes, void elements with children, duplicate keys, missing alt).
        // Release builds skip the walk entirely — zero cost in production.
        if (r) { auto vs = check(*r); for (auto& v : vs) std::fprintf(stderr, "waya: %s\n", v.message().c_str()); }
#endif
        return r;
    }
    catch (const std::exception& e) { return markup(error_html(e.what())); }
    catch (...) { return markup(error_html("unknown error in view()")); }
}

/// Dispatch with an error boundary: a throwing update() leaves the model
/// unchanged and emits no effect, rather than taking down the session.
template <typename P, typename Model, typename Msg>
std::pair<Model, Cmd<Msg>> safe_dispatch(Model m, Msg msg, const std::string& value, bool& ok){
    ok = true;
    try { return dispatch<P>(std::move(m), msg, value); }
    catch (...) { ok = false; return { std::move(m), Cmd<Msg>::none() }; }
}

/// The configured WebSocket Origin allowlist, or empty = allow all. Sources, in
/// order: a static `P::allowed_origins()` (returns a container of strings), else
/// the `WAYA_ALLOWED_ORIGINS` env (comma-separated). Cached after first read.
template <typename P>
const std::vector<std::string>& allowed_origins_list(){
    static const std::vector<std::string> list = []{
        std::vector<std::string> out;
        if constexpr (requires { P::allowed_origins(); }) {
            for (auto& o : P::allowed_origins()) out.emplace_back(o);
        } else if (const char* env = std::getenv("WAYA_ALLOWED_ORIGINS")) {
            std::string s = env, cur;
            for (char c : s){ if (c==','){ if(!cur.empty()) out.push_back(cur); cur.clear(); }
                              else if (c!=' ') cur += c; }
            if (!cur.empty()) out.push_back(cur);
        }
        return out;
    }();
    return list;
}

/// True if this handshake's Origin is permitted. Empty allowlist = allow all
/// (dev default). A missing Origin header is allowed (native/non-browser client);
/// browsers always send one, and a cross-site page can't forge it.
template <typename P>
bool origin_allowed(std::string_view raw){
    const auto& list = allowed_origins_list<P>();
    if (list.empty()) return true;
    // extract the Origin header value (case-insensitive header name).
    std::string origin;
    std::size_t pos = 0;
    while (pos < raw.size()){
        std::size_t eol = raw.find("\r\n", pos);
        if (eol == std::string_view::npos) eol = raw.size();
        std::string_view line = raw.substr(pos, eol - pos);
        if (line.size() > 7){
            std::string name; for (std::size_t i=0;i<6 && i<line.size();++i){ char c=line[i]; name += (c>='A'&&c<='Z')?char(c+32):c; }
            if (name == "origin"){
                std::size_t c = line.find(':');
                if (c != std::string_view::npos){
                    std::string_view v = line.substr(c+1);
                    while (!v.empty() && v.front()==' ') v.remove_prefix(1);
                    origin = std::string(v);
                }
                break;
            }
        }
        if (eol == raw.size()) break;
        pos = eol + 2;
    }
    if (origin.empty()) return true;   // non-browser client (no Origin) — not a CSRF vector
    for (auto& a : list) if (origin == a) return true;
    return false;
}

/// The terminal is the browser client (surface/client.hpp) — it holds no app
/// state or logic, just decodes binary frames and paints them. Kept in its own
/// file so the transport/runtime here stays free of the ~6 KB JS blob.

/// Interpret one Cmd. Effects that produce a message push it back into the
/// session queue (self-messaging); web effects send a control frame. This is
/// the runtime half of "effects are data" — the app returned a description, we
/// perform it here and nowhere else.
template <typename Msg>
void perform(const std::shared_ptr<Session>& s, const Cmd<Msg>& cmd) {
    std::visit(overload{
        [](const typename Cmd<Msg>::None&) {},
        [&](const typename Cmd<Msg>::Quit&) { s->quit_now(); },
        [&](const typename Cmd<Msg>::Batch& b) { for (auto& c : b.cmds) perform(s, c); },
        [&](const typename Cmd<Msg>::Emit& e) { s->push_msg(std::any{e.msg}); },
        [&](const typename Cmd<Msg>::After& a) {
            std::any m = a.msg; long ms = a.delay.count();
            std::weak_ptr<Session> ws_ = s;
            // One heap entry on the shared Scheduler — not a fresh sleeping thread
            // per delayed message (a Cmd::after on every keystroke would spawn a
            // thread per keystroke). The push runs on the Pool when due.
            detail::Scheduler::instance().after(ms, [ws_, m = std::move(m)]{
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(m);
            });
        },
        [&](const typename Cmd<Msg>::Task& t) {
            auto work = t.work; std::weak_ptr<Session> ws_ = s;
            Pool::instance().submit([ws_, work]{
                Msg r = work();
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(std::any{r});
            });
        },
        [&](const typename Cmd<Msg>::Fetch& f) {
            auto req = f; std::weak_ptr<Session> ws_ = s;
            Pool::instance().submit([ws_, req]{
                http::Response rr = http::request({
                    .method = req.method, .url = req.url,
                    .headers = req.headers, .body = req.body });
                // on_response gets the honest, full outcome (status/headers/body
                // — status 0 == the request never completed); on_done is the
                // body-only sugar. Exactly one is set.
                Msg r = req.on_response
                    ? req.on_response(typename Cmd<Msg>::Response{
                          rr.status, std::move(rr.body), std::move(rr.headers) })
                    : req.on_done(std::move(rr.body));
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(std::any{r});
            });
        },
        [&](const typename Cmd<Msg>::Navigate& n) {
            s->send_text((n.replace ? "@rep|" : "@nav|") + n.url);
        },
        [&](const typename Cmd<Msg>::PushUrl& p) { s->send_text("@url|" + p.url); },
        [&](const typename Cmd<Msg>::SetTitle& t) { s->send_text("@title|" + t.text); },
        [&](const typename Cmd<Msg>::ScrollTo& sc) {
            s->send_text(std::string("@scroll|") + (sc.smooth ? "1|" : "0|") + sc.target);
        },
        [&](const typename Cmd<Msg>::Focus& fo) {
            s->send_text(fo.off ? std::string("@blur|") : "@focus|" + fo.target);
        },
        [&](const typename Cmd<Msg>::Copy& c) { s->send_text("@copy|" + c.text); },
        [&](const typename Cmd<Msg>::Download& d) {
            // '|' is the frame separator for filename/mime (the payload itself is
            // base64, which never contains '|'); neutralise it in the two names.
            std::string fn = d.filename, mi = d.mime;
            for (char& ch : fn) if (ch=='|') ch = '_';
            for (char& ch : mi) if (ch=='|') ch = '_';
            s->send_text("@dl|" + fn + "|" + mi + "|" + ws::detail::base64(d.data));
        },
        [&](const typename Cmd<Msg>::Store& st) {
            if (st.clear) s->send_text("@storeclear|" + st.key);
            else {
                // key can't contain '|' (it's the field separator); the value
                // may, and everything after the first '|' is the value.
                std::string k = st.key; for (char& ch : k) if (ch=='|') ch='_';
                s->send_text("@store|" + k + "|" + st.value);
            }
        },
        [&](const typename Cmd<Msg>::Broadcast& b) {
            // Fan out to every session on the topic (this one included). Each
            // receiver maps the payload through its own Sub::on_topic.
            Hub::instance().publish(b.topic, b.payload);
        },
    }, cmd.alt());
}

/// Reconcile the model's declared subscriptions against the timers currently
/// running: start newly-declared intervals, stop ones no longer wanted. Idempotent
/// — safe to call after every update, like maya diffing Subs between frames.
template <typename Msg>
void reconcile_subs(const std::shared_ptr<Session>& s, const Sub<Msg>& sub) {
    auto wanted = sub.timers();
    // stable per-timer key = interval folded with the Msg token, so reconcile
    // matches the same declared timer across renders.
    auto keyof = [](const typename Sub<Msg>::Every& e){
        std::uint64_t alt = 0;
        if constexpr (requires { e.msg.index(); }) alt = e.msg.index();
        return (std::uint64_t)e.interval.count() * 1099511628211ull ^ (alt + 1);
    };

    // Fast path: fingerprint the declared subs (timer keys + topic names). If it
    // matches last frame's, the subscription set is unchanged and there is
    // nothing to reconcile — skip the timer diff AND the Hub's global-lock
    // set_topics entirely. A keystroke almost never changes what you subscribe
    // to, so this removes a mutex + N allocations from the steady-state loop.
    std::uint64_t fp = 1469598103934665603ull;
    auto mixfp = [&](std::uint64_t v){ fp ^= v; fp *= 1099511628211ull; };
    for (auto& e : wanted) mixfp(keyof(e));
    mixfp(0x9E3779B97F4A7C15ull);   // separator between timers and topics
    auto topic_ptrs = sub.topics();
    for (auto* t : topic_ptrs) for (char c : t->topic) mixfp((std::uint8_t)c);
    if (fp == 0) fp = 1;            // reserve 0 for "never reconciled"
    if (fp == s->sub_fingerprint) return;   // unchanged: nothing to do
    s->sub_fingerprint = fp;

    std::vector<Session::Timer> next;
    std::vector<bool> matched(wanted.size(), false);
    for (auto& t : s->timers) {
        bool keep = false;
        for (std::size_t i = 0; i < wanted.size(); ++i) {
            if (matched[i]) continue;
            if (keyof(wanted[i]) == t.key) { matched[i] = true; keep = true; break; }
        }
        if (keep) next.push_back(std::move(t));
        else *t.run = false;   // signal the interval thread to exit
    }
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        if (matched[i]) continue;
        long ms = wanted[i].interval.count();
        std::any m = wanted[i].msg;
        std::uint64_t key = keyof(wanted[i]);
        auto run = std::make_shared<std::atomic<bool>>(true);
        std::weak_ptr<Session> ws_ = s;
        // One heap entry on the shared Scheduler, cancelled via `run` — not a
        // dedicated sleeping thread per timer per session (5 timers x 1000
        // sessions used to be 5000 OS threads). `run` flips false on reconcile.
        detail::Scheduler::instance().every(ms, [ws_, m]{
            if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(m);
        }, run);
        next.push_back({ms, key, std::move(m), run});
    }
    s->timers = std::move(next);

    // Reconcile pub/sub topics: register the session for exactly the topics its
    // subscription currently declares (idempotent — joining/leaving a room is
    // just a model change that adds/removes an on_topic).
    std::vector<std::string> topics;
    for (auto* t : topic_ptrs) topics.push_back(t->topic);
    Hub::instance().set_topics(s, topics);
}

template <typename P>
bool serve_http(int conn, std::string_view req, int port,
                std::uint32_t page_bg, const char* page_title);

// The runtime's connection disposition, re-exported so the templated handlers
// (which live in this header) can name it without pulling more of runtime.hpp.
using RuntimeDisposition = Disposition;

template <typename P>
void run_ws_session(int conn, std::string req_str, int port,
                    std::uint32_t page_bg, const char* page_title);

// Handle ONE readiness event on `conn`: read+serve a single request (the gate
// already knows the socket is readable), then tell the gate what to do with the
// fd. `carry` carries pipelined bytes across keep-alive re-parks. A WebSocket
// upgrade spins the (long-lived, stateful) session onto its OWN thread and
// returns Owned so the bounded worker is freed immediately.
template <typename P>
RuntimeDisposition handle(int conn, std::string& carry, int port,
                          std::uint32_t page_bg = 0x0b1020, const char* page_title = "waya") {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;
    (void)sizeof(Model); (void)sizeof(Msg);

    RequestLimits limits;
    Request rq;
    ReadStatus st = read_request(conn, rq, carry, limits);
    if (st == ReadStatus::Closed || st == ReadStatus::Timeout) return RuntimeDisposition::Close;
    if (st == ReadStatus::TooLarge) {
        std::string r = http_response("431 Request Header Fields Too Large",
            "text/plain; charset=utf-8", "431\n", {}, false, false, false);
        send_all(conn, r.data(), r.size()); return RuntimeDisposition::Close;
    }
    if (st == ReadStatus::Malformed) {
        std::string r = http_response("400 Bad Request", "text/plain; charset=utf-8",
                                      "400 Bad Request\n", {}, false, false, false);
        send_all(conn, r.data(), r.size()); return RuntimeDisposition::Close;
    }

    // WebSocket upgrade -> hand the connection to a dedicated session thread and
    // free this pool worker (the session is long-lived + stateful and must not
    // occupy a bounded pool slot).
    if (ws::try_handshake(rq.raw)) {
        // Cross-site WebSocket hijacking defense: a browser lets ANY origin open
        // a ws:// to us and drive the app as the logged-in user (WS is exempt
        // from CORS). If an allowlist is configured — P::allowed_origins() or the
        // WAYA_ALLOWED_ORIGINS env (comma-separated) — reject a handshake whose
        // Origin isn't on it. Unset = allow all (dev default); a same-origin app
        // behind a proxy should set it in production.
        if (!detail::origin_allowed<P>(std::string_view{rq.raw})) {
            std::string body = "403 Forbidden: origin not allowed\n";
            std::string r = "HTTP/1.1 403 Forbidden\r\n" + detail::sec_headers() +
                "Content-Type: text/plain; charset=utf-8\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body;
            detail::send_all(conn, r.data(), r.size());
            return RuntimeDisposition::Close;
        }
        std::string req_copy = rq.raw;
        std::thread(run_ws_session<P>, conn, std::move(req_copy), port, page_bg, page_title).detach();
        return RuntimeDisposition::Owned;
    }

    // Ordinary HTTP request. Serve exactly one; keep the connection parked in
    // epoll for the next request if both sides want to.
    bool keep = serve_http<P>(conn, std::string_view{rq.raw}, port, page_bg, page_title);
    if (keep && !g_draining) return RuntimeDisposition::KeepAlive;
    return RuntimeDisposition::Close;
}

// The long-lived WebSocket session, run on its own thread. Owns `conn` and
// closes it on exit.
template <typename P>
void run_ws_session(int conn, std::string req_str, int port,
                    std::uint32_t page_bg, const char* page_title) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;
    (void)port; (void)page_bg; (void)page_title;
    std::string_view req{req_str};

    {
        auto resp = ws::try_handshake(req);
        detail::send_all(conn, resp->data(), resp->size());

        auto s = std::make_shared<Session>();
        s->conn = conn;
        detail::metrics().ws_sessions_total.fetch_add(1, std::memory_order_relaxed);
        detail::metrics().ws_sessions_live.fetch_add(1, std::memory_order_relaxed);

        // Session id for resumption: the client sends &s=<opaque> on the WS URL.
        std::string sid;
        {
            std::string rp = detail::request_path(req);
            if (auto q = rp.find("&s="); q != std::string::npos) {
                sid = rp.substr(q + 3);
                if (auto amp = sid.find('&'); amp != std::string::npos) sid = sid.substr(0, amp);
                if (auto sp = sid.find(' '); sp != std::string::npos) sid = sid.substr(0, sp);
            }
        }

        auto [model, init_cmd] = detail::init_of<P, Model, Msg>();
        // Resume: if we retained this client's model from a dropped connection,
        // rebind to it instead of the fresh init() — a reconnect keeps state.
        bool resumed = false;
        if (auto kept = detail::SessionStore::instance().take<Model>(sid)) {
            model = std::move(*kept);
            init_cmd = Cmd<Msg>::none();   // don't re-run init effects on resume
            resumed = true;
        }
        (void)resumed;
        // Route the initial model to the REQUESTED path (the client passes it as
        // ?r=<path> on the WS URL) so the live app starts on the SAME screen the
        // SSR rendered — no flash-to-Home, and the wired tokens match the DOM the
        // browser already has.
        {
            std::string rp = detail::request_path(req);   // e.g. "/?r=%2Fusers%2F2"
            std::string route = "/";
            if (auto q = rp.find("?r="); q != std::string::npos) {
                std::string enc = rp.substr(q + 3), dec;
                for (std::size_t i = 0; i < enc.size(); ++i) {
                    if (enc[i] == '%' && i + 2 < enc.size()) {
                        auto hex = [](char c){ return c<='9'?c-'0':(c|32)-'a'+10; };
                        dec += (char)(hex(enc[i+1])*16 + hex(enc[i+2])); i += 2;
                    } else if (enc[i] == '+') dec += ' ';
                    else dec += enc[i];
                }
                if (!dec.empty()) route = dec;
            }
            auto sub = detail::subs_of<P, Model, Msg>(model);
            if (auto* rt = sub.route()) {
                bool ok=true;
                auto rr = detail::safe_dispatch<P>(std::move(model), rt->route(route), route, ok);
                model = std::move(rr.first);
            }
        }
        detail::begin_msg_capture();
        detail::memo_begin_frame();
        NodeRef prev = detail::safe_view<P>(model);
        bool skipped_paint = false;   // a delta was withheld while the tab was hidden

        // First frame: a full paint. Same shape as any later frame — a
        // reconnecting client is resynced by another full paint.
        s->send_binary(ws::encode_binary(encode_full(*prev)));
        // Dev hot-reload beacon: a build id unique to this binary. When the dev
        // script rebuilds and restarts the server, the client reconnects, sees a
        // different id, and hard-reloads to pick up new shell/CSS/JS. In
        // production (WAYA_DEV unset) this is a stable constant and never fires.
        s->send_text(std::string("@build|") + detail::build_id());
        detail::perform<Msg>(s, init_cmd);
        detail::reconcile_subs<Msg>(s, detail::subs_of<P, Model, Msg>(model));

        // Reader thread: decode the WebSocket and funnel messages into the
        // queue. Runs alongside the effect threads; the owner loop below owns
        // the model and drains everything. We JOIN it before closing the fd, so
        // the socket is never closed out from under a blocking recv().
        std::thread reader([s, conn]{
            std::string acc;
            // Keepalive: wake recv() every 25s of silence to send a PING, so an
            // idle proxy/tunnel/LB (nginx/Cloudflare/ngrok/ALB, ~60s timeouts)
            // never tears the socket down. A timed-out recv returns -1/EAGAIN,
            // which we treat as "idle", not "dead".
            { timeval tv{}; tv.tv_sec = 25; tv.tv_usec = 0;
              ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); }
            // Send backpressure: a slow/stalled reader that fills the kernel
            // send buffer must not pin this session's render thread for the
            // generic 60s timeout. Cap a single send at 10s; on timeout, send
            // marks the session dead and it's reaped — so one slow client drops
            // itself instead of stalling its own updates indefinitely. (There's
            // no user-space send queue, so this is the only unbounded wait.)
            { timeval tv{}; tv.tv_sec = 10; tv.tv_usec = 0;
              ::setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); }
            // Token-bucket rate limit: a client can't pin a core by flooding taps.
            // ~120 msgs/sec sustained, burst 60. Over-limit frames are dropped
            // (not disconnected) so a legit fast typer isn't punished.
            double tokens = 60; auto last_refill = std::chrono::steady_clock::now();
            const double rate = 120.0, cap = 60.0;
            auto allow = [&]() -> bool {
                auto now = std::chrono::steady_clock::now();
                double dt = std::chrono::duration<double>(now - last_refill).count();
                last_refill = now;
                tokens = tokens + dt * rate; if (tokens > cap) tokens = cap;
                if (tokens < 1.0) return false;
                tokens -= 1.0; return true;
            };
            int idle_pings = 0;
            for (;;) {
                char fb[8192];
                ssize_t r = ::recv(conn, fb, sizeof(fb), 0);
                if (r <= 0) {
                    if (!s->alive) break;
                    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        // idle: send a keepalive ping. If the peer is truly gone,
                        // the send fails and marks the session dead, ending us.
                        s->send_binary(ws::encode_ping());
                        if (!s->alive) break;
                        if (++idle_pings > 20) break;   // ~8min of total silence: give up
                        continue;
                    }
                    break;   // real EOF / error
                }
                idle_pings = 0;
                acc.append(fb, r);
                // Bound the reassembly buffer: a peer that never completes a
                // frame can't make us allocate without limit.
                if (acc.size() > (1u << 20)) { break; }
                for (;;) {
                    std::size_t used = 0;
                    auto fr = ws::decode(acc, used);
                    // A malformed/oversized frame (opcode -2) is a protocol
                    // error: drop the whole connection instead of spinning.
                    if (fr.opcode == -2) { s->stop(); return; }
                    if (!fr.ok) break;
                    acc.erase(0, used);
                    if (fr.opcode == 0x8) { s->stop(); return; }        // close
                    if (fr.opcode == 0x9) { s->send_binary(ws::encode_pong(fr.payload)); continue; }
                    if (fr.opcode == 0xA) { continue; }                 // pong (reply to our ping): ignore
                    if (fr.opcode != 0x1) continue;                     // ignore other non-text
                    if (!allow()) continue;                             // rate-limited: drop

                    // Upstream messages: taps "<msg>"; inputs "i<msg>|<value>"
                    // / "c<msg>|<value>"; route "@route|<path>" (special msg).
                    const std::string& raw = fr.payload;
                    // Cap a single message's value length: even within the frame
                    // size limit, we won't feed an oversized string into the
                    // model loop on every keystroke.
                    constexpr std::size_t kMaxValue = 64u * 1024u;
                    if (raw.rfind("@route|", 0) == 0) {
                        std::string path = raw.substr(7);
                        if (path.size() <= kMaxValue) s->push_route(std::move(path));
                    } else if (raw.rfind("@env|", 0) == 0) {
                        // Display self-report "w|h|dark|tz" — the SIGWINCH path.
                        std::string rep = raw.substr(5);
                        if (rep.size() <= 256) s->push_env(std::move(rep));
                    } else if (raw.rfind("@storage|", 0) == 0) {
                        // A persisted localStorage value replayed on connect.
                        std::string kv = raw.substr(9);
                        if (kv.size() <= kMaxValue) s->push_storage(std::move(kv));
                    } else if (raw == "@hide") {
                        s->visible = false;         // stop shipping deltas
                    } else if (raw == "@show") {
                        s->visible = true;
                        s->push_sync();             // repaint if we skipped any
                    } else if (!raw.empty() && (raw[0]=='i' || raw[0]=='c' || raw[0]=='e' || raw[0]=='f')) {
                        // i/c: input/change value; e: a generic wired event
                        // (keyboard/focus/submit/drop); f: an uploaded file
                        // ("<token>|<name>|<mime>|<base64>") — all "<token>|<payload>".
                        auto bar = raw.find('|');
                        if (bar == std::string::npos) continue;         // malformed: no separator
                        // Checked token parse: a non-numeric token is dropped,
                        // never silently coerced to 0 (which is a valid msg).
                        char* end = nullptr;
                        long tok = std::strtol(raw.c_str() + 1, &end, 10);
                        if (!end || end != raw.c_str() + bar) continue; // token wasn't all digits
                        std::string val = raw.substr(bar + 1);
                        // Files legitimately exceed the keystroke cap; they're
                        // still bounded by the WS frame limit + client-side cap.
                        if (val.size() > (raw[0]=='f' ? ws::kMaxFrame : kMaxValue)) continue;
                        s->push_wire((int)tok, std::move(val));
                    } else if (!raw.empty()) {
                        // A bare tap is a wire token. Reject non-numeric frames.
                        char* end = nullptr;
                        long tok = std::strtol(raw.c_str(), &end, 10);
                        if (end && *end == '\0') s->push_wire((int)tok);
                    }
                }
            }
            s->stop();   // EOF / error: wake the owner loop so it can exit.
        });

        // The single owner loop: drain the queue, dispatch, interpret effects,
        // repaint the diff, reconcile subscriptions. One thread, one model — so
        // update()/view() never need a lock. `subscribe` is evaluated exactly
        // once per handled message, as in Elm.
        while (auto d = s->pop()) {
            std::pair<Model, Cmd<Msg>> r;
            bool ok = true;
            bool handled = true;
            if (d->is_sync) {
                // Tab visible again. If any paint was suppressed while hidden,
                // resync the display with ONE full frame — cheaper than the
                // pile of deltas it replaces, and always correct.
                if (skipped_paint) {
                    s->send_binary(ws::encode_binary(encode_full(*prev)));
                    skipped_paint = false;
                }
                continue;
            }
            if (d->is_env) {
                // Display self-report "w|h|dark|tz" → Sub::on_viewport → Msg.
                auto sub = detail::subs_of<P, Model, Msg>(model);
                auto* vh = sub.viewport();
                if (!vh) continue;                       // app doesn't care: drop
                Viewport vp;
                {   // parse the four '|'-separated fields (tz may be empty)
                    const std::string& v = d->value;
                    auto b1=v.find('|'); auto b2=(b1==std::string::npos)?b1:v.find('|',b1+1);
                    auto b3=(b2==std::string::npos)?b2:v.find('|',b2+1);
                    if (b1!=std::string::npos) vp.width  = std::atoi(v.substr(0,b1).c_str());
                    if (b2!=std::string::npos) vp.height = std::atoi(v.substr(b1+1,b2-b1-1).c_str());
                    if (b3!=std::string::npos){ vp.dark = v[b2+1]=='1'; vp.tz = v.substr(b3+1); }
                }
                if (vp.width <= 0 || vp.height <= 0) continue;   // malformed: drop
                r = detail::safe_dispatch<P>(std::move(model), vh->on(vp), d->value, ok);
            } else if (d->is_storage) {
                // A persisted "key|value" restored on connect → Sub::on_storage.
                auto sub = detail::subs_of<P, Model, Msg>(model);
                auto* sh = sub.storage();
                if (!sh) continue;                       // app doesn't restore: drop
                auto bar = d->value.find('|');
                if (bar == std::string::npos) continue;
                std::string key = d->value.substr(0, bar), val = d->value.substr(bar + 1);
                r = detail::safe_dispatch<P>(std::move(model), sh->on(key, val), val, ok);
            } else if (d->is_route) {
                // Route change: on_route maps the path to a Msg; the path also
                // rides as the update value (3-arg update).
                auto sub = detail::subs_of<P, Model, Msg>(model);
                auto* rt = sub.route();
                if (!rt) { handled = false; }
                else r = detail::safe_dispatch<P>(std::move(model), rt->route(d->value), d->value, ok);
            } else if (!d->topic.empty()) {
                // Broadcast: find the on_topic handler, map the payload to a Msg.
                auto sub = detail::subs_of<P, Model, Msg>(model);
                const typename Sub<Msg>::OnTopic* h = nullptr;
                for (auto* t : sub.topics()) if (t->topic == d->topic) { h = t; break; }
                if (!h) { handled = false; }
                else r = detail::safe_dispatch<P>(std::move(model), h->on(d->value), d->value, ok);
            } else if (d->has_msg()) {
                // Effect-produced typed Msg (emit/after/task/fetch/interval).
                if (auto* m = std::any_cast<Msg>(&d->msg))
                    r = detail::safe_dispatch<P>(std::move(model), *m, d->value, ok);
                else handled = false;
            } else {
                // Wire message: resolve the token (+event value) to a typed Msg
                // via the CURRENT render's registry.
                if (auto m = detail::resolve_msg<Msg>(d->token, d->value))
                    r = detail::safe_dispatch<P>(std::move(model), *m, d->value, ok);
                else handled = false;   // stale token (pre-rerender) → drop
            }
            if (!handled) { continue; }   // stale/undecodable msg: model unchanged, skip
            model = std::move(r.first);
            detail::perform<Msg>(s, r.second);

            detail::begin_msg_capture();     // fresh msg registry for this render
            detail::memo_begin_frame();      // new memo generation + amortised sweep
            NodeRef next = detail::safe_view<P>(model);
            Patch patch = diff(prev, next);
            prev = next;
            if (!patch.empty()) {
                // Suppressed while the tab is hidden: nothing would be seen, so
                // don't spend bytes. `prev` still advances — the model is live;
                // only the DISPLAY is stale — and @show resyncs with one full
                // frame. (Like not writing to an unfocused tty.)
                if (s->visible) s->send_binary(ws::encode_binary(encode_delta(patch)));
                else skipped_paint = true;
            }

            detail::reconcile_subs<Msg>(s, detail::subs_of<P, Model, Msg>(model));
            if (!s->alive) break;   // Cmd::quit or a dead socket: stop the loop.
        }
        // Orderly teardown: stop interval threads, leave all topics, unblock +
        // join the reader, then close the fd exactly once (no stale recv/send on
        // a recycled fd).
        for (auto& t : s->timers) *t.run = false;
        detail::Hub::instance().remove(s.get());
        // Retain the model so a reconnect within the TTL resumes exactly here,
        // instead of resetting to init(). Only when a session id was supplied
        // and the loop ended by the socket dropping (not an explicit Cmd::quit).
        if (!sid.empty() && !s->quit)
            detail::SessionStore::instance().save<Model>(sid, model);
        s->shutdown_io();
        if (reader.joinable()) reader.join();
        detail::metrics().ws_sessions_live.fetch_sub(1, std::memory_order_relaxed);
        detail::memo_reset();   // don't leak this session's cache onto a recycled thread
        ::close(conn);
        return;
    }
}

// Serve ONE HTTP request (non-WebSocket) on an already-read request `req`.
// Returns true to KEEP the connection alive for the next request, false to
// close it. Never closes the socket itself — the keep-alive loop in handle()
// owns the fd's lifetime.
template <typename P>
bool serve_http(int conn, std::string_view req, int port,
                std::uint32_t page_bg, const char* page_title) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    // ── SSR FIRST PAINT ────────────────────────────────────────────
    std::string route = request_path(req);
    std::string_view method = request_method(req);
    bool head_only = (method == "HEAD");

    // Persist the connection when the client wants it AND we're not draining.
    // A single Request parse serves keep-alive + conditional-request lookups.
    Request rq{std::string(req), {}};
    const bool keep = rq.wants_keep_alive() && !g_draining;
    const char* conn_hdr = keep ? "Connection: keep-alive\r\nKeep-Alive: timeout=60\r\n"
                                : "Connection: close\r\n";

    // OPTIONS is answered with the allowed methods (RFC 9110 9.3.7).
    if (method == "OPTIONS") {
        std::string r = "HTTP/1.1 204 No Content\r\n" + sec_headers() +
                        "Allow: GET, HEAD, OPTIONS\r\n" + conn_hdr + "Content-Length: 0\r\n\r\n";
        send_all(conn, r.data(), r.size()); access_log(method, route, 204); return keep;
    }
    // A live SSR server serves GET/HEAD only. Anything else is 405 + Allow.
    if (method != "GET" && !head_only) {
        auto r = http_response("405 Method Not Allowed", "text/plain; charset=utf-8",
                               "405 Method Not Allowed\n", "Allow: GET, HEAD, OPTIONS\r\n",
                               false, false, keep);
        send_all(conn, r.data(), r.size());
        access_log(method, route, 405);
        return keep;
    }

    // Health check for load balancers / orchestrators.
    if (route == "/healthz" || route.rfind("/healthz?",0)==0) {
        auto r = http_response("200 OK", "text/plain; charset=utf-8", "ok", {}, head_only, false, keep);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); return keep;
    }

    // Prometheus metrics — request/error totals, live sessions, uptime. Gated:
    // exposed only when WAYA_METRICS is set or the app opts in via
    // `static bool expose_metrics()` (metrics can leak traffic shape, so it's
    // off by default; scrape it on an internal network / behind auth).
    {
        bool metrics_on = std::getenv("WAYA_METRICS") != nullptr;
        if constexpr (requires { P::expose_metrics(); }) metrics_on = metrics_on || P::expose_metrics();
        if (metrics_on && (route == "/metrics" || route.rfind("/metrics?",0)==0)) {
            auto r = http_response("200 OK", "text/plain; version=0.0.4; charset=utf-8",
                                   detail::metrics_text(), {}, head_only, false, keep);
            send_all(conn, r.data(), r.size()); access_log(method, route, 200); return keep;
        }
    }

    if (route == "/robots.txt" || route.rfind("/robots.txt?",0)==0) {
        std::string body = "User-agent: *\nAllow: /\n";
        if constexpr (requires { P::site_url(); }) body += "Sitemap: " + std::string(P::site_url()) + "/sitemap.xml\n";
        auto r = http_response("200 OK", "text/plain; charset=utf-8", body, {}, head_only, /*cache=*/true, keep);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); return keep;
    }
    if (route == "/sitemap.xml" || route.rfind("/sitemap.xml?",0)==0) {
        std::string base; if constexpr (requires { P::site_url(); }) base = P::site_url();
        std::vector<std::string> paths;
        if constexpr (requires { P::sitemap(); }) paths = P::sitemap();
        else paths = {"/"};
        std::string body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">";
        for (auto& p : paths){ body += "<url><loc>"; body += base + p; body += "</loc></url>"; }
        body += "</urlset>";
        auto r = http_response("200 OK", "application/xml; charset=utf-8", body, {}, head_only, /*cache=*/true, keep);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); return keep;
    }
    if (route == "/favicon.ico") {
        std::string r = "HTTP/1.1 204 No Content\r\n" + sec_headers() +
                        "Cache-Control: public, max-age=86400\r\n" + conn_hdr + "Content-Length: 0\r\n\r\n";
        send_all(conn, r.data(), r.size()); access_log(method, route, 204); return keep;
    }

    auto [ssr_model, ssr_cmd] = detail::init_of<P, Model, Msg>();
    (void)ssr_cmd;
    // Route the model to the requested path so /about SSRs the about screen, etc.
    {
        auto sub = detail::subs_of<P, Model, Msg>(ssr_model);
        if (auto* rt = sub.route()) {
            bool ok=true;
            auto r = detail::safe_dispatch<P>(std::move(ssr_model), rt->route(route), route, ok);
            ssr_model = std::move(r.first);
        }
    }
    detail::begin_msg_capture();
    detail::memo_begin_frame();

    // Per-route HTTP outcome: an app may declare `static HttpResult
    // http_status(const Model&)` to return the RIGHT status for this route — a
    // real 404 for an unknown page, a 301/302 redirect, custom caching, cookies.
    // Defaults to a cacheless 200. A redirect short-circuits (no body rendered).
    HttpResult hr;
    if constexpr (requires { P::http_status(ssr_model); }) {
        hr = P::http_status(ssr_model);
    }
    if (hr.is_redirect()) {
        std::string extra = "Location: " + hr.location + "\r\n";
        for (auto& c : hr.cookies)         extra += "Set-Cookie: " + c + "\r\n";
        for (auto& [k,v] : hr.headers)     extra += k + ": " + v + "\r\n";
        std::string rr = http_response(hr.status_line(), "text/html; charset=utf-8",
                                       "<a href=\"" + hr.location + "\">Redirecting…</a>",
                                       extra, head_only, false, keep);
        send_all(conn, rr.data(), rr.size());
        access_log(method, route, hr.status);
        return keep;
    }

    NodeRef ssr_root = detail::safe_view<P>(ssr_model);   // captures tokens into a fresh table
    auto ssr = DomBackend{}.render(*ssr_root);   // {html, css}
    detail::memo_reset();   // SSR is a one-shot on this thread; start clean next time

    // Per-route SEO metadata, computed from the routed model.
    Meta mt = detail::meta_of<P, Model>(ssr_model);
    std::string head_seo = detail::render_head(mt, page_title);
    std::string html_lang = mt.lang.empty() ? std::string("en") : mt.lang;

    // Initial HTML: the SSR'd surface in #root, the app's CSS inline (so it's
    // styled on first paint), and the client script that upgrades to live.
    char bghex[8]; std::snprintf(bghex, sizeof(bghex), "#%06x", page_bg & 0xFFFFFF);
    std::string bg = bghex;
    std::string doc =
        "<!DOCTYPE html><html lang=\"" + html_lang + "\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
        "<meta name=\"theme-color\" content=\"" + bg + "\">"
        "<title>" + [&]{ std::string t; std::string src = mt.title.empty() ? std::string(page_title?page_title:"") : mt.title; for(char c:src){ if(c=='<')t+="&lt;"; else if(c=='>')t+="&gt;"; else if(c=='&')t+="&amp;"; else t+=c; } return t; }() + "</title>"
        + head_seo
        + assets().head_html()
        // App-provided <head> HTML: analytics snippets (GA/Plausible/PostHog),
        // font <link>s, site-verification <meta>s. Optional static P::head()
        // returning a raw string; spliced verbatim into <head> (it's the app's
        // own trusted markup, like markup()). Runs per request so it can vary
        // by nothing here, but it's a stable string in practice.
        + [&]() -> std::string {
              if constexpr (requires { P::head(); }) return std::string(P::head());
              else return std::string{};
          }() +
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        // Root fills the viewport; overscroll is contained on html itself so the
        // rubber-band at the top/bottom never reveals anything behind the app.
        "html,body{height:100%;min-height:100%}"
        "html{-webkit-text-size-adjust:100%;text-size-adjust:100%;overscroll-behavior:none}"
        // Hard stop against horizontal overflow: the viewport can never scroll
        // sideways, so an over-wide element (huge text, a fixed-width block) is
        // clipped/contained rather than pushing the whole page off to the right.
        "html,body{overflow-x:hidden;max-width:100%}"
        // The page background is painted on HTML+BODY (not just the app root), so
        // there is NEVER white behind the app — not during overscroll bounce, not
        // in the safe-area insets, not before the socket paints the first frame.
        "html,body{background:" + bg + "}"
        "body{overscroll-behavior:none;-webkit-tap-highlight-color:transparent;touch-action:manipulation}"
        // A real sans-serif stack; force EVERY element (incl. form controls,
        // which don't inherit font by default) to use it — otherwise inputs and
        // buttons render in the UA's monospace/serif default.
        "body{font-family:ui-sans-serif,system-ui,-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;line-height:1.5;-webkit-font-smoothing:antialiased}"
        "*,input,button,textarea,select{font-family:inherit;font-size:inherit;line-height:inherit;color:inherit}"
        "input,button,textarea,select{border:0;background:none;outline:none}"
        // RESPONSIVE BY DEFAULT: nothing may overflow its container. min-width:0
        // lets flex children shrink below their content size (the #1 fix for
        // 'my row won't wrap / overflows on mobile'); max-width:100% caps every
        // box to its parent. Together these make any layout fit any viewport
        // without the author writing a single media query or width.
        "*{min-width:0;max-width:100%}"
        "svg{display:block}"
        "img,video{max-width:100%;height:auto}"
        // Optimistic feedback: any tap target dims + nudges the instant it's
        // pressed, BEFORE the server round-trip, so the UI never feels laggy on
        // a slow link. [data-busy] (set by the client on click, cleared on the
        // next paint) shows a wait cursor + reduced opacity for in-flight actions.
        "[data-tap]{cursor:pointer;-webkit-user-select:none;user-select:none}"
        "[data-tap]:active{transform:scale(.97);opacity:.85}"
        "[data-busy]{opacity:.6;cursor:progress;pointer-events:none}"
        // #root is the page surface: a full-viewport centering flex column that
        // INHERITS the page background, so the app root's bg (opaque) paints over
        // it and there are never white gutters. min-height uses dvh so it tracks
        // the mobile browser chrome; the app root fills width + stretches.
        "#root{min-height:100vh;min-height:100dvh;display:flex;flex-direction:column;align-items:stretch;background:inherit}"
        "#root>*{flex:1 0 auto}"
        // Motion library: a fixed set of @keyframes the animation mods reference
        // by name (spin/pulse/shimmer/fade/slide/bounce). Defined ONCE here so
        // animations cost nothing per element — a mod just sets `animation:...`.
        "@keyframes wa-spin{to{transform:rotate(360deg)}}"
        "@keyframes wa-pulse{0%,100%{opacity:1}50%{opacity:.45}}"
        "@keyframes wa-ping{75%,100%{transform:scale(2);opacity:0}}"
        "@keyframes wa-bounce{0%,100%{transform:translateY(0)}50%{transform:translateY(-25%)}}"
        "@keyframes wa-shimmer{100%{background-position:-200% 0}}"
        "@keyframes wa-fade{from{opacity:0}to{opacity:1}}"
        "@keyframes wa-fade-up{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}"
        "@keyframes wa-fade-down{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:none}}"
        "@keyframes wa-slide-left{from{opacity:0;transform:translateX(16px)}to{opacity:1;transform:none}}"
        "@keyframes wa-slide-right{from{opacity:0;transform:translateX(-16px)}to{opacity:1;transform:none}}"
        "@keyframes wa-pop{0%{opacity:0;transform:scale(.92)}60%{transform:scale(1.02)}100%{opacity:1;transform:none}}"
        // "cool" library: an aurora background that drifts, a gradient-text hue
        // shift, a gentle float, and a breathing glow. Used by the flashy mods.
        "@keyframes wa-aurora{0%,100%{background-position:0% 50%}50%{background-position:100% 50%}}"
        "@keyframes wa-hue{to{filter:hue-rotate(360deg)}}"
        "@keyframes wa-float{0%,100%{transform:translateY(0)}50%{transform:translateY(-8px)}}"
        "@keyframes wa-breathe{0%,100%{opacity:.6;transform:scale(1)}50%{opacity:1;transform:scale(1.04)}}"
        "@keyframes wa-sheen{0%{transform:translateX(-120%) skewX(-20deg)}60%,100%{transform:translateX(220%) skewX(-20deg)}}"
        // Respect the user's reduced-motion preference — accessibility, by default.
        "@media(prefers-reduced-motion:reduce){*{animation-duration:.001ms!important;animation-iteration-count:1!important;transition-duration:.001ms!important}}"
        // User- and component-library-registered document assets: :root design
        // tokens, custom @keyframes, @font-face, and global rules (::selection,
        // scrollbars, resets). Emitted LAST so they win over the defaults above.
        + assets().style_css() +
        "</style>"
        "<style id=\"wsheet\">" + ssr.css + "</style>"
        "</head><body><div id=\"root\">" + ssr.html + "</div>" + client(port) + "</body></html>";
    std::string http;
    int status = 200;
    // Conditional request: a weak ETag over the rendered document lets a caching
    // proxy (or the browser) revalidate cheaply. If it still matches, answer
    // 304 Not Modified with no body (RFC 9110 13.1.2 / 15.4.5). Only for the
    // default 200 render — redirects/custom statuses already returned above.
    std::string etag = weak_etag(doc);
    {
        std::string inm{ Request{std::string(req), {}}.header("If-None-Match") };
        if (!inm.empty() && inm.find(etag) != std::string_view::npos) {
            std::string r = "HTTP/1.1 304 Not Modified\r\n" + sec_headers() +
                            "ETag: " + etag + "\r\nCache-Control: no-cache\r\n" +
                            std::string(keep ? "Connection: keep-alive\r\n" : "Connection: close\r\n") +
                            "Content-Length: 0\r\n\r\n";
            send_all(conn, r.data(), r.size());
            access_log(method, route, 304);
            return keep;
        }
    }
    const std::string etag_hdr = "ETag: " + etag + "\r\n";
#ifdef WAYA_GZIP
    if (accepts_gzip(req)) {
        std::string gz = gzip(doc);
        if (!gz.empty()) {
            http = http_response("200 OK", "text/html; charset=utf-8", gz,
                                 "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n" + etag_hdr, head_only, false, keep);
        }
    }
#endif
    if (http.empty())
        http = http_response("200 OK", "text/html; charset=utf-8", doc, etag_hdr, head_only, false, keep);
    send_all(conn, http.data(), http.size());
    access_log(method, route, status);
    return keep;
}

} // namespace detail

/// Serve a Surface Program live. Thread-per-connection (one open client can't
/// block others). Blocks until Ctrl-C. The accept loop, socket setup + hardening
/// live in the compiled runtime (surface/runtime.cpp); this templated shell only
/// builds the per-connection handler that knows the app's Program.
template <typename P>
int live(LiveConfig cfg = {}) {
    check_program<P>();   // readable diagnostics before anything else
    if (const char* p = std::getenv("WAYA_PORT")) cfg.port = std::atoi(p);
    if (const char* h = std::getenv("WAYA_HOST")) cfg.host = h;

    ServeConfig sc;
    sc.port  = cfg.port;
    sc.host  = cfg.host;
    sc.open  = cfg.open;

    // The ONLY app-specific piece: handle one connection by rendering/serving
    // Program P. Everything else (accept, sockets, signals, banner) is compiled
    // once in detail::serve().
    auto page_bg = cfg.page_bg;
    auto title   = cfg.title;
    int  port    = cfg.port;
    return detail::serve(sc, [port, page_bg, title](int conn, std::string& carry){
        return detail::handle<P>(conn, carry, port, page_bg, title);
    });
}

} // namespace waya::surface
