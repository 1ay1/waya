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

/// The client: opens a WS, injects the app's stylesheet, builds the initial
/// HTML into #root, then applies HTML-fragment patches by path. Dumb by design
/// — no style logic in JS; the server's DOM backend did it all. Reconnects on
/// drop (and reloads after a rebuild).
inline std::string client(int port) {
    return
    "<script>(function(){"
    "var R=document.getElementById('root'),S=document.getElementById('wsheet');"
    "function css(c){if(c)S.textContent=c;}"
    // resolve a dotted path to a node under #root's single child (all childNodes)
    "function at(p){var e=R.firstElementChild;if(p==='')return e;var q=p.split('.');"
    "for(var i=0;i<q.length;i++){e=e.childNodes[+q[i]];if(!e)return null;}return e;}"
    "function frag(html){var d=document.createElement('div');d.innerHTML=html;return d.firstChild;}"
    "function apply(op){var k=op[0],p=op[1],e=at(p);"
    "if(k===0){if(e)e.textContent=op[2];}"                        // set_text
    "else if(k===1){if(e)e.replaceWith(frag(op[2]));}"           // set_paint→replace subtree
    "else if(k===2){if(e)e.replaceWith(frag(op[2]));}"           // set_path
    "else if(k===3){if(e){var f=frag(op[2]);if(e.tagName==='IMG')e.src=f.src;else e.replaceWith(f);}}" // set_src
    "else if(k===4){if(e)e.replaceWith(frag(op[2]));}"           // replace
    "else if(k===5){if(e)e.remove();}"                           // remove
    "else if(k===6){var pa=at(p);if(pa)pa.appendChild(frag(op[2]));}}" // insert
    "var ws,started=false;"
    "function connect(){ws=new WebSocket('ws://'+location.hostname+':"+std::to_string(port)+"');"
    "ws.onopen=function(){if(started)location.reload();started=true;};"
    "ws.onmessage=function(ev){var m=JSON.parse(ev.data);"
    "if(m.init!==undefined){css(m.css);R.innerHTML=m.init;}"     // first frame: css + html
    "else{css(S.textContent+(m.css||''));for(var i=0;i<m.ops.length;i++)apply(m.ops[i]);}};" // deltas
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

        // First frame: render the whole surface with the DOM backend and send
        // {"init": html, "css": stylesheet}.
        {
            DomBackend dom; auto out = dom.render(*prev);
            std::string msg = "{\"init\":"; detail::jstr(msg, out.html);
            msg += ",\"css\":"; detail::jstr(msg, out.css); msg += "}";
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

    // Initial HTML: a bare shell with #root, an empty <style id=wsheet> for the
    // app's stylesheet, and the client. The surface fills in over the socket.
    std::string doc =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>*{box-sizing:border-box;margin:0}body{font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif}</style>"
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
