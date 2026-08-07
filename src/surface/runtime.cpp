/// \file runtime.cpp
/// The non-templated transport: socket setup + hardening + accept loop, compiled
/// once into waya_runtime. See runtime.hpp for the seam.

#include "waya/surface/runtime.hpp"
#include "waya/surface/http_util.hpp"   // detail::g_fd, on_sigint, lan_ip

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>   // TCP_NODELAY
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>

namespace waya::surface {
namespace detail {

void tune_conn(int conn) {
    // TCP_NODELAY: live diffs are small frames; disabling Nagle removes the
    // up-to-40ms coalescing delay so a tap feels instant.
    int one = 1;
    ::setsockopt(conn, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    // Bound how long a single blocking recv/send may stall on one client, so a
    // slow-loris / half-open peer cannot pin a worker thread forever. Generous
    // (60s) — a live session's reader legitimately blocks between messages, and
    // the server sends WS pings to keep it warm; this is the backstop.
    timeval tv{}; tv.tv_sec = 60; tv.tv_usec = 0;
    ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(conn, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static int make_listener(const ServeConfig& cfg, bool all_ifaces) {
    int lfd = -1;
    auto reuse = [](int fd){
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
        // Instant restart + lets multiple acceptors share the port if desired.
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif
    };
    if (all_ifaces) {
        lfd = ::socket(AF_INET6, SOCK_STREAM, 0);
        if (lfd >= 0) {
            reuse(lfd);
            int v6only = 0; ::setsockopt(lfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
            sockaddr_in6 a6{}; a6.sin6_family=AF_INET6; a6.sin6_port=htons((uint16_t)cfg.port); a6.sin6_addr=in6addr_any;
            if (::bind(lfd,(sockaddr*)&a6,sizeof(a6)) < 0) { ::close(lfd); lfd = -1; }
        }
    }
    if (lfd < 0) {
        lfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (lfd < 0) return -1;
        reuse(lfd);
        sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons((uint16_t)cfg.port);
        a.sin_addr.s_addr = all_ifaces ? INADDR_ANY : inet_addr(cfg.host.c_str());
        if (::bind(lfd,(sockaddr*)&a,sizeof(a))<0) { ::close(lfd); return -1; }
    }
    return lfd;
}

int serve(const ServeConfig& cfg, std::function<void(int conn)> on_conn) {
    const bool all_ifaces = (cfg.host == "0.0.0.0" || cfg.host == "::");

    int lfd = make_listener(cfg, all_ifaces);
    if (lfd < 0) { std::perror("waya: bind"); return 1; }
    ::listen(lfd, 128);
    g_fd = lfd;
    std::signal(SIGINT, on_sigint);
    std::signal(SIGPIPE, SIG_IGN);
#ifdef SIGTERM
    std::signal(SIGTERM, on_sigint);
#endif

    const std::string open_host = all_ifaces ? "localhost" : cfg.host;
    const std::string url = cfg.url_scheme + "://" + open_host + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: surface app on %s  (Ctrl-C to stop)\n", url.c_str());
    if (all_ifaces) {
        std::string lan = lan_ip();
        if (!lan.empty())
            std::fprintf(stderr, "waya: on your network at %s://%s:%d\n",
                         cfg.url_scheme.c_str(), lan.c_str(), cfg.port);
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
            if (g_fd < 0) break;            // shutting down
            if (errno == EINTR) continue;   // signal, retry
            continue;                        // transient accept error, keep serving
        }
        tune_conn(conn);
        std::thread([on_conn, conn]{ on_conn(conn); }).detach();
    }
    return 0;
}

} // namespace detail
} // namespace waya::surface
