#pragma once
/// \file http_util.hpp
/// Non-templated HTTP/socket helpers for the Surface live runtime. These build
/// ONCE into the waya_runtime static lib (maya convention: header declares,
/// src/surface/http_util.cpp defines). The live<App> serving loop and the
/// templated safe_view/safe_dispatch stay in surface/live.hpp.

#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

namespace waya::surface::detail {

/// The listening socket fd, published so the SIGINT handler can close it and
/// exit cleanly. -1 when not listening.
extern std::atomic<int> g_fd;

/// SIGINT handler: close the listening socket and _Exit immediately.
void on_sigint(int);

/// The machine's primary LAN IP — so a 0.0.0.0-bound app can print a URL other
/// devices can reach. Trick: "connect" a UDP socket toward a public address (no
/// packet is sent) and read back the local endpoint the OS picked. Empty on
/// failure (offline / no route).
std::string lan_ip();

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
std::string request_path(std::string_view req);
/// The HTTP method token, e.g. "GET" from "GET /about HTTP/1.1".
std::string_view request_method(std::string_view req);

/// The set of response headers a production SSR page always carries: the type,
/// a caching policy, and the security headers a hardened server sends. `extra`
/// appends anything response-specific (Content-Encoding, Allow, ...).
/// - no-store on the HTML so a personalised, live-upgraded page is never cached
///   stale by a browser/proxy (the app streams its own updates over the socket);
/// - nosniff stops content-type confusion attacks;
/// - frame-ancestors 'self' + X-Frame-Options blocks clickjacking;
/// - a strict-origin referrer policy leaks less on outbound links.
std::string sec_headers();
/// Build a complete HTTP/1.1 response. `status` is the full status line text
/// ("200 OK", "404 Not Found", "405 Method Not Allowed"), `ctype` the
/// Content-Type. `head_only` sends headers but omits the body (HTTP HEAD).
std::string http_response(const char* status, const std::string& ctype,
                          const std::string& body, const std::string& extra_headers = {},
                          bool head_only = false, bool cache = false);
/// A structured access-log line: method, path, status. Opt-in — off by default
/// so a dev run isn't noisy, on when WAYA_LOG is set (production behind a proxy).
/// One line per request to stderr, so it composes with journald/Docker logs.
void access_log(std::string_view method, const std::string& path, int status);
/// Write ALL bytes, looping over short/partial writes. A single ::send can
/// short-write on a backpressured socket (far likelier through a proxy than on
/// loopback), which would truncate an HTTP response or handshake. Returns false
/// if the peer went away.
bool send_all(int fd, const char* data, std::size_t len);
/// True if the client advertised gzip in Accept-Encoding.
bool accepts_gzip(std::string_view req);

#ifdef WAYA_GZIP
/// gzip a buffer (opt-in: compile with -DWAYA_GZIP and link zlib). Returns empty
/// on failure so the caller falls back to sending the body uncompressed.
std::string gzip(const std::string& in);
#endif

/// An error card — shown in place of the app when view()/update() throws, so a
/// bug isolates the session instead of crashing the server. Valid HTML/CSS.
std::string error_html(std::string_view what);

/// A build id unique to this compiled binary (the compile timestamp). Used by
/// the dev hot-reload beacon: a rebuild produces a new id, so a reconnecting
/// client can tell "the server was rebuilt" apart from "the network blipped."
const char* build_id();

}  // namespace waya::surface::detail
