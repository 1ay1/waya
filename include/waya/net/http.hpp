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

inline Url parse_url(const std::string& url) {
    Url u; u.port = 80; u.scheme = "http"; u.path = "/";
    std::string rest = url;
    if (auto p = rest.find("://"); p != std::string::npos) {
        u.scheme = rest.substr(0, p); rest = rest.substr(p + 3);
    }
    u.port = (u.scheme == "https") ? 443 : 80;
    auto slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    u.path = slash == std::string::npos ? "/" : rest.substr(slash);
    if (auto c = hostport.find(':'); c != std::string::npos) {
        u.host = hostport.substr(0, c); u.port = std::atoi(hostport.substr(c + 1).c_str());
    } else u.host = hostport;
    return u;
}

/// Connect a TCP socket to host:port with a timeout. -1 on failure.
inline int tcp_connect(const std::string& host, int port, int timeout_ms) {
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) return -1;
    int fd = -1;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        // non-blocking connect with select() timeout
        int fl = ::fcntl(fd, F_GETFL, 0); ::fcntl(fd, F_SETFL, fl | O_NONBLOCK);
        int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc == 0) { ::fcntl(fd, F_SETFL, fl); break; }
        if (errno == EINPROGRESS) {
            fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
            timeval tv{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
            if (::select(fd + 1, nullptr, &wf, nullptr, &tv) > 0) {
                int err = 0; socklen_t len = sizeof(err);
                ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) { ::fcntl(fd, F_SETFL, fl); break; }
            }
        }
        ::close(fd); fd = -1;
    }
    ::freeaddrinfo(res);
    if (fd >= 0) {
        // apply a read timeout for the response
        timeval tv{ timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    return fd;
}

inline std::string build_request(const Url& u, const Request& r) {
    std::string req = r.method + " " + u.path + " HTTP/1.1\r\n";
    req += "Host: " + u.host + "\r\n";
    req += "Connection: close\r\n";
    bool has_len = false, has_ua = false;
    for (auto& [k, v] : r.headers) {
        req += k + ": " + v + "\r\n";
        if (k.size() == 14) has_len = true;   // "Content-Length"
        if (k.size() == 10) has_ua = true;    // "User-Agent"
    }
    if (!has_ua) req += "User-Agent: waya/0.1\r\n";
    if (!r.body.empty() && !has_len) req += "Content-Length: " + std::to_string(r.body.size()) + "\r\n";
    req += "\r\n";
    req += r.body;
    return req;
}

inline Response parse_response(const std::string& raw) {
    Response r;
    auto he = raw.find("\r\n\r\n");
    std::string head = he == std::string::npos ? raw : raw.substr(0, he);
    r.body = he == std::string::npos ? "" : raw.substr(he + 4);
    // status line
    if (auto sp = head.find(' '); sp != std::string::npos)
        r.status = std::atoi(head.substr(sp + 1).c_str());
    // headers
    std::size_t pos = head.find("\r\n");
    while (pos != std::string::npos) {
        std::size_t next = head.find("\r\n", pos + 2);
        std::string line = head.substr(pos + 2, (next == std::string::npos ? head.size() : next) - pos - 2);
        if (auto c = line.find(':'); c != std::string::npos) {
            std::string k = line.substr(0, c);
            std::string v = line.substr(c + 1);
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
            r.headers.emplace_back(std::move(k), std::move(v));
        }
        pos = next;
    }
    // de-chunk if needed. A hostile/truncated response can declare a chunk
    // length longer than the actual body, or a negative/overflowing hex length;
    // every access below is bounds-checked so a bad response yields a short body
    // rather than an out_of_range throw or an OOB read.
    for (auto& [k, v] : r.headers) {
        if (k.size() == 17 && (k[0]|32) == 't' && v.find("chunked") != std::string::npos) {
            std::string out; std::size_t i = 0;
            while (i < r.body.size()) {
                std::size_t nl = r.body.find("\r\n", i); if (nl == std::string::npos) break;
                // parse the hex chunk size (ignoring any ;chunk-extension)
                std::string szline = r.body.substr(i, nl - i);
                errno = 0;
                long long len = std::strtoll(szline.c_str(), nullptr, 16);
                if (len <= 0 || errno) break;                      // 0 = terminator; <0/overflow = malformed
                std::size_t data = nl + 2;                          // start of chunk data
                if (data > r.body.size()) break;
                std::size_t avail = r.body.size() - data;
                std::size_t take = (std::size_t)len <= avail ? (std::size_t)len : avail;
                out.append(r.body, data, take);
                if (take < (std::size_t)len) break;                 // truncated: stop cleanly
                i = data + take + 2;                                // skip data + trailing CRLF
            }
            r.body = std::move(out);
            break;
        }
    }
    return r;
}

} // namespace detail

