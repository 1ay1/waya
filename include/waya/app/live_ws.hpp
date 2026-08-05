#pragma once
/// \file live_ws.hpp
/// The live runtime over a PERSISTENT WebSocket. maya's event loop, but the
/// connection stays open and patches STREAM to the client as they're produced —
/// no request-per-click. Each connected browser gets its OWN session (its own
/// Model + memo cache + prev VNode), so multiple clients are independent.
///
///   waya::live_ws<Counter>({.port = 8080});
///
/// The page ships a small client that opens a WebSocket, sends {msg id} on
/// click, and applies the JSON patch the server pushes back. This is the
/// production shape of the live loop (the HTTP-swap `live<P>()` remains for the
/// simplest cases / environments without WS).

#include "program.hpp"
#include "msg.hpp"
#include "../net/serve.hpp"
#include "../net/ws.hpp"
#include "../render/html.hpp"
#include "../render/vwalk.hpp"
#include "../render/diff.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace waya {

namespace detail {

/// The WS client. Robust by design: it auto-reconnects with backoff (so a
/// dropped socket — e.g. a `scripts/dev.sh` rebuild, a network blip — recovers
/// on its own), and on a successful RECONNECT it reloads the page to pick up any
/// new markup from a restarted server. First connect just wires up; only
/// reconnects reload. This is what makes dev.sh + counter "just work".
inline const char* live_ws_client(int port) {
    static std::string s;
    s =
    "<script>(function(){"
    "var url='ws://'+location.hostname+':" + std::to_string(port) + "';"
    "var ws,connected=false;"
    "function at(path){var el=document.getElementById('waya-root').firstElementChild;"
    "if(path==='')return el;var p=path.split('.');"
    // Server indexes ALL children (text + elements); the DOM's `children` skips
    // text nodes, so we must walk `childNodes` to stay in sync. waya emits no
    // whitespace between nodes, so childNodes matches the server's `kids` 1:1.
    "for(var i=0;i<p.length;i++){el=el.childNodes[+p[i]];if(!el)return null;}return el;}"
    "function apply(op){var k=op[0],path=op[1],a=op[2],b=op[3];var el=at(path);"
    "if(k===0){if(el)el.textContent=a;}"
    "else if(k===1){if(el)el.setAttribute(a,b);}"
    "else if(k===2){if(el)el.removeAttribute(a);}"
    "else if(k===3){if(el){var d=document.createElement('div');d.innerHTML=a;"
    "el.replaceWith(d.firstElementChild||document.createTextNode(a));}}"
    "else if(k===4){if(el)el.remove();}"
    "else if(k===5){var pa=at(path);if(pa){var d=document.createElement('div');"
    "d.innerHTML=a;pa.appendChild(d.firstElementChild||document.createTextNode(a));}}}"
    "function connect(){ws=new WebSocket(url);"
    "ws.onopen=function(){"
    // A reconnect (the server came back, e.g. after a rebuild): reload to get
    // the fresh page. The first connect just proceeds.
    "if(connected){location.reload();}connected=true;};"
    "ws.onmessage=function(e){var ops=JSON.parse(e.data);for(var i=0;i<ops.length;i++)apply(ops[i]);};"
    "ws.onclose=function(){setTimeout(connect,300);};"
    "ws.onerror=function(){try{ws.close();}catch(_){}}; }"
    "connect();"
    "document.addEventListener('click',function(e){"
    "var t=e.target.closest('[data-waya-msg]');"
    "if(t&&ws&&ws.readyState===1){e.preventDefault();"
    "ws.send(t.getAttribute('data-waya-msg'));}});"
    "})();</script>";
    return s.c_str();
}

inline std::atomic<int> g_ws_fd{-1};
inline void ws_sigint(int){ int fd=g_ws_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

/// Handle ONE accepted connection to completion (its own thread). Serves either
/// a WebSocket streaming session or the initial HTML page. All session state is
/// local; thread_local msg-table / memo cache keep sessions isolated.
template <typename P>
inline void handle_connection(int conn, int port) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    char buf[8192];
    ssize_t n = ::recv(conn, buf, sizeof(buf)-1, 0);
    if (n <= 0) { ::close(conn); return; }
    std::string_view req{buf, (size_t)n};

    // WebSocket upgrade → a streaming session.
    if (auto resp = ws::try_handshake(req)) {
        ::send(conn, resp->data(), resp->size(), 0);

        Model model = program_init<P>().first;
        render::MemoCache memo;
        vdom::VNode prev; bool have_prev = false;

        auto rebuild = [&](style::StyleSheet& sheet) {
            app::detail::begin_msg_capture<Msg>();
            render::active_memo = &memo; render::memo_builds = 0;
            auto node = P::view(model);
            auto v = render::to_vnode(node, sheet);
            memo.rotate(); render::active_memo = nullptr;
            return v;
        };
        { style::StyleSheet s; prev = rebuild(s); have_prev = true; }

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
            if (fr.opcode == 0x8) break;                 // close
            if (fr.opcode == 0x9) { auto p=ws::encode_pong(fr.payload);
                ::send(conn,p.data(),p.size(),0); continue; }
            if (fr.opcode != 0x1) continue;

            if (auto msg = app::detail::lookup_msg<Msg>(fr.payload)) {
                auto [m2, cmd] = P::update(std::move(model), *msg);
                model = std::move(m2); (void)cmd;
            }
            style::StyleSheet sheet;
            auto next = rebuild(sheet);
            auto patch = have_prev ? vdom::diff(prev, next) : vdom::Patch{};
#ifndef NDEBUG
            {   vdom::VNode check = prev; vdom::apply(check, patch);
                if (have_prev && vdom::vnode_to_html(check) != vdom::vnode_to_html(next))
                    std::fprintf(stderr, "waya: BUG — patch/render divergence!\n"); }
#endif
            prev = std::move(next); have_prev = true;
            std::string frame = ws::encode_text(vdom::to_json(patch));
            ::send(conn, frame.data(), frame.size(), 0);
        }
        ::close(conn);
        return;
    }

