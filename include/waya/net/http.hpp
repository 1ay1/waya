#pragma once
/// \file net/http.hpp
/// A real (if small) blocking HTTP/1.1 client for the effect runtime. This is
/// what Cmd::fetch/post actually use. Unlike a toy GET, it does DNS, arbitrary
/// methods, request headers + body, a connect/read timeout, and \u2014 when built
/// with -DWAYA_TLS (OpenSSL) \u2014 real HTTPS. Without TLS, https:// URLs fail
/// cleanly (empty body, status 0) rather than silently talking plaintext.
///
///   auto r = http::request({ .method="POST", .url="https://api.x.dev/v1/pay",
///       .headers={{"Authorization","Bearer ..."},{"Content-Type","application/json"}},
///       .body=payload, .timeout_ms=8000 });
///   if (r.ok()) use(r.body);        // r.status, r.headers, r.body
///
/// Runs on a worker thread (never the model loop) \u2014 the runtime calls it from
/// Cmd::fetch/task. Blocking is fine there; one request, one thread, then gone.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <chrono>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

#ifdef WAYA_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace waya::http {

struct Request {
    std::string method = "GET";
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeout_ms = 10000;
};

struct Response {
    int status = 0;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    bool ok() const { return status >= 200 && status < 300; }
    std::string header(std::string_view name) const {
        for (auto& [k, v] : headers) {
            if (k.size() == name.size()) {
                bool eq = true;
                for (std::size_t i = 0; i < k.size(); ++i)
                    if ((k[i]|32) != (name[i]|32)) { eq = false; break; }
                if (eq) return v;
            }
        }
        return {};
    }
};

namespace detail {

struct Url { std::string scheme, host, path; int port; };

Url parse_url(const std::string& url);

/// Connect a TCP socket to host:port with a timeout. -1 on failure.
int tcp_connect(const std::string& host, int port, int timeout_ms);

bool iequals(std::string_view a, std::string_view b);

std::string build_request(const Url& u, const Request& r);

Response parse_response(const std::string& raw);

} // namespace detail

/// Perform an HTTP(S) request, blocking, with timeout. Empty/status-0 on error.
Response request(const Request& r);

/// `get(url)` / `post(url, body, content_type)` — the common shorthands.
Response get(const std::string& url, int timeout_ms = 10000);
Response post(const std::string& url, std::string body,
              std::string content_type = "application/json", int timeout_ms = 10000);

// ── Cookies ──────────────────────────────────────────────────────────
// The live handshake is an HTTP request, so its Cookie: header carries whatever
// the browser stored (a session id, an auth token). These parse it, so an app
// can identify a returning user. (Setting cookies happens in the HTTP response;
// the live runtime exposes that via LiveConfig hooks.)

/// Parse a `Cookie:` header value ("a=1; b=2") into name->value pairs.
std::vector<std::pair<std::string,std::string>> parse_cookies(std::string_view header);

/// Extract the Cookie: header value from a raw HTTP request (the WS handshake).
std::string cookie_header(std::string_view raw_request);

/// Build a `Set-Cookie` value with sane security defaults (HttpOnly, SameSite).
std::string set_cookie(const std::string& name, const std::string& value,
                       long max_age_secs = 0, bool http_only = true,
                       bool secure = false, std::string same_site = "Lax");

} // namespace waya::http
