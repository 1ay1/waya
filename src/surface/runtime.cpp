/// \file runtime.cpp
/// The non-templated transport: socket setup + hardening + accept loop, compiled
/// once into waya_runtime. See runtime.hpp for the seam.

#include "waya/surface/runtime.hpp"
#include "waya/surface/http_util.hpp"   // detail::g_fd, on_sigint, lan_ip

#include <sys/socket.h>
#if defined(__linux__)
#  include <sys/epoll.h>
#  include <sys/eventfd.h>
#else
#  include <sys/event.h>   // kqueue — macOS & the BSDs
#  include <sys/time.h>
#endif
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

    // ── Scale model: event gate + bounded worker pool + connection ceiling ───
    //
    // Idle & between-request keep-alive connections are PARKED in the kernel
    // event queue (a slot, not a thread) — epoll on Linux, kqueue on macOS/BSD;
    // same semantics (oneshot arm, re-arm on keep-alive), one small Gate seam.
    // Only a connection with request bytes READY is handed to a bounded worker
    // pool, which serves exactly what's available and hands the fd back:
    // KeepAlive => re-park, Owned => a WS session took its own thread (gate
    // forgets it), Close => drop. So N idle keep-alive sockets cost O(N) memory
    // + O(workers) threads, not O(N) threads — the event-queue scaling win
    // without rewriting the stateful session model. A ceiling sheds excess with
    // a fast 503.

    unsigned hw = std::thread::hardware_concurrency(); if (!hw) hw = 4;
    int workers = cfg.workers;
    if (const char* w = std::getenv("WAYA_WORKERS")) workers = std::atoi(w);
    if (workers <= 0) workers = (int)hw * 4;
    int max_conn = cfg.max_conn;
    if (const char* m = std::getenv("WAYA_MAX_CONN")) max_conn = std::atoi(m);
    if (max_conn <= 0) max_conn = 10000;

    // The Gate: a thin platform seam over epoll/kqueue with identical semantics.
    //   add_listener  level-triggered read interest, persistent
    //   arm(fd)       ONESHOT read interest (first add and re-arm are the same op)
    //   forget(fd)    drop interest (safe if the oneshot already auto-dropped)
    //   wake()        thread-safe user wakeup (workers → gate thread)
    //   wait(evs,ms)  → {fd, hup, is_wake} triples
    struct Gate {
        struct Ev { int fd; bool hup; bool is_wake; };
        int q = -1;
#if defined(__linux__)
        int wakefd = -1;
        bool init() {
            q = ::epoll_create1(EPOLL_CLOEXEC);
            if (q < 0) return false;
            wakefd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
            if (wakefd >= 0) { epoll_event we{}; we.events = EPOLLIN; we.data.fd = wakefd; ::epoll_ctl(q, EPOLL_CTL_ADD, wakefd, &we); }
            return true;
        }
        void add_listener(int lfd) { epoll_event lev{}; lev.events = EPOLLIN; lev.data.fd = lfd; ::epoll_ctl(q, EPOLL_CTL_ADD, lfd, &lev); }
        bool arm(int fd, bool first) {
            epoll_event ce{}; ce.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT; ce.data.fd = fd;
            return ::epoll_ctl(q, first ? EPOLL_CTL_ADD : EPOLL_CTL_MOD, fd, &ce) == 0;
        }
        void forget(int fd) { ::epoll_ctl(q, EPOLL_CTL_DEL, fd, nullptr); }
        void wake() { if (wakefd >= 0) { std::uint64_t one = 1; (void)::write(wakefd, &one, sizeof(one)); } }
        int wait(std::vector<Ev>& out, int ms) {
            epoll_event evs[256];
            int n = ::epoll_wait(q, evs, 256, ms);
            if (n <= 0) return n;
            out.clear();
            for (int i = 0; i < n; ++i) {
                int fd = evs[i].data.fd;
                if (fd == wakefd) { std::uint64_t v; while (::read(wakefd, &v, sizeof(v)) > 0) {} out.push_back({fd, false, true}); continue; }
                out.push_back({fd, (evs[i].events & (EPOLLHUP | EPOLLERR)) != 0, false});
            }
            return (int)out.size();
        }
        void shut() { if (wakefd >= 0) ::close(wakefd); if (q >= 0) ::close(q); }
#else
        // kqueue: EV_ONESHOT auto-deletes after delivery, so arm() is the same
        // EV_ADD for first-park and re-park. Peer hangup surfaces as EV_EOF on
        // the read filter — only a hup when no readable bytes remain (data==0),
        // matching epoll's "EPOLLRDHUP still goes to a worker" behaviour.
        bool init() {
            q = ::kqueue();
            if (q < 0) return false;
            struct kevent we; EV_SET(&we, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
            ::kevent(q, &we, 1, nullptr, 0, nullptr);
            return true;
        }
        void add_listener(int lfd) { struct kevent lev; EV_SET(&lev, lfd, EVFILT_READ, EV_ADD, 0, 0, nullptr); ::kevent(q, &lev, 1, nullptr, 0, nullptr); }
        bool arm(int fd, bool /*first*/) {
            struct kevent ce; EV_SET(&ce, fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, nullptr);
            return ::kevent(q, &ce, 1, nullptr, 0, nullptr) == 0;
        }
        void forget(int fd) { struct kevent de; EV_SET(&de, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr); ::kevent(q, &de, 1, nullptr, 0, nullptr); }
        void wake() { struct kevent tr; EV_SET(&tr, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr); ::kevent(q, &tr, 1, nullptr, 0, nullptr); }
        int wait(std::vector<Ev>& out, int ms) {
            struct kevent evs[256];
            timespec ts{ ms / 1000, (long)(ms % 1000) * 1000000L };
            int n = ::kevent(q, nullptr, 0, evs, 256, &ts);
            if (n <= 0) return n;
            out.clear();
            for (int i = 0; i < n; ++i) {
                if (evs[i].filter == EVFILT_USER) { out.push_back({-1, false, true}); continue; }
                bool hup = (evs[i].flags & EV_EOF) && evs[i].data == 0;
                out.push_back({(int)evs[i].ident, hup, false});
            }
            return (int)out.size();
        }
        void shut() { if (q >= 0) ::close(q); }
#endif
    };

    Gate gate;
    if (!gate.init()) { std::perror("waya: event gate"); return 1; }
    gate.add_listener(lfd);

    auto set_nonblock = [](int fd){ int f=::fcntl(fd,F_GETFL,0); if(f>=0) ::fcntl(fd,F_SETFL,f|O_NONBLOCK); };
    auto clr_nonblock = [](int fd){ int f=::fcntl(fd,F_GETFL,0); if(f>=0) ::fcntl(fd,F_SETFL,f&~O_NONBLOCK); };

    // Per-connection state that must survive across re-parks: the carry buffer
    // of bytes already read past the previous request. `conn_count` is the live
    // connection total (parked + in-flight) for the ceiling. Both are touched
    // ONLY by the gate thread.
    std::unordered_map<int, std::string> conns;          // fd -> carry (gate thread only)
    std::atomic<int> conn_count{0};

    // CRITICAL fd-lifetime rule: ONLY the gate thread registers/deregisters or
    // close()s a connection fd. Workers never touch the gate or close — they
    // serve a request and push a (fd, disposition, carry) result back; the gate
    // thread applies it. This removes the classic close()-vs-rearm race on a
    // possibly-recycled fd.
    struct Done { int fd; Disposition disp; std::string carry; };
    struct Job  { int fd; std::string carry; };
    std::mutex qm; std::condition_variable qcv; std::deque<Job> ready; bool stop = false;
    std::mutex dm; std::deque<Done> done;
    // Carry travels WITH the fd in the ready queue (not a separate fd->carry map
    // under `dm`), so a request touches `dm` only for the finished-result push,
    // not also for a carry handoff — half the cross-thread lock traffic per
    // request, and one fewer hash map on the hot path.

    auto worker_fn = [&]{
        for (;;) {
            Job job;
            { std::unique_lock<std::mutex> l(qm);
              qcv.wait(l, [&]{ return stop || !ready.empty(); });
              if (stop && ready.empty()) return;
              job = std::move(ready.front()); ready.pop_front(); }
            Disposition d = on_ready(job.fd, job.carry);   // blocking recv/send happens here
            { std::lock_guard<std::mutex> l(dm); done.push_back(Done{job.fd, d, std::move(job.carry)}); }
            gate.wake();
        }
    };
    std::vector<std::thread> pool; pool.reserve(workers);
    for (int i = 0; i < workers; ++i) pool.emplace_back(worker_fn);

    // Apply a finished worker's decision (gate thread only).
    auto apply_done = [&](Done& r){
        if (r.disp == Disposition::KeepAlive) {
            conns[r.fd] = std::move(r.carry);
            set_nonblock(r.fd);
            if (!gate.arm(r.fd, false)) {
                ::close(r.fd); conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
            }
        } else if (r.disp == Disposition::Owned) {
            // A WS session owns+closes the fd on its own thread; forget it here.
            conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
        } else {
            ::close(r.fd); conns.erase(r.fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    std::vector<Gate::Ev> evs; evs.reserve(256);
    for (;;) {
        if (g_draining) break;
        int n = gate.wait(evs, 500);
        if (n < 0) { if (errno == EINTR) continue; if (g_fd < 0 || g_draining) break; continue; }
        // Drain finished-worker results first (all registration/close happen here).
        { std::deque<Done> batch;
          { std::lock_guard<std::mutex> l(dm); batch.swap(done); }
          for (auto& r : batch) apply_done(r); }
        for (int i = 0; i < n; ++i) {
            if (evs[i].is_wake) continue;
            int fd = evs[i].fd;
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
                    if (!gate.arm(c, true)) {
                        ::close(c); conns.erase(c); conn_count.fetch_sub(1, std::memory_order_relaxed);
                    }
                }
                continue;
            }
            // A parked connection is ready (oneshot: now disarmed) or hung up.
            if (evs[i].hup) {
                gate.forget(fd);
                ::close(fd); conns.erase(fd); conn_count.fetch_sub(1, std::memory_order_relaxed);
                continue;
            }
            // Hand it (with its carry) to a worker. clr_nonblock so the worker's
            // recv/send block with the SO_*TIMEO backstop.
            std::string carry;
            { auto it = conns.find(fd); if (it != conns.end()) carry = std::move(it->second); }
            clr_nonblock(fd);
            { std::lock_guard<std::mutex> l(qm); ready.push_back(Job{fd, std::move(carry)}); }
            qcv.notify_one();
        }
    }

    // Drain: stop workers, apply any last results, brief grace, close everything.
    { std::lock_guard<std::mutex> l(qm); stop = true; } qcv.notify_all();
    if (g_draining) std::this_thread::sleep_for(std::chrono::milliseconds(250));
    for (auto& t : pool) if (t.joinable()) t.join();
    { std::deque<Done> batch; { std::lock_guard<std::mutex> l(dm); batch.swap(done); } for (auto& r : batch) if (r.disp != Disposition::Owned) ::close(r.fd); }
    for (auto& [fd, _] : conns) ::close(fd);
    gate.shut();
    return 0;
}

} // namespace detail
} // namespace waya::surface
