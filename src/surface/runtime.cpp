/// \file runtime.cpp
/// The non-templated transport: socket setup + hardening + accept loop, compiled
/// once into waya_runtime. See runtime.hpp for the seam.

#include "waya/surface/runtime.hpp"
#include "waya/surface/http_util.hpp"   // detail::g_fd, on_sigint, lan_ip

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>   // TCP_NODELAY
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace waya::surface {
namespace detail {

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

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

int serve(const ServeConfig& cfg,
          std::function<Disposition(int fd, std::string& carry)> on_ready) {
    const bool all_ifaces = (cfg.host == "0.0.0.0" || cfg.host == "::");

    int lfd = make_listener(cfg, all_ifaces);
    if (lfd < 0) { std::perror("waya: bind"); return 1; }
    ::listen(lfd, 512);
    { int f=::fcntl(lfd,F_GETFL,0); if(f>=0) ::fcntl(lfd,F_SETFL,f|O_NONBLOCK); }
    g_fd = lfd;
    std::signal(SIGINT, on_sigint);
    std::signal(SIGPIPE, SIG_IGN);
#ifdef SIGTERM
    std::signal(SIGTERM, [](int){ g_draining = true; int fd=g_fd.exchange(-1); if(fd>=0)::shutdown(fd, SHUT_RDWR); });
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

    // ── Scale model: epoll gate + bounded worker pool + connection ceiling ───
    //
    // Idle & between-request keep-alive connections are PARKED in epoll (a slot,
    // not a thread). Only a connection with request bytes READY is handed to a
    // bounded worker pool, which serves exactly what's available and hands the
    // fd back: KeepAlive => re-park in epoll, Owned => a WS session took its own
    // thread (gate forgets it), Close => drop. So N idle keep-alive sockets cost
    // O(N) memory + O(workers) threads, not O(N) threads — the epoll scaling win
    // without rewriting the stateful session model. A ceiling sheds excess with
    // a fast 503.

    unsigned hw = std::thread::hardware_concurrency(); if (!hw) hw = 4;
    int workers = cfg.workers;
    if (const char* w = std::getenv("WAYA_WORKERS")) workers = std::atoi(w);
    if (workers <= 0) workers = (int)hw * 4;
    int max_conn = cfg.max_conn;
    if (const char* m = std::getenv("WAYA_MAX_CONN")) max_conn = std::atoi(m);
    if (max_conn <= 0) max_conn = 10000;

    int ep = ::epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) { std::perror("waya: epoll_create1"); return 1; }
    epoll_event lev{}; lev.events = EPOLLIN; lev.data.fd = lfd;
    ::epoll_ctl(ep, EPOLL_CTL_ADD, lfd, &lev);

    // An eventfd workers write to so the epoll thread wakes to process results.
    int wake = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake >= 0) { epoll_event we{}; we.events = EPOLLIN; we.data.fd = wake; ::epoll_ctl(ep, EPOLL_CTL_ADD, wake, &we); }
    const int wake_w = wake;

    auto set_nonblock = [](int fd){ int f=::fcntl(fd,F_GETFL,0); if(f>=0) ::fcntl(fd,F_SETFL,f|O_NONBLOCK); };
    auto clr_nonblock = [](int fd){ int f=::fcntl(fd,F_GETFL,0); if(f>=0) ::fcntl(fd,F_SETFL,f&~O_NONBLOCK); };

    // Per-connection state that must survive across re-parks: the carry buffer
    // of bytes already read past the previous request. `conn_count` is the live
    // connection total (parked + in-flight) for the ceiling. Both are touched
    // ONLY by the epoll thread.
    std::unordered_map<int, std::string> conns;          // fd -> carry (epoll thread only)
    std::atomic<int> conn_count{0};

    // CRITICAL fd-lifetime rule: ONLY the epoll thread calls epoll_ctl()/close()
    // on a connection fd. Workers never touch epoll or close — they serve a
    // request and push a (fd, disposition, carry) result back; the epoll thread
    // applies it. This removes the classic close()-vs-epoll_ctl race on a
    // possibly-recycled fd.
    struct Done { int fd; Disposition disp; std::string carry; };
    std::mutex qm; std::condition_variable qcv; std::deque<int> ready; bool stop = false;
    std::mutex dm; std::deque<Done> done;
    std::unordered_map<int, std::string> pending_carry;  // fd -> carry handed to a worker

    auto worker_fn = [&]{
        for (;;) {
            int fd;
            { std::unique_lock<std::mutex> l(qm);
              qcv.wait(l, [&]{ return stop || !ready.empty(); });
              if (stop && ready.empty()) return;
              fd = ready.front(); ready.pop_front(); }
            std::string carry;
            { std::lock_guard<std::mutex> l(dm);
              auto it = pending_carry.find(fd);
              if (it != pending_carry.end()) { carry = std::move(it->second); pending_carry.erase(it); } }
            Disposition d = on_ready(fd, carry);   // blocking recv/send happens here
            { std::lock_guard<std::mutex> l(dm); done.push_back(Done{fd, d, std::move(carry)}); }
            if (wake_w >= 0) { std::uint64_t one = 1; (void)::write(wake_w, &one, sizeof(one)); }
        }
    };
    std::vector<std::thread> pool; pool.reserve(workers);
    for (int i = 0; i < workers; ++i) pool.emplace_back(worker_fn);

    // Apply a finished worker's decision (epoll thread only).
    auto apply_done = [&](Done& r){
        if (r.disp == Disposition::KeepAlive) {
            conns[r.fd] = std::move(r.carry);
            set_nonblock(r.fd);
            epoll_event ce{}; ce.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT; ce.data.fd = r.fd;
            if (::epoll_ctl(ep, EPOLL_CTL_MOD, r.fd, &ce) < 0) {
                ::close(r.fd); conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
            }
        } else if (r.disp == Disposition::Owned) {
            // A WS session owns+closes the fd on its own thread; forget it here.
            conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
        } else {
            ::close(r.fd); conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    std::vector<epoll_event> evs(256);
    for (;;) {
        if (g_draining) break;
        int n = ::epoll_wait(ep, evs.data(), (int)evs.size(), 500);
        if (n < 0) { if (errno == EINTR) continue; if (g_fd < 0 || g_draining) break; continue; }
        // Drain finished-worker results first (all epoll/close happen here).
        { std::deque<Done> batch;
          { std::lock_guard<std::mutex> l(dm); batch.swap(done); }
          for (auto& r : batch) apply_done(r); }
        for (int i = 0; i < n; ++i) {
            int fd = evs[i].data.fd;
            if (fd == wake) { std::uint64_t v; while (::read(wake, &v, sizeof(v)) > 0) {} continue; }
            if (fd == lfd) {
                for (;;) {
                    int c = ::accept(lfd, nullptr, nullptr);
                    if (c < 0) { if (errno==EINTR) continue; break; }
                    if (g_draining) { ::close(c); continue; }
                    if (conn_count.load(std::memory_order_relaxed) >= max_conn) {
                        static const char* busy =
                            "HTTP/1.1 503 Service Unavailable\r\n"
                            "Retry-After: 2\r\nConnection: close\r\nContent-Length: 11\r\n\r\n"
                            "busy, retry";
                        ::send(c, busy, std::strlen(busy), MSG_NOSIGNAL);
                        ::close(c);
                        continue;
                    }
                    tune_conn(c);
                    set_nonblock(c);
                    conn_count.fetch_add(1, std::memory_order_relaxed);
                    conns[c] = std::string{};   // empty carry
                    epoll_event ce{}; ce.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT; ce.data.fd = c;
                    if (::epoll_ctl(ep, EPOLL_CTL_ADD, c, &ce) < 0) {
                        ::close(c); conns.erase(c); conn_count.fetch_sub(1, std::memory_order_relaxed);
                    }
                }
                continue;
            }
            // A parked connection is ready (EPOLLONESHOT: now disarmed) or hung up.
            if (evs[i].events & (EPOLLHUP | EPOLLERR)) {
                ::epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
                ::close(fd); conns.erase(fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
                continue;
            }
            // Hand it (with its carry) to a worker. clr_nonblock so the worker's
            // recv/send block with the SO_*TIMEO backstop.
            { std::lock_guard<std::mutex> l(dm); auto it = conns.find(fd); pending_carry[fd] = (it!=conns.end()? it->second : std::string{}); }
            clr_nonblock(fd);
            { std::lock_guard<std::mutex> l(qm); ready.push_back(fd); }
            qcv.notify_one();
        }
    }

    // Drain: stop workers, apply any last results, brief grace, close everything.
    { std::lock_guard<std::mutex> l(qm); stop = true; } qcv.notify_all();
    if (g_draining) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    for (auto& t : pool) if (t.joinable()) t.join();
    { std::deque<Done> batch; { std::lock_guard<std::mutex> l(dm); batch.swap(done); } for (auto& r : batch) if (r.disp != Disposition::Owned) ::close(r.fd); }
    for (auto& [fd, _] : conns) ::close(fd);
    if (wake >= 0) ::close(wake);
    if (ep >= 0) ::close(ep);
    return 0;
}

} // namespace detail
} // namespace waya::surface