    // Otherwise: the initial HTML page (with the WS client embedded).
    Model model = program_init<P>().first;
    render::MemoCache memo; render::active_memo = &memo; render::memo_builds = 0;
    app::detail::begin_msg_capture<Msg>();
    auto node = P::view(model);
    style::StyleSheet sheet; std::string html;
    render::detail::walk(html, sheet, node);
    memo.rotate(); render::active_memo = nullptr;
    std::string css = sheet.render();
    std::string styleTag = css.empty() ? "" : "<style>"+css+"</style>";
    std::string doc =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\">" + styleTag +
        "</head><body><div id=\"waya-root\">" + html + "</div>" +
        live_ws_client(port) + "</body></html>";
    std::string http =
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(doc.size()) + "\r\n"
        "Connection: close\r\n\r\n" + doc;
    ::send(conn, http.data(), http.size(), 0);
    ::close(conn);
}

} // namespace detail

/// Run `P` as a live WebSocket app. One accept loop; each connection is a
/// self-contained session handled to completion (blocking, one at a time \u2014 the
/// coroutine reactor that makes this massively concurrent is the next step).
template <typename P>
    requires Program<P>
int live_ws(ServeConfig cfg = {}) {
    using Model = typename P::Model;
    using Msg   = typename P::Msg;

    if (const char* p = std::getenv("WAYA_PORT")) cfg.port = std::atoi(p);

    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons((uint16_t)cfg.port);
    addr.sin_addr.s_addr = inet_addr(cfg.host);
    if (::bind(lfd,(sockaddr*)&addr,sizeof(addr))<0){ std::perror("waya: bind"); return 1; }
    ::listen(lfd, 16);
    detail::g_ws_fd = lfd;
    std::signal(SIGINT, detail::ws_sigint);
    std::signal(SIGPIPE, SIG_IGN);

    std::string url = "http://" + std::string(cfg.host) + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: live (WebSocket) on %s  (Ctrl-C to stop)\n", url.c_str());
    if (cfg.open && !std::getenv("WAYA_NO_OPEN")) {
#if defined(__APPLE__)
        std::system(("open '"+url+"' >/dev/null 2>&1 &").c_str());
#else
        std::system(("xdg-open '"+url+"' >/dev/null 2>&1 &").c_str());
#endif
    }

    for (;;) {
        int conn = ::accept(lfd, nullptr, nullptr);
        if (conn < 0) { if (detail::g_ws_fd < 0) break; continue; }

        // Handle each connection on its OWN thread. This is what stops a single
        // open WebSocket from blocking the whole server (the "stuck on loading"
        // bug: the accept loop was trapped inside one long-lived WS read loop,
        // so new tabs/refreshes never got served). thread_local msg-table and
        // memo cache make each session naturally isolated — no locks needed.
        std::thread([conn, port = cfg.port]{
            detail::handle_connection<P>(conn, port);
        }).detach();
    }
    return 0;
}

} // namespace waya
