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

inline std::atomic<int> g_fd{-1};
inline void on_sigint(int){ int fd=g_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

/// The machine's primary LAN IP — so a 0.0.0.0-bound app can print a URL other
/// devices can reach. Trick: "connect" a UDP socket toward a public address (no
/// packet is sent) and read back the local endpoint the OS picked. Empty on
/// failure (offline / no route).
inline std::string lan_ip() {
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return {};
    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(53);
    to.sin_addr.s_addr = inet_addr("8.8.8.8");
    std::string ip;
    if (::connect(s, (sockaddr*)&to, sizeof(to)) == 0) {
        sockaddr_in me{}; socklen_t len = sizeof(me);
        if (::getsockname(s, (sockaddr*)&me, &len) == 0) {
            char buf[INET_ADDRSTRLEN];
            if (::inet_ntop(AF_INET, &me.sin_addr, buf, sizeof(buf))) ip = buf;
        }
    }
    ::close(s);
    return ip;
}

/// Reserved message id for route deliveries. The wire never carries this from a
/// tap (taps are the app's own enum values, always >= 0 in practice); the
/// runtime injects it when a "@route|<path>" frame arrives and routes it through
/// the app's Sub::on_route handler. Chosen far from any plausible app enum.
inline constexpr int kRouteMsg = -0x7ACE;
/// Reserved message id for a topic broadcast delivery. The owner loop reads the
/// topic+payload off the Deliver and maps it through the app's on_topic handler.
inline constexpr int kTopicMsg = -0x7ACD;

/// The request line's path, e.g. "/about?x=1" from "GET /about?x=1 HTTP/1.1".
/// Used to SSR the CORRECT screen for the requested route on first paint.
inline std::string request_path(std::string_view req){
    auto sp = req.find(' ');
    if (sp == std::string_view::npos) return "/";
    auto start = sp + 1;
    auto end = req.find(' ', start);
    if (end == std::string_view::npos || end <= start) return "/";
    std::string p{req.substr(start, end - start)};
    return p.empty() ? "/" : p;
}
/// The HTTP method token, e.g. "GET" from "GET /about HTTP/1.1".
inline std::string_view request_method(std::string_view req){
    auto sp = req.find(' ');
    return sp == std::string_view::npos ? std::string_view{"GET"} : req.substr(0, sp);
}
/// The set of response headers a production SSR page always carries: the type,
/// a caching policy, and the security headers a hardened server sends. `extra`
/// appends anything response-specific (Content-Encoding, Allow, ...).
/// - no-store on the HTML so a personalised, live-upgraded page is never cached
///   stale by a browser/proxy (the app streams its own updates over the socket);
/// - nosniff stops content-type confusion attacks;
/// - frame-ancestors 'self' + X-Frame-Options blocks clickjacking;
/// - a strict-origin referrer policy leaks less on outbound links.
inline std::string sec_headers(){
    return "X-Content-Type-Options: nosniff\r\n"
           "X-Frame-Options: SAMEORIGIN\r\n"
           "Content-Security-Policy: frame-ancestors 'self'\r\n"
           "Referrer-Policy: strict-origin-when-cross-origin\r\n";
}
/// Build a complete HTTP/1.1 response. `status` is the full status line text
/// ("200 OK", "404 Not Found", "405 Method Not Allowed"), `ctype` the
/// Content-Type. `head_only` sends headers but omits the body (HTTP HEAD).
inline std::string http_response(const char* status, const std::string& ctype,
                                 const std::string& body, const std::string& extra_headers = {},
                                 bool head_only = false, bool cache = false){
    std::string h = "HTTP/1.1 " + std::string(status) + "\r\n";
    h += "Content-Type: " + ctype + "\r\n";
    h += cache ? "Cache-Control: public, max-age=3600\r\n"
               : "Cache-Control: no-store\r\n";
    h += sec_headers();
    h += extra_headers;
    h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    h += "Connection: close\r\n\r\n";
    if (!head_only) h += body;
    return h;
}
/// A structured access-log line: method, path, status. Opt-in — off by default
/// so a dev run isn't noisy, on when WAYA_LOG is set (production behind a proxy).
/// One line per request to stderr, so it composes with journald/Docker logs.
inline void access_log(std::string_view method, const std::string& path, int status){
    static const bool on = std::getenv("WAYA_LOG") != nullptr;
    if (!on) return;
    std::time_t t = std::time(nullptr); char ts[32];
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    std::fprintf(stderr, "waya: %s %.*s %s %d\n", ts, (int)method.size(), method.data(), path.c_str(), status);
}
/// Write ALL bytes, looping over short/partial writes. A single ::send can
/// short-write on a backpressured socket (far likelier through a proxy than on
/// loopback), which would truncate an HTTP response or handshake. Returns false
/// if the peer went away.
inline bool send_all(int fd, const char* data, std::size_t len){
    std::size_t off = 0;
    while (off < len) {
        ssize_t w = ::send(fd, data + off, len - off, WAYA_MSG_NOSIGNAL);
        if (w > 0) { off += (std::size_t)w; continue; }
        if (w < 0 && (errno == EINTR)) continue;
        return false;   // EAGAIN on a blocking socket is rare; treat as failure
    }
    return true;
}
/// True if the client advertised gzip in Accept-Encoding.
inline bool accepts_gzip(std::string_view req){
    auto pos = req.find("Accept-Encoding:");
    if (pos == std::string_view::npos) pos = req.find("accept-encoding:");
    if (pos == std::string_view::npos) return false;
    auto eol = req.find("\r\n", pos);
    return req.substr(pos, (eol==std::string_view::npos?req.size():eol) - pos).find("gzip") != std::string_view::npos;
}

#ifdef WAYA_GZIP
/// gzip a buffer (opt-in: compile with -DWAYA_GZIP and link zlib). Returns empty
/// on failure so the caller falls back to sending the body uncompressed.
inline std::string gzip(const std::string& in){
    z_stream zs{};
    if (deflateInit2(&zs, Z_BEST_SPEED, Z_DEFLATED, 15+16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};
    zs.next_in = (Bytef*)in.data(); zs.avail_in = (uInt)in.size();
    std::string out; char buf[16384];
    int ret;
    do {
        zs.next_out = (Bytef*)buf; zs.avail_out = sizeof(buf);
        ret = deflate(&zs, Z_FINISH);
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret == Z_OK);
    deflateEnd(&zs);
    return ret == Z_STREAM_END ? out : std::string{};
}
#endif

/// The Program hooks — dispatch/init_of/subs_of/meta_of (which shape of the
/// app's update/init/subscribe/meta it uses) — live in surface/program.hpp.
/// Below are the runtime-only helpers: error boundaries, the build id, etc.

/// An error card — shown in place of the app when view()/update() throws, so a
/// bug isolates the session instead of crashing the server. Valid HTML/CSS.
inline std::string error_html(std::string_view what){
    std::string safe; for(char c : what){ if(c=='<')safe+="&lt;"; else if(c=='>')safe+="&gt;"; else if(c=='&')safe+="&amp;"; else safe+=c; }
    return "<div style=\"min-height:100dvh;display:flex;align-items:center;justify-content:center;"
           "padding:24px;background:#0b1020;color:#e2e8f0;font-family:ui-sans-serif,system-ui,sans-serif\">"
           "<div style=\"max-width:32rem;padding:24px;border-radius:16px;background:#141b2e;"
           "border:1px solid #ef444455\">"
           "<div style=\"font-size:15px;font-weight:700;color:#ef4444;margin-bottom:8px\">"
           "Something went wrong</div>"
           "<div style=\"font-size:13px;color:#94a3b8;line-height:1.6;white-space:pre-wrap\">" + safe +
           "</div></div></div>";
}

/// Render P::view(model) with an ERROR BOUNDARY: if the app's view throws, we
/// return an error card node instead of letting the exception unwind into the
/// detached thread (which would std::terminate the whole process). Keeps the
/// server and every other session alive.
/// A build id unique to this compiled binary (the compile timestamp). Used by
/// the dev hot-reload beacon: a rebuild produces a new id, so a reconnecting
/// client can tell "the server was rebuilt" apart from "the network blipped."
inline const char* build_id(){ return __DATE__ " " __TIME__; }

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

/// The terminal is the browser client (surface/client.hpp) — it holds no app
/// state or logic, just decodes binary frames and paints them. Kept in its own
/// file so the transport/runtime here stays free of the ~6 KB JS blob.

/// A live session: the single owner of one connection's model + render loop.
/// Background effects (timers, tasks, fetches, wire input) all funnel messages
/// into `queue`; the loop drains it, so the model is only ever touched by one
/// thread. `write` is serialized so paint frames and control frames from
/// different threads never interleave on the socket.
struct Session {
    int conn;
    std::mutex qm;
    std::condition_variable qcv;
    std::deque<Deliver> queue;         // pending (msg,value) to dispatch
    std::atomic<bool> alive{true};
    std::atomic<bool> quit{false};    // true only on an explicit Cmd::quit (not a drop)
    std::mutex wm;                     // serializes socket writes
    // Running interval subscriptions. Each carries the typed Msg to deliver on
    // tick, keyed for reconciliation by (interval_ms, Msg-token).
    struct Timer { long ms; std::uint64_t key; std::any msg; std::shared_ptr<std::atomic<bool>> run; };
    std::vector<Timer> timers;

    /// Push a WIRE message: a token (looked up in the msg registry) + value.
    void push_wire(int token, std::string value = {}) {
        { std::lock_guard<std::mutex> l(qm); Deliver d; d.token=token; d.value=std::move(value); queue.push_back(std::move(d)); }
        qcv.notify_one();
    }
    /// Push an already-typed Msg produced by an effect (emit/after/task/fetch).
    void push_msg(std::any msg) {
        { std::lock_guard<std::mutex> l(qm); Deliver d; d.msg=std::move(msg); queue.push_back(std::move(d)); }
        qcv.notify_one();
    }
    /// Push a route change (value = path).
    void push_route(std::string path) {
        { std::lock_guard<std::mutex> l(qm); Deliver d; d.is_route=true; d.value=std::move(path); queue.push_back(std::move(d)); }
        qcv.notify_one();
    }
    /// Deliver a topic broadcast: the owner loop resolves the on_topic handler.
    void push_topic(std::string topic, std::string payload) {
        { std::lock_guard<std::mutex> l(qm); Deliver d; d.topic=std::move(topic); d.value=std::move(payload); queue.push_back(std::move(d)); }
        qcv.notify_one();
    }
    std::optional<Deliver> pop() {
        std::unique_lock<std::mutex> l(qm);
        qcv.wait(l, [&]{ return !queue.empty() || !alive; });
        if (!alive && queue.empty()) return std::nullopt;
        Deliver d = std::move(queue.front()); queue.pop_front();
        return d;
    }
    void stop() { alive = false; qcv.notify_all(); }
    /// Explicit application quit (Cmd::quit) — distinct from a dropped socket, so
    /// teardown knows NOT to retain the model for resumption.
    void quit_now() { quit = true; alive = false; qcv.notify_all(); }

    /// Unblock a reader parked in ::recv and wake the owner loop. Does NOT close
    /// the fd — the owner loop is the sole closer, after the reader has exited,
    /// so the fd number can never be recycled under a stale recv()/send().
    void shutdown_io() {
        alive = false;
        ::shutdown(conn, SHUT_RDWR);   // makes the blocking recv return 0/-1
        qcv.notify_all();
    }

    void send_binary(const std::string& frame) {
        if (!alive) return;
        std::lock_guard<std::mutex> l(wm);
        if (::send(conn, frame.data(), frame.size(), WAYA_MSG_NOSIGNAL) < 0) alive = false;
    }
    void send_text(const std::string& s) {
        if (!alive) return;
        auto f = ws::encode_text(s);
        std::lock_guard<std::mutex> l(wm);
        if (::send(conn, f.data(), f.size(), WAYA_MSG_NOSIGNAL) < 0) alive = false;
    }
};

/// The broadcast Hub: a process-global, thread-safe registry mapping a topic to
/// the sessions subscribed to it. `Cmd::broadcast` publishes into it and it
/// fans the payload into every subscribed session's queue (each session then
/// dispatches it through its OWN update, so no shared model, no locks in app
/// code). Sessions register/unregister as their Sub::on_topic set changes.
/// Weak pointers mean a dropped connection is reaped lazily on the next publish.
class Hub {
public:
    static Hub& instance() { static Hub h; return h; }

    /// Set the exact set of topics this session is subscribed to (idempotent).
    void set_topics(const std::shared_ptr<Session>& s, const std::vector<std::string>& topics) {
        std::lock_guard<std::mutex> l(m_);
        Session* key = s.get();
        // Remove from topics no longer wanted.
        auto cur = joined_[key];
        for (auto& t : cur)
            if (std::find(topics.begin(), topics.end(), t) == topics.end())
                drop(t, key);
        // Add to newly wanted topics.
        for (auto& t : topics)
            if (std::find(cur.begin(), cur.end(), t) == cur.end())
                subs_[t].push_back(s);
        if (topics.empty()) joined_.erase(key);
        else joined_[key] = topics;
    }

    /// Publish `payload` to every session currently on `topic` (incl. sender).
    void publish(const std::string& topic, const std::string& payload) {
        std::vector<std::shared_ptr<Session>> live;
        {
            std::lock_guard<std::mutex> l(m_);
            auto it = subs_.find(topic);
            if (it == subs_.end()) return;
            auto& vec = it->second;
            // Reap dead sessions while collecting the live ones (lazy GC).
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [&](const std::weak_ptr<Session>& w){
                    if (auto sp = w.lock(); sp && sp->alive) { live.push_back(sp); return false; }
                    return true;
                }), vec.end());
            if (vec.empty()) subs_.erase(it);
        }
        // Deliver OUTSIDE the lock: each push takes the session's own queue lock,
        // so a slow/blocked receiver can never stall the publisher or the Hub.
        for (auto& sp : live) sp->push_topic(topic, payload);
    }

    /// Drop a session from every topic (called on teardown).
    void remove(Session* key) {
        std::lock_guard<std::mutex> l(m_);
        auto it = joined_.find(key);
        if (it == joined_.end()) return;
        for (auto& t : it->second) drop(t, key);
        joined_.erase(it);
    }

private:
    void drop(const std::string& topic, Session* key) {  // caller holds m_
        auto it = subs_.find(topic);
        if (it == subs_.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&](const std::weak_ptr<Session>& w){ auto sp = w.lock(); return !sp || sp.get() == key; }),
            vec.end());
        if (vec.empty()) subs_.erase(it);
    }
    std::mutex m_;
    std::unordered_map<std::string, std::vector<std::weak_ptr<Session>>> subs_;
    std::unordered_map<Session*, std::vector<std::string>> joined_;
};