/// Perform an HTTP(S) request, blocking, with timeout. Empty/status-0 on error.
inline Response request(const Request& r) {
    detail::Url u = detail::parse_url(r.url);
    Response resp;

    if (u.scheme == "https") {
#ifndef WAYA_TLS
        return resp;   // status 0: HTTPS needs -DWAYA_TLS (OpenSSL)
#else
        int fd = detail::tcp_connect(u.host, u.port, r.timeout_ms);
        if (fd < 0) return resp;
        static bool inited = [] { SSL_library_init(); SSL_load_error_strings(); return true; }();
        (void)inited;
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { ::close(fd); return resp; }
        SSL_CTX_set_default_verify_paths(ctx);
        SSL* ssl = SSL_new(ctx);
        SSL_set_tlsext_host_name(ssl, u.host.c_str());   // SNI
        SSL_set_fd(ssl, fd);
        if (SSL_connect(ssl) == 1) {
            std::string req = detail::build_request(u, r);
            SSL_write(ssl, req.data(), (int)req.size());
            std::string raw; char b[8192]; int n;
            while ((n = SSL_read(ssl, b, sizeof(b))) > 0) raw.append(b, n);
            resp = detail::parse_response(raw);
        }
        SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); ::close(fd);
        return resp;
#endif
    }

    // plain HTTP
    int fd = detail::tcp_connect(u.host, u.port, r.timeout_ms);
    if (fd < 0) return resp;
    std::string req = detail::build_request(u, r);
    ::send(fd, req.data(), req.size(), 0);
    std::string raw; char b[8192]; ssize_t n;
    while ((n = ::recv(fd, b, sizeof(b), 0)) > 0) raw.append(b, n);
    ::close(fd);
    return detail::parse_response(raw);
}

/// `get(url)` / `post(url, body, content_type)` — the common shorthands.
inline Response get(const std::string& url, int timeout_ms = 10000) {
    return request({ .method = "GET", .url = url, .timeout_ms = timeout_ms });
}
inline Response post(const std::string& url, std::string body,
                     std::string content_type = "application/json", int timeout_ms = 10000) {
    return request({ .method = "POST", .url = url,
                     .headers = {{"Content-Type", std::move(content_type)}},
                     .body = std::move(body), .timeout_ms = timeout_ms });
}

// ── Cookies ──────────────────────────────────────────────────────────
// The live handshake is an HTTP request, so its Cookie: header carries whatever
// the browser stored (a session id, an auth token). These parse it, so an app
// can identify a returning user. (Setting cookies happens in the HTTP response;
// the live runtime exposes that via LiveConfig hooks.)

/// Parse a `Cookie:` header value ("a=1; b=2") into name->value pairs.
inline std::vector<std::pair<std::string,std::string>> parse_cookies(std::string_view header) {
    std::vector<std::pair<std::string,std::string>> out;
    std::size_t i = 0;
    while (i < header.size()) {
        std::size_t semi = header.find(';', i);
        std::string_view part = header.substr(i, (semi == std::string_view::npos ? header.size() : semi) - i);
        std::size_t eq = part.find('=');
        if (eq != std::string_view::npos) {
            std::string k(part.substr(0, eq)), v(part.substr(eq + 1));
            auto trim = [](std::string& s){ while(!s.empty()&&(s.front()==' '||s.front()=='\t'))s.erase(s.begin());
                                            while(!s.empty()&&(s.back()==' '||s.back()=='\t'))s.pop_back(); };
            trim(k); trim(v);
            if (!k.empty()) out.emplace_back(std::move(k), std::move(v));
        }
        if (semi == std::string_view::npos) break;
        i = semi + 1;
    }
    return out;
}

/// Extract the Cookie: header value from a raw HTTP request (the WS handshake).
inline std::string cookie_header(std::string_view raw_request) {
    // case-insensitive line scan for "cookie:"
    std::size_t i = 0;
    while (i < raw_request.size()) {
        std::size_t nl = raw_request.find("\r\n", i);
        std::string_view line = raw_request.substr(i, (nl == std::string_view::npos ? raw_request.size() : nl) - i);
        if (line.size() >= 7) {
            bool m = true; const char* k = "cookie:";
            for (int j = 0; j < 7; ++j) if ((line[j]|32) != k[j]) { m = false; break; }
            if (m) { std::string v(line.substr(7)); while(!v.empty()&&v.front()==' ')v.erase(v.begin()); return v; }
        }
        if (nl == std::string_view::npos) break;
        i = nl + 2;
    }
    return {};
}

/// Build a `Set-Cookie` value with sane security defaults (HttpOnly, SameSite).
inline std::string set_cookie(const std::string& name, const std::string& value,
                              long max_age_secs = 0, bool http_only = true,
                              bool secure = false, std::string same_site = "Lax") {
    std::string c = name + "=" + value + "; Path=/";
    if (max_age_secs > 0) c += "; Max-Age=" + std::to_string(max_age_secs);
    if (!same_site.empty()) c += "; SameSite=" + same_site;
    if (http_only) c += "; HttpOnly";
    if (secure)    c += "; Secure";
    return c;
}

} // namespace waya::http
