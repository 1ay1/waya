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
#include "dom.hpp"
#include "diff.hpp"
#include "wire.hpp"

#include <atomic>
#include <csignal>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

/// A Surface Program: Model + Msg + init/update/view(->NodeRef).
template <typename P>
concept SurfaceProgram = requires(typename P::Model m, typename P::Msg msg) {
    typename P::Model; typename P::Msg;
    { P::init() } -> std::convertible_to<typename P::Model>;
    { P::update(m, msg) } -> std::convertible_to<typename P::Model>;
    { P::view(std::as_const(m)) } -> std::convertible_to<NodeRef>;
};

namespace detail {

inline std::atomic<int> g_fd{-1};
inline void on_sigint(int){ int fd=g_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

/// The client: opens a WS, builds the initial surface into #root from the node
/// JSON, applies streamed patches, and forwards taps. One small renderer maps
/// surface nodes → DOM (the client's chosen backend). Reconnects on drop.
inline std::string client(int port) {
    return
    "<script>(function(){"
    "var R=document.getElementById('root');"
    // build a DOM element from a surface node
    "function el(n){var e;var k=n.k;"
    "if(k===1){e=document.createElement('span');e.textContent=n.t||'';}"
    "else if(k===2){e=document.createElement('img');e.src=n.s||'';}"
    "else if(k===3){e=document.createElementNS('http://www.w3.org/2000/svg','svg');"
    "var pl=document.createElementNS('http://www.w3.org/2000/svg','polyline');"
    "pl.setAttribute('points',(n.p||[]).map(function(p){return p[0]+','+p[1]}).join(' '));"
    "var pt=n.pt||{};pl.setAttribute('fill',n.cl?col(pt.bg):'none');"
    "pl.setAttribute('stroke',col(pt.fg));pl.setAttribute('stroke-width',(pt.sz||16)/8);"
    "e.appendChild(pl);}"
    "else{e=document.createElement('div');(n.c||[]).forEach(function(c){e.appendChild(el(c))});}"
    "style(e,n);return e;}"
    "function col(v){if(v==null)return 'none';return '#'+('000000'+(v>>>0).toString(16)).slice(-6);}"
    "function style(e,n){var p=n.pt||{},s=e.style;"
    "if(n.f===1){s.display='flex';s.flexDirection='row';}"
    "else if(n.f===2){s.display='flex';s.flexDirection='column';}"
    "if(n.k===1){s.color=col(p.fg);s.fontSize=(p.sz||16)+'px';if(p.b)s.fontWeight='700';}"
    "if(p.bg!=null)s.background=col(p.bg);"
    "if(p.r)s.borderRadius=p.r+'px';if(p.pd)s.padding=p.pd+'px';"
    "if(p.gp)s.gap=p.gp+'px';if(p.gr)s.flex=p.gr+' 1 0';"
    "if(n.tap!=null){s.cursor='pointer';e.dataset.tap=n.tap;}}"
    // resolve a dotted path to a DOM node under #root's single child
    "function at(path){var e=R.firstElementChild;if(path==='')return e;"
    "var q=path.split('.');for(var i=0;i<q.length;i++){e=e.childNodes[+q[i]];if(!e)return null;}return e;}"
    "function apply(op){var k=op[0],path=op[1];var e=at(path);"
    "if(k===0){if(e)e.textContent=op[2];}"                       // set_text
    "else if(k===1){if(e)style(e,op[2]);}"                       // set_paint: restyle in place (keep kids)
    "else if(k===2){if(e){var pl=e.firstChild;if(pl)pl.setAttribute('points',(op[2].p||[]).map(function(p){return p[0]+','+p[1]}).join(' '));}}" // set_path
    "else if(k===3){if(e)e.src=op[2].s||'';}"                    // set_src
    "else if(k===4){if(e)e.replaceWith(el(op[2]));}"             // replace
    "else if(k===5){if(e)e.remove();}"                           // remove
    "else if(k===6){var pa=at(path);if(pa)pa.appendChild(el(op[2]));}}" // insert
    "var ws,started=false;"
    "function connect(){ws=new WebSocket('ws://'+location.hostname+':"+std::to_string(port)+"');"
    "ws.onopen=function(){if(started)location.reload();started=true;};"
    "ws.onmessage=function(ev){var m=JSON.parse(ev.data);"
    "if(m.init){R.innerHTML='';R.appendChild(el(m.init));}"       // first frame: full surface
    "else{for(var i=0;i<m.length;i++)apply(m[i]);}};"            // deltas
    "ws.onclose=function(){setTimeout(connect,300);};ws.onerror=function(){try{ws.close()}catch(_){}}}"
    "connect();"
    "document.addEventListener('click',function(ev){var t=ev.target.closest('[data-tap]');"
    "if(t&&ws&&ws.readyState===1){ev.preventDefault();ws.send(t.dataset.tap);}});"
    "})();</script>";
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

        Model model = P::init();
        NodeRef prev = P::view(model);

        // First frame: send the whole surface as {"init": <node>}.
        {
            std::string msg = "{\"init\":"; node_json(msg, *prev); msg += "}";
            auto f = ws::encode_text(msg);
            ::send(conn, f.data(), f.size(), 0);
        }

        std::string acc;
        for (;;) {
            char fb[8192];
            ssize_t r = ::recv(conn, fb, sizeof(fb), 0);
            if (r <= 0) break;
            acc.append(fb, r);
            std::size_t used = 0;
            auto fr = ws::decode(acc, used);
            if (!fr.ok) continue;
            acc.erase(0, used);
            if (fr.opcode == 0x8) break;
            if (fr.opcode == 0x9) { auto p=ws::encode_pong(fr.payload); ::send(conn,p.data(),p.size(),0); continue; }
            if (fr.opcode != 0x1) continue;

            // The payload is a tap message index.
            Msg msg = static_cast<Msg>(std::atoi(fr.payload.c_str()));
            model = P::update(std::move(model), msg);
            NodeRef next = P::view(model);
            Patch patch = diff(prev, next);
            prev = next;
            auto f = ws::encode_text(patch_json(patch));
            ::send(conn, f.data(), f.size(), 0);
        }
        ::close(conn);
        return;
    }

    // Initial HTML: a bare page with #root and the client. The surface fills in
    // over the socket — the page itself contains no app markup.
    std::string doc =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<style>body{margin:0;font-family:ui-sans-serif,system-ui,sans-serif}</style>"
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