/// A bounded worker pool for blocking effects (fetch/task) and one-shot timers.
/// Replaces spawning an unbounded std::thread per effect — under load (many
/// fetches, many timers) that exhausts the OS thread limit and falls over. A
/// fixed pool caps concurrency; excess work queues. Sized from hardware
/// concurrency (min 4), overridable via WAYA_WORKERS.
class Pool {
public:
    static Pool& instance() { static Pool p; return p; }

    void submit(std::function<void()> job) {
        { std::lock_guard<std::mutex> l(m_); jobs_.push_back(std::move(job)); }
        cv_.notify_one();
    }

private:
    Pool() {
        unsigned n = std::thread::hardware_concurrency();
        if (const char* w = std::getenv("WAYA_WORKERS")) n = (unsigned)std::atoi(w);
        if (n < 4) n = 4;
        for (unsigned i = 0; i < n; ++i)
            std::thread([this]{ worker(); }).detach();
    }
    void worker() {
        for (;;) {
            std::function<void()> job;
            { std::unique_lock<std::mutex> l(m_);
              cv_.wait(l, [this]{ return !jobs_.empty(); });
              job = std::move(jobs_.front()); jobs_.pop_front(); }
            try { job(); }
            catch (const std::exception& e) {
                // An effect must never kill a worker — but a silently-dropped
                // effect is a debugging nightmare, so surface it loudly in debug.
#ifndef NDEBUG
                std::fprintf(stderr, "waya: effect threw (dropped): %s\n", e.what());
#else
                (void)e;
#endif
            }
            catch (...) {
#ifndef NDEBUG
                std::fprintf(stderr, "waya: effect threw (dropped): unknown exception\n");
#endif
            }
        }
    }
    std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> jobs_;
};

