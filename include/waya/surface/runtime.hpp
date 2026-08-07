#pragma once
/// \file runtime.hpp
/// The transport seam. `serve()` owns everything non-templated about running a
/// live server: socket setup (IPv6 dual-stack + hardening), bind/listen, signal
/// handling, the LAN-address banner, optional browser-open, and the accept loop.
/// For each accepted connection it invokes a type-erased handler. The templated
/// live<App>() in surface/live.hpp builds that handler (the only part that must
/// see the app's Program) and hands it here — so the whole accept path compiles
/// ONCE into waya_runtime rather than per app TU.

#include <cstdint>
#include <functional>
#include <string>

namespace waya::surface {

/// Public per-run configuration (mirrors the fields of LiveConfig that the
/// transport needs; LiveConfig itself stays in live.hpp for the app-facing API).
struct ServeConfig {
    int             port  = 8080;
    std::string     host  = "0.0.0.0";
    bool            open  = true;    // open a browser on start
    std::string     url_scheme = "http";
    // Scale knobs (env-overridable in serve()):
    int             max_conn = 0;    // live-connection ceiling; 0 = auto (see serve)
    int             workers  = 0;    // handler thread-pool size; 0 = auto (CPU-based)
};

namespace detail {

/// What the connection handler wants the gate to do with the fd afterwards.
enum class Disposition {
    Close,      // handler is done and has closed / will close the fd
    KeepAlive,  // HTTP keep-alive: re-park the fd in epoll for the next request
    Owned,      // handler took ownership (e.g. a WebSocket session on its own
                // thread); the gate must forget the fd entirely and NOT close it
};

/// Set the speed/robustness socket options on an ACCEPTED connection fd:
/// TCP_NODELAY (diffs are tiny frames — Nagle would add ~40ms latency per tap),
/// and send/recv timeouts (a stalled or slow-loris client can't pin the worker
/// thread forever). Best-effort; failures are ignored.
void tune_conn(int conn);

/// Run the accept loop. Creates the listening socket (IPv6 dual-stack when
/// host is 0.0.0.0/::, else the exact host; REUSEADDR + REUSEPORT), installs
/// SIGINT/SIGTERM/SIGPIPE handling, prints the banner, optionally opens a
/// browser, then drives an epoll gate: idle/keep-alive connections are PARKED in
/// epoll (cheap — a slot, not a thread) and only handed to a BOUNDED worker pool
/// when they have request bytes ready. A connection ceiling sheds excess load
/// with a fast 503. `on_ready(fd, carry)` serves what's available and returns a
/// Disposition telling the gate whether to re-park (KeepAlive), forget (Owned,
/// e.g. a WebSocket that took its own thread) or drop (Close) the fd. `carry`
/// holds bytes already read past one request, preserved across re-parks.
/// Returns a process exit code (0 normal, 1 on bind failure).
int serve(const ServeConfig& cfg,
          std::function<Disposition(int fd, std::string& carry)> on_ready);

} // namespace detail
} // namespace waya::surface
