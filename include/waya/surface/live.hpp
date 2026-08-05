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
#include "layout.hpp"
#include "dom.hpp"
#include "diff.hpp"
#include "wire.hpp"
#include "binary.hpp"
#include "effect.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Reuse the WebSocket codec from the DOM live runtime.
#include "../net/ws.hpp"

namespace waya::surface {

struct LiveConfig { int port = 8080; const char* host = "127.0.0.1"; bool open = true; };

/// A Surface Program: Model + Msg + init/update/view(->NodeRef). `update` may
/// be `update(Model, Msg)` (taps) OR `update(Model, Msg, std::string value)`
/// (inputs carry a value) — the runtime calls whichever you define.
///
/// Surface `Msg` must be an integer or an integer-backed enum: taps travel over
/// the WebSocket as integers, so the runtime converts Msg <-> int at the wire.
/// (Use a `std::variant` Msg with the DOM `waya::app` runtime, not this one.)
template <typename P>
concept SurfaceProgram =
    requires { typename P::Model; typename P::Msg; }
    && (std::integral<typename P::Msg> || std::is_enum_v<typename P::Msg>)
    && requires(const typename P::Model& m) { { P::view(m) } -> std::convertible_to<NodeRef>; };

namespace detail {

inline std::atomic<int> g_fd{-1};
inline void on_sigint(int){ int fd=g_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

/// Reserved message id for route deliveries. The wire never carries this from a
/// tap (taps are the app's own enum values, always >= 0 in practice); the
/// runtime injects it when a "@route|<path>" frame arrives and routes it through
/// the app's Sub::on_route handler. Chosen far from any plausible app enum.
inline constexpr int kRouteMsg = -0x7ACE;

/// A tiny blocking GET for Cmd::fetch. Absolute http:// URLs only; anything else
/// (or a network error) yields an empty body so the app's handler still fires.
/// Runs on a detached worker thread, never the model loop.
inline std::string http_get(const std::string& url) {
    auto pos = url.find("://");
    std::string rest = pos == std::string::npos ? url : url.substr(pos + 3);
    auto slash = rest.find('/');
    std::string host = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    int port = 80;
    if (auto c = host.find(':'); c != std::string::npos) {
        port = std::atoi(host.substr(c + 1).c_str()); host = host.substr(0, c);
    }
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return {};
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = inet_addr(host.c_str());
    if (a.sin_addr.s_addr == INADDR_NONE) a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::connect(fd, (sockaddr*)&a, sizeof(a)) < 0) { ::close(fd); return {}; }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host +
                      "\r\nConnection: close\r\n\r\n";
    ::send(fd, req.data(), req.size(), 0);
    std::string resp; char b[4096]; ssize_t r;
    while ((r = ::recv(fd, b, sizeof(b), 0)) > 0) resp.append(b, r);
    ::close(fd);
    auto hdr = resp.find("\r\n\r\n");
    return hdr == std::string::npos ? std::string{} : resp.substr(hdr + 4);
}

/// Call P::update and return (Model, Cmd). Supports FOUR update shapes so apps
/// range from trivial to full effectful, and the runtime doesn't care which:
///   update(Model, Msg)                       → no value, no effects
///   update(Model, Msg, std::string value)    → value (inputs), no effects
///   update(Model, Msg)         -> (Model,Cmd) → effects
///   update(Model, Msg, string) -> (Model,Cmd) → value + effects
template <typename P, typename Model, typename Msg>
std::pair<Model, Cmd<Msg>> dispatch(Model m, Msg msg, const std::string& value){
    using C = Cmd<Msg>;
    // 3-arg forms first (value-carrying), then 2-arg.
    if constexpr (requires(Model mm, Msg mg, std::string v){ { P::update(mm,mg,v) } -> std::convertible_to<std::pair<Model,C>>; }) {
        auto r = P::update(std::move(m), msg, value); return { std::move(r.first), std::move(r.second) };
    } else if constexpr (requires(Model mm, Msg mg, std::string v){ { P::update(mm,mg,v) } -> std::convertible_to<Model>; }) {
        return { P::update(std::move(m), msg, value), C::none() };
    } else if constexpr (requires(Model mm, Msg mg){ { P::update(mm,mg) } -> std::convertible_to<std::pair<Model,C>>; }) {
        auto r = P::update(std::move(m), msg); return { std::move(r.first), std::move(r.second) };
    } else {
        return { P::update(std::move(m), msg), C::none() };
    }
}

/// program_init returns (Model, Cmd) too — supports init()->Model or ->(Model,Cmd).
template <typename P, typename Model, typename Msg>
std::pair<Model, Cmd<Msg>> init_of(){
    if constexpr (requires{ { P::init() } -> std::convertible_to<std::pair<Model,Cmd<Msg>>>; }) {
        auto r = P::init(); return { std::move(r.first), std::move(r.second) };
    } else return { P::init(), Cmd<Msg>::none() };
}

/// program subscriptions — P::subscribe(Model)->Sub<Msg> if it exists, else none.
template <typename P, typename Model, typename Msg>
Sub<Msg> subs_of(const Model& m){
    if constexpr (requires{ { P::subscribe(m) } -> std::convertible_to<Sub<Msg>>; }) return P::subscribe(m);
    else return Sub<Msg>::none();
}

/// The terminal. Holds NO app state and NO app logic — it decodes packed binary
/// frames and paints them, coalescing all ops of a frame into a single
/// requestAnimationFrame so the DOM is touched once per frame (fewest paints).
/// One code path: decode → inject css → apply ops. A full paint is just an op
/// that repaints the root, so the terminal is trivially resyncable.
inline std::string client(int port) {
    return
    "<script>(function(){"
    "var R=document.getElementById('root'),S=document.getElementById('wsheet');"
    "var dec=new TextDecoder();"
    // — binary frame reader (LEB128 varints) → {css, ops:[[op,path,payload]]} —
    "function readFrame(buf){var b=new Uint8Array(buf),p=0;"
    "function vi(){var x=0,s=0,c;do{c=b[p++];x|=(c&0x7f)<<s;s+=7;}while(c&0x80);return x>>>0;}"
    "function str(){var n=vi(),o=p;p+=n;return dec.decode(b.subarray(o,o+n));}"
    "var css=str();var nop=vi();var ops=[];"
    "for(var i=0;i<nop;i++){var k=b[p++];var d=vi();var path='';"
    "for(var j=0;j<d;j++){path+=(j?'.':'')+vi();}var payload=(k===5)?'':str();ops.push([k,path,payload]);}"
    "return{css:css,ops:ops};}"
    "function frag(html){var d=document.createElement('div');d.innerHTML=html;return d.firstChild;}"
    "function at(p){var e=R.firstElementChild;if(p==='')return e;var q=p.split('.');"
    "for(var i=0;i<q.length;i++){e=e.childNodes[+q[i]];if(!e)return null;}return e;}"
    "function apply(op){var k=op[0],p=op[1],e=at(p);"
    "if(k===7){R.innerHTML=op[2];}"
    "else if(k===0){if(e)e.textContent=op[2];}"
    "else if(k===1||k===2||k===4){if(e)e.replaceWith(frag(op[2]));}"
    "else if(k===3){if(e){var f=frag(op[2]);if(e.tagName==='IMG')e.src=f.src;else e.replaceWith(f);}}"
    "else if(k===5){if(e)e.remove();}"
    "else if(k===6){var pa=at(p);if(pa)pa.appendChild(frag(op[2]));}}"
    // — rAF-coalesced paint: queue frames, apply them all in one animation frame —
    "var q=[],raf=0;"
    "function flush(){raf=0;var frames=q;q=[];"
    "for(var fi=0;fi<frames.length;fi++){var m=frames[fi];if(m.css)S.textContent+=m.css;"
    "for(var i=0;i<m.ops.length;i++)apply(m.ops[i]);}}"
    "function paint(m){q.push(m);if(!raf)raf=requestAnimationFrame(flush);}"
    "var ws,started=false;"
    "function route(){if(ws&&ws.readyState===1)ws.send('@route|'+location.pathname+location.search);}"
    "function connect(){ws=new WebSocket('ws://'+location.hostname+':"+std::to_string(port)+"');"
    "ws.binaryType='arraybuffer';"
    "ws.onopen=function(){if(started){S.textContent='';R.innerHTML='';}started=true;route();};"
    // Text frames are runtime control messages (navigation); binary frames are
    // paints. This keeps one socket doing input, output, and effects.
    "ws.onmessage=function(ev){if(typeof ev.data==='string'){ctl(ev.data);return;}paint(readFrame(ev.data));};"
    "ws.onclose=function(){setTimeout(connect,300);};ws.onerror=function(){try{ws.close()}catch(_){}}}"
    // control: "@nav|<url>" pushes history + re-routes; "@url|<url>" only syncs
    // the address bar (deep-link) without a route.
    "function ctl(s){var b=s.indexOf('|'),k=s.slice(0,b),v=s.slice(b+1);"
    "if(k==='@nav'){history.pushState({},'',v);route();}"
    "else if(k==='@rep'){history.replaceState({},'',v);route();}"
    "else if(k==='@url'){history.pushState({},'',v);}}"
    "window.addEventListener('popstate',route);"
    "connect();"
    "document.addEventListener('click',function(ev){var t=ev.target.closest('[data-tap]');"
    "if(t&&ws&&ws.readyState===1){ev.preventDefault();ws.send(t.dataset.tap);}});"
    // input events carry a value: send \"i<msg>|<value>\" (change: \"c<msg>|<value>\")
    "document.addEventListener('input',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.input!=null&&ws&&ws.readyState===1){ws.send('i'+t.dataset.input+'|'+t.value);}});"
    "document.addEventListener('change',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.change!=null&&ws&&ws.readyState===1){ws.send('c'+t.dataset.change+'|'+t.value);}});"
    "})();</script>";
}

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
    std::mutex wm;                     // serializes socket writes
    // Running interval subscriptions, keyed by (interval_ms, msg). Each has a
    // generation flag so reconciliation can stop the ones no longer subscribed.
    struct Timer { long ms; int msg; std::shared_ptr<std::atomic<bool>> run; };
    std::vector<Timer> timers;

