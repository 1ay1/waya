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

// ── Request limits (abuse defense at the HTTP layer) ────────────────────────
/// A production origin must bound how much it will read before the request is
/// complete, so a slow-loris / oversized-header client can't exhaust memory or
/// pin a worker. These are generous for a legitimate request and hard-fail
/// anything past them with a 4xx.
struct RequestLimits {
    std::size_t max_request  = 64u * 1024u;   // total header bytes before \r\n\r\n
    std::size_t max_body     = 8u  * 1024u * 1024u;  // request-body cap (POST/PUT)
    std::size_t max_line     = 8u  * 1024u;   // longest single header/request line
    int         max_headers  = 100;           // header-field count cap
};

/// The outcome of trying to read one request off a persistent connection.
enum class ReadStatus {
    Ok,              // a complete request is in `raw` (+ `body`)
    Closed,          // peer closed cleanly before/between requests (keep-alive end)
    Timeout,         // idle too long between requests
    TooLarge,        // exceeded a RequestLimits bound  -> respond 431/413 then close
    Malformed,       // not valid HTTP/1.x               -> respond 400 then close
};

/// A parsed HTTP request: the raw header block plus cheap accessors. Header
/// lookups are case-insensitive per RFC 9110.
struct Request {
    std::string raw;          // the full header block up to and incl. the blank line
    std::string body;         // decoded request body (Content-Length; may be empty)
    std::string_view method() const;
    std::string_view target() const;          // request-target (path?query), raw
    std::string_view version() const;         // "HTTP/1.1" etc.
    std::string path() const;                 // target with the query stripped
    /// Case-insensitive header value ("" if absent).
    std::string_view header(std::string_view name) const;
    bool has_header(std::string_view name) const;
    /// Does the client keep the connection alive? (HTTP/1.1 default keep, unless
    /// Connection: close; HTTP/1.0 default close, unless Connection: keep-alive.)
    bool wants_keep_alive() const;
    /// The effective client scheme/host/ip honoring X-Forwarded-* from a trusted
    /// proxy (waya is an origin behind one). Fall back to direct values.
    std::string forwarded_proto(std::string_view fallback = "http") const;
    std::string forwarded_host(std::string_view fallback = {}) const;
    std::string forwarded_for(std::string_view direct_ip = {}) const;
};

/// Read exactly ONE request off `fd` into `out`, enforcing `lim`. Buffers across
/// multiple recv()s, splits header block from an over-read body, and (when a
/// Content-Length is present and within max_body) reads the body too. Any bytes
/// belonging to the NEXT pipelined request are left in `carry` for the next call
/// on the same connection. Returns a ReadStatus; on Ok, `out` is populated.
ReadStatus read_request(int fd, Request& out, std::string& carry,
                        const RequestLimits& lim = {});

/// The listening socket fd, published so the SIGINT handler can close it and
/// exit cleanly. -1 when not listening.
extern std::atomic<int> g_fd;

/// Graceful-drain flag: set by SIGTERM so the accept loop stops taking new
/// connections and in-flight requests are allowed to finish before exit.
extern std::atomic<bool> g_draining;

/// A weak ETag for a body (FNV-1a of the bytes, quoted, W/ prefix). Lets a
/// caching proxy revalidate the SSR page with If-None-Match -> 304.
std::string weak_etag(std::string_view body);

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
                          bool head_only = false, bool cache = false,
                          bool keep_alive = false);
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