/// Retained models for session resumption. When a connection drops, we stash the
/// app's Model (type-erased) under the client's session id with a timestamp; a
/// reconnect within the TTL rebinds to it instead of calling init() again — so a
/// wifi blip or a slept laptop doesn't wipe a half-filled form. Entries older
/// than the TTL are swept lazily on access. Keyed by client-supplied id (opaque
/// per-tab token from sessionStorage), so it's per-tab, not cross-user shared.
class SessionStore {
public:
    static SessionStore& instance() { static SessionStore s; return s; }

    /// Seconds a detached model is kept for a possible reconnect.
    long ttl_seconds = 900;   // 15 minutes

    template <typename Model>
    void save(const std::string& id, Model model) {
        if (id.empty()) return;
        std::lock_guard<std::mutex> l(m_);
        sweep_locked();
        store_[id] = Entry{ std::any{std::move(model)}, now() };
    }

    /// Take the retained model for `id` if present and fresh; removes it (a model
    /// belongs to exactly one live owner loop at a time).
    template <typename Model>
    std::optional<Model> take(const std::string& id) {
        if (id.empty()) return std::nullopt;
        std::lock_guard<std::mutex> l(m_);
        sweep_locked();
        auto it = store_.find(id);
        if (it == store_.end()) return std::nullopt;
        auto* mp = std::any_cast<Model>(&it->second.model);
        if (!mp) { store_.erase(it); return std::nullopt; }   // type changed (rebuild)
        Model m = std::move(*mp);
        store_.erase(it);
        return m;
    }

private:
    struct Entry { std::any model; long ts; };
    static long now() {
        return (long)std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void sweep_locked() {
        long cutoff = now() - ttl_seconds;
        for (auto it = store_.begin(); it != store_.end();)
            it = (it->second.ts < cutoff) ? store_.erase(it) : std::next(it);
    }
    std::mutex m_;
    std::unordered_map<std::string, Entry> store_;
};

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
            std::thread([ws_, ms, m = std::move(m)]{
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(m);
            }).detach();
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
        std::thread([ws_, ms, m, run]{
            while (*run) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                if (!*run) break;
                auto sp = ws_.lock();
                if (!sp || !sp->alive) break;
                sp->push_msg(m);
            }
        }).detach();
        next.push_back({ms, key, std::move(m), run});
    }
    s->timers = std::move(next);

    // Reconcile pub/sub topics: register the session for exactly the topics its
    // subscription currently declares (idempotent — joining/leaving a room is
    // just a model change that adds/removes an on_topic).
    std::vector<std::string> topics;
    for (auto* t : sub.topics()) topics.push_back(t->topic);
    Hub::instance().set_topics(s, topics);
}