    void push(int msg, std::string value = {}) {
        { std::lock_guard<std::mutex> l(qm); queue.push_back({msg, std::move(value)}); }
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
        if (::send(conn, frame.data(), frame.size(), MSG_NOSIGNAL) < 0) alive = false;
    }
    void send_text(const std::string& s) {
        if (!alive) return;
        auto f = ws::encode_text(s);
        std::lock_guard<std::mutex> l(wm);
        if (::send(conn, f.data(), f.size(), MSG_NOSIGNAL) < 0) alive = false;
    }
};

/// Interpret one Cmd. Effects that produce a message push it back into the
/// session queue (self-messaging); web effects send a control frame. This is
/// the runtime half of "effects are data" — the app returned a description, we
/// perform it here and nowhere else.
template <typename Msg>
void perform(const std::shared_ptr<Session>& s, const Cmd<Msg>& cmd) {
    std::visit(overload{
        [](const typename Cmd<Msg>::None&) {},
        [&](const typename Cmd<Msg>::Quit&) { s->stop(); },
        [&](const typename Cmd<Msg>::Batch& b) { for (auto& c : b.cmds) perform(s, c); },
        [&](const typename Cmd<Msg>::Emit& e) { s->push(static_cast<int>(e.msg)); },
        [&](const typename Cmd<Msg>::After& a) {
            int m = static_cast<int>(a.msg); long ms = a.delay.count();
            std::weak_ptr<Session> ws_ = s;
            std::thread([ws_, ms, m]{
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push(m);
            }).detach();
        },
        [&](const typename Cmd<Msg>::Task& t) {
            auto work = t.work; std::weak_ptr<Session> ws_ = s;
            std::thread([ws_, work]{
                Msg r = work();
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push(static_cast<int>(r));
            }).detach();
        },
        [&](const typename Cmd<Msg>::Fetch& f) {
            auto url = f.url; auto on = f.on_done; std::weak_ptr<Session> ws_ = s;
            std::thread([ws_, url, on]{
                std::string body = detail::http_get(url);
                Msg r = on(std::move(body));
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push(static_cast<int>(r));
            }).detach();
        },
        [&](const typename Cmd<Msg>::Navigate& n) {
            s->send_text((n.replace ? "@rep|" : "@nav|") + n.url);
        },
        [&](const typename Cmd<Msg>::PushUrl& p) { s->send_text("@url|" + p.url); },
    }, cmd.alt());
}

/// Reconcile the model's declared subscriptions against the timers currently
/// running: start newly-declared intervals, stop ones no longer wanted. Idempotent
/// — safe to call after every update, like maya diffing Subs between frames.
template <typename Msg>
void reconcile_subs(const std::shared_ptr<Session>& s, const Sub<Msg>& sub) {
    auto wanted = sub.timers();
    std::vector<Session::Timer> next;
    // Keep timers still wanted; mark which wanted ones are already running.
    std::vector<bool> matched(wanted.size(), false);
    for (auto& t : s->timers) {
        bool keep = false;
        for (std::size_t i = 0; i < wanted.size(); ++i) {
            if (matched[i]) continue;
            if (wanted[i].interval.count() == t.ms && static_cast<int>(wanted[i].msg) == t.msg) {
                matched[i] = true; keep = true; break;
            }
        }
        if (keep) next.push_back(t);
        else *t.run = false;   // signal the interval thread to exit
    }
    // Start any wanted timer not already running.
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        if (matched[i]) continue;
        long ms = wanted[i].interval.count(); int m = static_cast<int>(wanted[i].msg);
        auto run = std::make_shared<std::atomic<bool>>(true);
        next.push_back({ms, m, run});
        std::weak_ptr<Session> ws_ = s;
        std::thread([ws_, ms, m, run]{
            while (*run) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                if (!*run) break;
                auto sp = ws_.lock();
                if (!sp || !sp->alive) break;
                sp->push(m);
            }
        }).detach();
    }
    s->timers = std::move(next);
}

