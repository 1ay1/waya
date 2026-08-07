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
};

namespace detail {

/// Set the speed/robustness socket options on an ACCEPTED connection fd:
/// TCP_NODELAY (diffs are tiny frames — Nagle would add ~40ms latency per tap),
/// and send/recv timeouts (a stalled or slow-loris client can't pin the worker
/// thread forever). Best-effort; failures are ignored.
void tune_conn(int conn);

/// Run the accept loop. Creates the listening socket (IPv6 dual-stack when
/// host is 0.0.0.0/::, else the exact host; REUSEADDR + REUSEPORT), installs
/// SIGINT/SIGTERM/SIGPIPE handling, prints the banner, optionally opens a
/// browser, then loops accept()->tune_conn()->on_conn(fd) until Ctrl-C.
/// Returns a process exit code (0 normal, 1 on bind failure). `on_conn` takes
/// ownership of the fd (it is expected to close it). Each connection is handled
/// on its own detached thread inside serve().
int serve(const ServeConfig& cfg, std::function<void(int conn)> on_conn);

} // namespace detail
} // namespace waya::surface