template <typename P>
void handle(int conn, int port, std::uint32_t page_bg = 0x0b1020, const char* page_title = "waya") {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    // Read the full HTTP request — headers can arrive across MULTIPLE recv()s
    // (TCP segmentation, common behind a proxy) and a proxy fattens the header
    // set (X-Forwarded-*, CF-*, cookies) well past a single small read. Loop
    // until the end-of-headers marker, bounded so a slow-loris can't grow us
    // without limit. On localhost a request usually arrives whole; a proxy is
    // where the one-shot read silently truncated the handshake.
    std::string reqbuf;
    {
        constexpr std::size_t kMaxHeaders = 64u * 1024u;
        char rb[8192];
        for (;;) {
            ssize_t n = ::recv(conn, rb, sizeof(rb), 0);
            if (n <= 0) { if (reqbuf.empty()) { ::close(conn); return; } break; }
            reqbuf.append(rb, (std::size_t)n);
            if (reqbuf.find("\r\n\r\n") != std::string::npos) break;   // headers complete
            if (reqbuf.size() > kMaxHeaders) break;                    // guard: stop reading
        }
    }
    std::string_view req{reqbuf};

    if (auto resp = ws::try_handshake(req)) {
        detail::send_all(conn, resp->data(), resp->size());

        auto s = std::make_shared<Session>();
        s->conn = conn;

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
                    } else if (!raw.empty() && (raw[0]=='i' || raw[0]=='c' || raw[0]=='e')) {
                        // i/c: input/change value; e: a generic wired event
                        // (keyboard/focus/submit/drop) — all carry "<token>|<payload>".
                        auto bar = raw.find('|');
                        if (bar == std::string::npos) continue;         // malformed: no separator
                        // Checked token parse: a non-numeric token is dropped,
                        // never silently coerced to 0 (which is a valid msg).
                        char* end = nullptr;
                        long tok = std::strtol(raw.c_str() + 1, &end, 10);
                        if (!end || end != raw.c_str() + bar) continue; // token wasn't all digits
                        std::string val = raw.substr(bar + 1);
                        if (val.size() > kMaxValue) continue;           // oversized value: drop
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
            if (d->is_route) {
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
            if (!patch.empty())
                s->send_binary(ws::encode_binary(encode_delta(patch)));

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
        detail::memo_reset();   // don't leak this session's cache onto a recycled thread
        ::close(conn);
        return;
    }

    // ── SSR FIRST PAINT ────────────────────────────────────────────
    // Render the app's CURRENT screen for the REQUESTED route directly into the
    // initial HTML, plus its CSS inline. The browser shows real content on the
    // first byte — no blank flash, works before/without JS, and is crawlable.
    // The WebSocket then takes over live; its first full paint reconciles onto
    // this SSR'd DOM (same node structure → usually a no-op).
    std::string route = request_path(req);
    std::string_view method = request_method(req);
    bool head_only = (method == "HEAD");
    // A live SSR server serves GET/HEAD only (state changes travel over the
    // socket, not HTTP verbs). Anything else is 405 with an Allow header —
    // correct, and it stops a stray POST from getting a 200 HTML page.
    if (method != "GET" && !head_only) {
        auto r = http_response("405 Method Not Allowed", "text/plain; charset=utf-8",
                               "405 Method Not Allowed\n", "Allow: GET, HEAD\r\n");
        send_all(conn, r.data(), r.size());
        access_log(method, route, 405);
        ::close(conn); return;
    }

    // Health check for load balancers / orchestrators (Docker, k8s, Fly.io).
    // A cheap 200 that doesn't render the app — answers "is the process up?".
    if (route == "/healthz" || route.rfind("/healthz?",0)==0) {
        auto r = http_response("200 OK", "text/plain; charset=utf-8", "ok", {}, head_only);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); ::close(conn); return;
    }

    // SEO plumbing files, served automatically. robots.txt tells crawlers they
    // may index everything and where the sitemap is; sitemap.xml lists the
    // routes the app declared (P::sitemap()). Both are optional — default robots
    // allows all. Cacheable (they change rarely).
    if (route == "/robots.txt" || route.rfind("/robots.txt?",0)==0) {
        std::string body = "User-agent: *\nAllow: /\n";
        if constexpr (requires { P::site_url(); }) body += "Sitemap: " + std::string(P::site_url()) + "/sitemap.xml\n";
        auto r = http_response("200 OK", "text/plain; charset=utf-8", body, {}, head_only, /*cache=*/true);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); ::close(conn); return;
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
        auto r = http_response("200 OK", "application/xml; charset=utf-8", body, {}, head_only, /*cache=*/true);
        send_all(conn, r.data(), r.size()); access_log(method, route, 200); ::close(conn); return;
    }
    // The dev-mode favicon: browsers auto-request /favicon.ico; answer it with a
    // 204 so it isn't SSR'd as the app (and doesn't 404-noise the logs).
    if (route == "/favicon.ico") {
        std::string r = "HTTP/1.1 204 No Content\r\n" + sec_headers() +
                        "Cache-Control: public, max-age=86400\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send_all(conn, r.data(), r.size()); access_log(method, route, 204); ::close(conn); return;
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
        + assets().head_html() +
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
#ifdef WAYA_GZIP
    if (accepts_gzip(req)) {
        std::string gz = gzip(doc);
        if (!gz.empty()) {
            http = http_response("200 OK", "text/html; charset=utf-8", gz,
                                 "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n", head_only);
        }
    }
#endif
    if (http.empty())
        http = http_response("200 OK", "text/html; charset=utf-8", doc, {}, head_only);
    send_all(conn, http.data(), http.size());
    access_log(method, route, status);
    ::close(conn);
}

} // namespace detail

