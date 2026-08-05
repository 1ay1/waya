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

/// A Surface Program: Model + Msg + init/update/view(->NodeRef). `update` may
/// be `update(Model, Msg)` (taps) OR `update(Model, Msg, std::string value)`
/// (inputs carry a value) — the runtime calls whichever you define.
template <typename P>
concept SurfaceProgram =
    requires { typename P::Model; typename P::Msg; }
    && requires(typename P::Model m) { { P::init() } -> std::convertible_to<typename P::Model>; }
    && requires(const typename P::Model& m) { { P::view(m) } -> std::convertible_to<NodeRef>; }
    && ( requires(typename P::Model m, typename P::Msg msg) { { P::update(m, msg) } -> std::convertible_to<typename P::Model>; }
      || requires(typename P::Model m, typename P::Msg msg, std::string v) { { P::update(m, msg, v) } -> std::convertible_to<typename P::Model>; } );

namespace detail {

inline std::atomic<int> g_fd{-1};
inline void on_sigint(int){ int fd=g_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

/// Call P::update with a value when the Program supports it, else without.
/// This lets `update(Model, Msg)` and `update(Model, Msg, std::string value)`
/// both work — taps use the 2-arg form, inputs the 3-arg form.
template <typename P, typename Model, typename Msg>
Model dispatch(Model m, Msg msg, const std::string& value){
    if constexpr (requires(Model mm, Msg mg, std::string v){ P::update(mm, mg, v); })
        return P::update(std::move(m), msg, value);
    else
        return P::update(std::move(m), msg);
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
    "function connect(){ws=new WebSocket('ws://'+location.hostname+':"+std::to_string(port)+"');"
    "ws.binaryType='arraybuffer';"
    "ws.onopen=function(){if(started){S.textContent='';R.innerHTML='';}started=true;};"
    "ws.onmessage=function(ev){paint(readFrame(ev.data));};"
    "ws.onclose=function(){setTimeout(connect,300);};ws.onerror=function(){try{ws.close()}catch(_){}}}"
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

        // First frame: a full paint, packed binary. Same shape as any later
        // frame — the terminal doesn't know it's "first", and a reconnecting
        // client is resynced by sending it another full paint.
        {
            auto f = ws::encode_binary(encode_full(*prev));
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

            // Parse the upstream message. Taps: "<msg>". Input/change events:
            // "i<msg>|<value>" / "c<msg>|<value>" — the value rides along.
            const std::string& raw = fr.payload;
            Msg msg{}; std::string value;
            if (!raw.empty() && (raw[0]=='i' || raw[0]=='c')) {
                auto bar = raw.find('|');
                msg = static_cast<Msg>(std::atoi(raw.substr(1, bar-1).c_str()));
                if (bar != std::string::npos) value = raw.substr(bar+1);
            } else {
                msg = static_cast<Msg>(std::atoi(raw.c_str()));
            }
            model = detail::dispatch<P>(std::move(model), msg, value);
            NodeRef next = P::view(model);
            Patch patch = diff(prev, next);
            prev = next;
            auto f = ws::encode_binary(encode_delta(patch));
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
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "html,body{height:100%}"
        // A real sans-serif stack; force EVERY element (incl. form controls,
        // which don't inherit font by default) to use it — otherwise inputs and
        // buttons render in the UA's monospace/serif default.
        "body{font-family:ui-sans-serif,system-ui,-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;line-height:1.5;-webkit-font-smoothing:antialiased}"
        "*,input,button,textarea,select{font-family:inherit;font-size:inherit;line-height:inherit;color:inherit}"
        "input,button,textarea,select{border:0;background:none;outline:none}"
        "svg{display:block;overflow:visible}"
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
