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
#include "meta.hpp"

#include <atomic>
#include <algorithm>
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
#include <unordered_map>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef WAYA_GZIP
#include <zlib.h>
#endif

// Reuse the WebSocket codec from the DOM live runtime.
#include "../net/ws.hpp"

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
template <typename P>
concept SurfaceProgram =
    requires { typename P::Model; typename P::Msg; }
    && requires(const typename P::Model& m) { { P::view(m) } -> std::convertible_to<NodeRef>; };

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

/// program SEO metadata — P::meta(Model)->Meta if it exists, else a blank Meta
/// (the shell then uses its default title and index,follow robots).
template <typename P, typename Model>
Meta meta_of(const Model& m){
    if constexpr (requires{ { P::meta(m) } -> std::convertible_to<Meta>; }) return P::meta(m);
    else return Meta{};
}

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
template <typename P, typename Model>
NodeRef safe_view(const Model& m){
    try { return P::view(m); }
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
    "for(var j=0;j<d;j++){path+=(j?'.':'')+vi();}"
    // payload shape depends on op: remove(5) none; move(8) [from,to];
    // insert_at(9) [to,html]; everything else a single html/text string.
    "var payload;"
    "if(k===5){payload='';}"
    "else if(k===8){payload=[vi(),vi()];}"
    "else if(k===9){var to=vi();payload=[to,str()];}"
    "else{payload=str();}"
    "ops.push([k,path,payload]);}"
    "return{css:css,ops:ops};}"
    "function frag(html){var d=document.createElement('div');d.innerHTML=html;return d.firstChild;}"
    "function at(p){var e=R.firstElementChild;if(p==='')return e;var q=p.split('.');"
    "for(var i=0;i<q.length;i++){e=e.childNodes[+q[i]];if(!e)return null;}return e;}"
    "function parent(p){var q=p.split('.');q.pop();return at(q.join('.'));}"
    // Copy attributes from `nw` onto the live element `e`, deleting stale ones.
    // Reuses the SAME DOM node, so focus, caret and selection survive an update.
    "function morphAttrs(e,nw){"
    "for(var i=e.attributes.length-1;i>=0;i--){var a=e.attributes[i].name;if(!nw.hasAttribute(a))e.removeAttribute(a);}"
    "for(var j=0;j<nw.attributes.length;j++){var b=nw.attributes[j];if(e.getAttribute(b.name)!==b.value)e.setAttribute(b.name,b.value);}}"
    // Is `e` a form control the user could be editing right now?
    "function editable(e){var t=e.tagName;return t==='INPUT'||t==='TEXTAREA'||t==='SELECT';}"
    // In-place update of a control: morph attrs, but DON'T fight the user for
    // the value of the field they're focused in (the DOM already holds it).
    "function morphControl(e,nw){var focused=(document.activeElement===e);morphAttrs(e,nw);"
    "if(!focused){if(nw.tagName==='SELECT'){e.value=nw.value;}"
    "else if('value'in nw&&e.value!==nw.value){e.value=nw.value;}"
    "if('checked'in nw&&e.checked!==nw.checked)e.checked=nw.checked;"
    "if('textContent'in nw&&nw.tagName==='TEXTAREA'&&e.value!==nw.value)e.value=nw.value;}}"
    "function apply(op){var k=op[0],p=op[1],e=at(p);"
    "if(k===7){R.innerHTML=op[2];}"
    "else if(k===0){if(e)e.textContent=op[2];}"
    // set_paint(1): a NODE-LEVEL change (style/tap/control value). Morph the
    // live element in place so focus/caret survive; the node's CHILDREN are
    // diffed by their own deeper ops, so we only touch attrs here (+ the value
    // for a leaf control). Fall back to replace only if the tag changed.
    "else if(k===1){if(e){var nw=frag(op[2]);"
    "if(!nw||nw.tagName!==e.tagName){if(e&&nw)e.replaceWith(nw);}"
    "else if(editable(e)){morphControl(e,nw);}"
    "else{morphAttrs(e,nw);}}}"
    "else if(k===2||k===4){if(e)e.replaceWith(frag(op[2]));}"
    "else if(k===3){if(e){var f=frag(op[2]);if(e.tagName==='IMG')e.src=f.src;else e.replaceWith(f);}}"
    "else if(k===5){if(e)e.remove();}"
    "else if(k===6){var pa=at(p);if(pa)pa.appendChild(frag(op[2]));}"
    // insert_at: op[2]=[to,html]; insert BEFORE the current child at `to`.
    "else if(k===9){var pa=at(p);if(pa){var nd=frag(op[2][1]);var ref=pa.childNodes[op[2][0]];pa.insertBefore(nd,ref||null);}}"
    // move: op[2]=[from,to]; detach the child at `from`, reinsert before `to`.
    "else if(k===8){var pa=at(p);if(pa){var from=op[2][0],to=op[2][1];var nd=pa.childNodes[from];if(nd){nd.remove();var ref=pa.childNodes[to];pa.insertBefore(nd,ref||null);}}}}"
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
    // input/change carry a payload. Checkboxes & radios send their checked
    // state ("true"/"false"); every other control sends its value. So one path
    // serves text, textarea, select, checkbox and radio uniformly.
    "function payload(t){return (t.type==='checkbox'||t.type==='radio')?String(t.checked):t.value;}"
    "document.addEventListener('input',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.input!=null&&ws&&ws.readyState===1){ws.send('i'+t.dataset.input+'|'+payload(t));}});"
    "document.addEventListener('change',function(ev){var t=ev.target;"
    "if(t.dataset&&t.dataset.change!=null&&ws&&ws.readyState===1){ws.send('c'+t.dataset.change+'|'+payload(t));}});"
    // Generic events wired via data-ev-<type>="<msg>[|<arg>]". One delegated
    // listener per type; `e<msg>|<payload>` goes up. Keyboard events carry the
    // key as payload and honor an arg filter (on_key("Enter",..)); form submit
    // serialises the form's named fields; drop carries the dragged payload.
    "function evattr(el,type){return el&&el.dataset?el.dataset['ev'+type[0].toUpperCase()+type.slice(1)]:null;}"
    "function sendev(spec,pl){if(!ws||ws.readyState!==1)return;var bar=spec.indexOf('|');var msg=bar<0?spec:spec.slice(0,bar);ws.send('e'+msg+'|'+pl);}"
    "function evMatch(spec,key){var bar=spec.indexOf('|');if(bar<0)return true;return spec.slice(bar+1)===key;}"
    // keydown: walk up to the nearest node wiring keydown, honor the key filter.
    "document.addEventListener('keydown',function(ev){var t=ev.target;while(t&&t!==document){var s=evattr(t,'keydown');"
    "if(s!=null&&evMatch(s,ev.key)){ev.preventDefault();sendev(s,ev.key);return;}t=t.parentElement;}});"
    // focus/blur are non-bubbling → capture phase.
    "document.addEventListener('focus',function(ev){var s=evattr(ev.target,'focus');if(s!=null)sendev(s,payload(ev.target)||'');},true);"
    "document.addEventListener('blur',function(ev){var s=evattr(ev.target,'blur');if(s!=null)sendev(s,payload(ev.target)||'');},true);"
    // pointer enter/leave (capture; non-bubbling).
    "document.addEventListener('pointerenter',function(ev){var s=evattr(ev.target,'pointerenter');if(s!=null)sendev(s,'');},true);"
    "document.addEventListener('pointerleave',function(ev){var s=evattr(ev.target,'pointerleave');if(s!=null)sendev(s,'');},true);"
    // form submit: gather named fields into a=1&b=2 (URL-encoded).
    "document.addEventListener('submit',function(ev){var f=ev.target.closest('[data-ev-submit]');if(!f)return;ev.preventDefault();"
    "var d=new FormData(f),ps=[];d.forEach(function(v,k){ps.push(encodeURIComponent(k)+'='+encodeURIComponent(v));});"
    "sendev(f.dataset.evSubmit,ps.join('&'));});"
    // drag & drop: dragstart stashes the source's payload (its name attr);
    // dragover allows the drop; drop delivers "<dragged>:<target-arg>" so the app
    // learns WHAT was dropped and WHERE (the target's data-drop-arg, e.g. a column).
    "document.addEventListener('dragstart',function(ev){var t=ev.target.closest('[draggable=true]');if(t){ev.dataTransfer.setData('text/plain',t.getAttribute('name')||'');ev.dataTransfer.effectAllowed='move';}});"
    "document.addEventListener('dragover',function(ev){if(ev.target.closest&&ev.target.closest('[data-ev-drop]'))ev.preventDefault();});"
    "document.addEventListener('drop',function(ev){var t=ev.target.closest('[data-ev-drop]');if(t){ev.preventDefault();var src=ev.dataTransfer.getData('text/plain');var arg=t.getAttribute('data-drop-arg');sendev(t.dataset.evDrop,arg!=null?src+':'+arg:src);}});"
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
            std::thread([ws_, work]{
                Msg r = work();
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(std::any{r});
            }).detach();
        },
        [&](const typename Cmd<Msg>::Fetch& f) {
            auto url = f.url; auto on = f.on_done; std::weak_ptr<Session> ws_ = s;
            std::thread([ws_, url, on]{
                std::string body = detail::http_get(url);
                Msg r = on(std::move(body));
                if (auto sp = ws_.lock(); sp && sp->alive) sp->push_msg(std::any{r});
            }).detach();
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
        return (std::uint64_t)e.interval.count() * 1099511628211ull ^ (std::uint64_t)(std::uint32_t)detail::value_token<Msg>(e.msg);
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

    char buf[8192];
    ssize_t n = ::recv(conn, buf, sizeof(buf)-1, 0);
    if (n <= 0) { ::close(conn); return; }
    std::string_view req{buf, (size_t)n};

    if (auto resp = ws::try_handshake(req)) {
        ::send(conn, resp->data(), resp->size(), 0);

        auto s = std::make_shared<Session>();
        s->conn = conn;

        auto [model, init_cmd] = detail::init_of<P, Model, Msg>();
        detail::begin_msg_capture();
        NodeRef prev = detail::safe_view<P>(model);

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
                        s->push_route(raw.substr(7));
                    } else if (!raw.empty() && (raw[0]=='i' || raw[0]=='c' || raw[0]=='e')) {
                        // i/c: input/change value; e: a generic wired event
                        // (keyboard/focus/submit/drop) — all carry "<token>|<payload>".
                        auto bar = raw.find('|');
                        int tok = std::atoi(raw.substr(1, bar-1).c_str());
                        s->push_wire(tok, bar != std::string::npos ? raw.substr(bar+1) : std::string{});
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
            if (!handled) { model = std::move(model); continue; }
            model = std::move(r.first);
            detail::perform<Msg>(s, r.second);

            detail::begin_msg_capture();     // fresh msg registry for this render
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
        s->shutdown_io();
        if (reader.joinable()) reader.join();
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

    // SEO plumbing files, served automatically. robots.txt tells crawlers they
    // may index everything and where the sitemap is; sitemap.xml lists the
    // routes the app declared (P::sitemap()). Both are optional — default robots
    // allows all.
    if (route == "/robots.txt" || route.rfind("/robots.txt?",0)==0) {
        std::string body = "User-agent: *\nAllow: /\n";
        if constexpr (requires { P::site_url(); }) body += "Sitemap: " + std::string(P::site_url()) + "/sitemap.xml\n";
        std::string http = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        ::send(conn, http.data(), http.size(), 0); ::close(conn); return;
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
        std::string http = "HTTP/1.1 200 OK\r\nContent-Type: application/xml\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        ::send(conn, http.data(), http.size(), 0); ::close(conn); return;
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
    NodeRef ssr_root = detail::safe_view<P>(ssr_model);   // captures tokens into a fresh table
    auto ssr = DomBackend{}.render(*ssr_root);   // {html, css}

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
        + head_seo +
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
        // Respect the user's reduced-motion preference — accessibility, by default.
        "@media(prefers-reduced-motion:reduce){*{animation-duration:.001ms!important;animation-iteration-count:1!important;transition-duration:.001ms!important}}"
        "</style>"
        "<style id=\"wsheet\">" + ssr.css + "</style>"
        "</head><body><div id=\"root\">" + ssr.html + "</div>" + client(port) + "</body></html>";
    std::string http;
#ifdef WAYA_GZIP
    if (accepts_gzip(req)) {
        std::string gz = gzip(doc);
        if (!gz.empty()) {
            http = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                   "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n"
                   "Content-Length: " + std::to_string(gz.size()) + "\r\nConnection: close\r\n\r\n" + gz;
        }
    }
#endif
    if (http.empty())
        http = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
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
    if (const char* h = std::getenv("WAYA_HOST")) cfg.host = h;
    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)cfg.port); a.sin_addr.s_addr=inet_addr(cfg.host);
    if (::bind(lfd,(sockaddr*)&a,sizeof(a))<0) { std::perror("waya: bind"); return 1; }
    ::listen(lfd, 16);
    detail::g_fd = lfd;
    std::signal(SIGINT, detail::on_sigint); std::signal(SIGPIPE, SIG_IGN);

    // When bound to 0.0.0.0 (all interfaces), "http://0.0.0.0" isn't a browsable
    // address — open localhost locally, and ALSO print the LAN address so other
    // devices (a phone on the same wifi) know where to point.
    bool all_ifaces = std::string(cfg.host) == "0.0.0.0";
    std::string open_host = all_ifaces ? "localhost" : std::string(cfg.host);
    std::string url = "http://" + open_host + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: surface app on %s  (Ctrl-C to stop)\n", url.c_str());
    if (all_ifaces) {
        std::string lan = detail::lan_ip();
        if (!lan.empty())
            std::fprintf(stderr, "waya: on your network at http://%s:%d\n", lan.c_str(), cfg.port);
    }
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
        std::thread([conn, port=cfg.port, bg=cfg.page_bg, title=cfg.title]{ detail::handle<P>(conn, port, bg, title); }).detach();
    }
    return 0;
}

} // namespace waya::surface