/// Serve a Surface Program live. Thread-per-connection (one open client can't
/// block others). Blocks until Ctrl-C.
template <typename P>
int live(LiveConfig cfg = {}) {
    check_program<P>();   // readable diagnostics before anything else
    if (const char* p = std::getenv("WAYA_PORT")) cfg.port = std::atoi(p);
    if (const char* h = std::getenv("WAYA_HOST")) cfg.host = h;

    // Bind IPv6 dual-stack for the default all-interfaces case: bind `::` with
    // IPV6_V6ONLY off, so ONE socket accepts BOTH IPv4 and IPv6 clients. Many
    // proxies/tunnels/`localhost` resolvers reach the server over IPv6 (::1),
    // which an IPv4-only 0.0.0.0 socket refuses. A specific WAYA_HOST still uses
    // the exact IPv4 address given. Falls back to IPv4 if IPv6 is unavailable.
    bool all_ifaces = std::string(cfg.host) == "0.0.0.0" || std::string(cfg.host) == "::";
    int lfd = -1;
    if (all_ifaces) {
        lfd = ::socket(AF_INET6, SOCK_STREAM, 0);
        if (lfd >= 0) {
            int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
            int v6only = 0; ::setsockopt(lfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
            sockaddr_in6 a6{}; a6.sin6_family=AF_INET6; a6.sin6_port=htons((uint16_t)cfg.port); a6.sin6_addr=in6addr_any;
            if (::bind(lfd,(sockaddr*)&a6,sizeof(a6)) < 0) { ::close(lfd); lfd = -1; }  // fall back to IPv4
        }
    }
    if (lfd < 0) {   // specific host, or IPv6 unavailable → IPv4
        lfd = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)cfg.port);
        a.sin_addr.s_addr = all_ifaces ? INADDR_ANY : inet_addr(cfg.host);
        if (::bind(lfd,(sockaddr*)&a,sizeof(a))<0) { std::perror("waya: bind"); return 1; }
    }
    ::listen(lfd, 64);
    detail::g_fd = lfd;
    std::signal(SIGINT, detail::on_sigint); std::signal(SIGPIPE, SIG_IGN);