template <typename P>
void handle(int conn, int port) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    char buf[8192];
    ssize_t n = ::recv(conn, buf, sizeof(buf)-1, 0);
    if (n <= 0) { ::close(conn); return; }
    std::string_view req{buf, (size_t)n};

    if (auto resp = ws::try_handshake(req)) {
        ::send(conn, resp->data(), resp->size(), 0);

        auto s = std::make_shared<Session>();
        s->conn = conn;

        auto [model, init_cmd] = detail::init_of<P, Model, Msg>();
        NodeRef prev = P::view(model);

        // First frame: a full paint. Same shape as any later frame — a
        // reconnecting client is resynced by another full paint.
        s->send_binary(ws::encode_binary(encode_full(*prev)));
        detail::perform<Msg>(s, init_cmd);
        detail::reconcile_subs<Msg>(s, detail::subs_of<P, Model, Msg>(model));

        // Reader thread: decode the WebSocket and funnel messages into the
        // queue. Runs alongside the effect threads; the owner loop below owns
        // the model and drains everything. We JOIN it before closing the fd, so
        // the socket is never closed out from under a blocking recv().
        std::thread reader([s, conn]{
            std::string acc;
            for (;;) {
                char fb[8192];
                ssize_t r = ::recv(conn, fb, sizeof(fb), 0);
                if (r <= 0) break;
                acc.append(fb, r);
                // Bound the reassembly buffer: a peer that never completes a
                // frame can't make us allocate without limit.
                if (acc.size() > (1u << 20)) { break; }
                for (;;) {
                    std::size_t used = 0;
                    auto fr = ws::decode(acc, used);
                    if (!fr.ok) break;
                    acc.erase(0, used);
                    if (fr.opcode == 0x8) { s->stop(); return; }        // close
                    if (fr.opcode == 0x9) { s->send_binary(ws::encode_pong(fr.payload)); continue; }
                    if (fr.opcode != 0x1) continue;                     // ignore non-text

                    // Upstream messages: taps "<msg>"; inputs "i<msg>|<value>"
                    // / "c<msg>|<value>"; route "@route|<path>" (special msg).
                    const std::string& raw = fr.payload;
                    if (raw.rfind("@route|", 0) == 0) {
                        s->push(kRouteMsg, raw.substr(7));
                    } else if (!raw.empty() && (raw[0]=='i' || raw[0]=='c')) {
                        auto bar = raw.find('|');
                        int m = std::atoi(raw.substr(1, bar-1).c_str());
                        s->push(m, bar != std::string::npos ? raw.substr(bar+1) : std::string{});
                    } else if (!raw.empty()) {
                        // A bare tap is a signed integer msg id. Reject anything
                        // non-numeric so a malformed frame can't masquerade as
                        // msg 0 (atoi's silent failure).
                        char* end = nullptr;
                        long m = std::strtol(raw.c_str(), &end, 10);
                        if (end && *end == '\0') s->push(static_cast<int>(m));
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
            if (d->msg == kRouteMsg) {
                // Route changes flow through the app's on_route subscription,
                // computed from the CURRENT model (before this message).
                auto sub = detail::subs_of<P, Model, Msg>(model);
                auto* rt = sub.route();
                if (!rt) continue;   // app doesn't route: drop it
                r = detail::dispatch<P>(std::move(model), rt->route(d->value), std::string{});
            } else {
                r = detail::dispatch<P>(std::move(model), static_cast<Msg>(d->msg), d->value);
            }
            model = std::move(r.first);
            detail::perform<Msg>(s, r.second);

            NodeRef next = P::view(model);
            Patch patch = diff(prev, next);
            prev = next;
            if (!patch.empty())
                s->send_binary(ws::encode_binary(encode_delta(patch)));

            detail::reconcile_subs<Msg>(s, detail::subs_of<P, Model, Msg>(model));
            if (!s->alive) break;   // Cmd::quit or a dead socket: stop the loop.
        }
        // Orderly teardown: stop interval threads, unblock + join the reader,
        // then close the fd exactly once (no stale recv/send on a recycled fd).
        for (auto& t : s->timers) *t.run = false;
        s->shutdown_io();
        if (reader.joinable()) reader.join();
        ::close(conn);
        return;
    }

    // Initial HTML: a bare shell with #root, an empty <style id=wsheet> for the
    // app's stylesheet, and the client. The surface fills in over the socket.
    std::string doc =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "html,body{height:100%}"
        // A real sans-serif stack; force EVERY element (incl. form controls,
        // which don't inherit font by default) to use it — otherwise inputs and
        // buttons render in the UA's monospace/serif default.
        "body{font-family:ui-sans-serif,system-ui,-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;line-height:1.5;-webkit-font-smoothing:antialiased}"
        "*,input,button,textarea,select{font-family:inherit;font-size:inherit;line-height:inherit;color:inherit}"
        "input,button,textarea,select{border:0;background:none;outline:none}"
        "svg{display:block;max-width:100%}"
        "img{max-width:100%;height:auto}"
        // #root is the page surface: full viewport height. The app's own root
        // fills it (so its background covers the page) but its CHILDREN size to
        // their content — a `grow` card no longer absorbs the whole viewport.
        "#root{min-height:100vh;display:flex;flex-direction:column}"
        "#root>*{width:100%;flex:1 0 auto;align-content:flex-start}"
        "</style>"
        "<style id=\"wsheet\"></style>"
        "</head><body><div id=\"root\"></div>" + client(port) + "</body></html>";
    std::string http =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(doc.size()) + "\r\nConnection: close\r\n\r\n" + doc;
    ::send(conn, http.data(), http.size(), 0);
    ::close(conn);
}

} // namespace detail

/// Serve a Surface Program live. Thread-per-connection (one open client can't
/// block others). Blocks until Ctrl-C.
template <typename P>
    requires SurfaceProgram<P>
int live(LiveConfig cfg = {}) {
    if (const char* p = std::getenv("WAYA_PORT")) cfg.port = std::atoi(p);
    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)cfg.port); a.sin_addr.s_addr=inet_addr(cfg.host);
    if (::bind(lfd,(sockaddr*)&a,sizeof(a))<0) { std::perror("waya: bind"); return 1; }
    ::listen(lfd, 16);
    detail::g_fd = lfd;
    std::signal(SIGINT, detail::on_sigint); std::signal(SIGPIPE, SIG_IGN);

    std::string url = "http://" + std::string(cfg.host) + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: surface app on %s  (Ctrl-C to stop)\n", url.c_str());
    if (cfg.open && !std::getenv("WAYA_NO_OPEN")) {
#if defined(__APPLE__)
        std::system(("open '"+url+"' >/dev/null 2>&1 &").c_str());
#else
        std::system(("xdg-open '"+url+"' >/dev/null 2>&1 &").c_str());
#endif
    }
    for (;;) {
        int conn = ::accept(lfd, nullptr, nullptr);
        if (conn < 0) { if (detail::g_fd < 0) break; continue; }
        std::thread([conn, port=cfg.port]{ detail::handle<P>(conn, port); }).detach();
    }
    return 0;
}

} // namespace waya::surface