#ifdef SIGTERM
    std::signal(SIGTERM, detail::on_sigint);   // Docker/systemd stop -> clean exit
#endif

    // When bound to all interfaces, "http://0.0.0.0"/"[::]" isn't a browsable
    // address — open localhost locally, and ALSO print the LAN address so other
    // devices (a phone on the same wifi) know where to point.
    std::string open_host = all_ifaces ? "localhost" : std::string(cfg.host);
    std::string url = "http://" + open_host + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: surface app on %s  (Ctrl-C to stop)\n", url.c_str());
    if (all_ifaces) {
        std::string lan = detail::lan_ip();
        if (!lan.empty())
            std::fprintf(stderr, "waya: on your network at http://%s:%d\n", lan.c_str(), cfg.port);
    }
    if (cfg.open && !std::getenv("WAYA_NO_OPEN")) {
#if defined(_WIN32)
        std::system(("start \"\" \"" + url + "\"").c_str());
#elif defined(__APPLE__)
        std::system(("open '"+url+"' >/dev/null 2>&1 &").c_str());
#else
        std::system(("xdg-open '"+url+"' >/dev/null 2>&1 &").c_str());
#endif
    }
    for (;;) {
        int conn = ::accept(lfd, nullptr, nullptr);
        if (conn < 0) {
            if (detail::g_fd < 0) break;      // shutting down
            if (errno == EINTR) continue;     // interrupted by a signal, retry
            continue;                          // transient accept error, keep serving
        }
        std::thread([conn, port=cfg.port, bg=cfg.page_bg, title=cfg.title]{ detail::handle<P>(conn, port, bg, title); }).detach();
    }
    return 0;
}

} // namespace waya::surface
