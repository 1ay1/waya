#include "waya/surface/http_util.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cerrno>

#ifdef WAYA_GZIP
#include <zlib.h>
#endif

// SIGPIPE avoidance is platform-specific. Linux offers the MSG_NOSIGNAL send
// flag; macOS/BSD historically don't. We pass this flag where available so a
// client vanishing mid-send never kills the process.
#ifndef MSG_NOSIGNAL
#define WAYA_MSG_NOSIGNAL 0
#else
#define WAYA_MSG_NOSIGNAL MSG_NOSIGNAL
#endif

namespace waya::surface::detail {

std::atomic<int> g_fd{-1};
std::atomic<bool> g_draining{false};

// ── case-insensitive helpers (RFC 9110 header names are case-insensitive) ────
static bool ci_eq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}
static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r')) s.remove_suffix(1);
    return s;
}

// ── Request accessors ────────────────────────────────────────────────────────
std::string_view Request::method() const {
    std::string_view r = raw; auto sp = r.find(' ');
    return sp == std::string_view::npos ? std::string_view{} : r.substr(0, sp);
}
std::string_view Request::target() const {
    std::string_view r = raw; auto a = r.find(' ');
    if (a == std::string_view::npos) return {};
    auto b = r.find(' ', a+1);
    if (b == std::string_view::npos) return {};
    return r.substr(a+1, b-a-1);
}
std::string_view Request::version() const {
    std::string_view r = raw; auto a = r.find(' ');
    if (a == std::string_view::npos) return {};
    auto b = r.find(' ', a+1);
    if (b == std::string_view::npos) return {};
    auto eol = r.find("\r\n", b+1);
    return trim(r.substr(b+1, (eol==std::string_view::npos? r.size() : eol) - b - 1));
}
std::string Request::path() const {
    std::string_view t = target();
    auto q = t.find('?');
    return std::string(q == std::string_view::npos ? t : t.substr(0, q));
}
std::string_view Request::header(std::string_view name) const {
    std::string_view r = raw;
    // Skip the request line.
    auto pos = r.find("\r\n");
    if (pos == std::string_view::npos) return {};
    pos += 2;
    while (pos < r.size()) {
        auto eol = r.find("\r\n", pos);
        if (eol == std::string_view::npos || eol == pos) break;   // end of headers
        auto colon = r.find(':', pos);
        if (colon != std::string_view::npos && colon < eol) {
            std::string_view key = trim(r.substr(pos, colon - pos));
            if (ci_eq(key, name)) return trim(r.substr(colon+1, eol - colon - 1));
        }
        pos = eol + 2;
    }
    return {};
}
bool Request::has_header(std::string_view name) const { return !header(name).empty(); }

bool Request::wants_keep_alive() const {
    std::string_view conn = header("Connection");
    std::string_view ver  = version();
    // A token-list match, case-insensitive.
    auto has_token = [&](std::string_view tok){
        std::string_view c = conn;
        while (!c.empty()) {
            auto comma = c.find(',');
            std::string_view t = trim(comma==std::string_view::npos ? c : c.substr(0, comma));
            if (ci_eq(t, tok)) return true;
            if (comma==std::string_view::npos) break;
            c = c.substr(comma+1);
        }
        return false;
    };
    if (has_token("close")) return false;
    if (ci_eq(ver, "HTTP/1.0")) return has_token("keep-alive");
    return true;   // HTTP/1.1 default: persistent
}

std::string Request::forwarded_proto(std::string_view fallback) const {
    std::string_view v = header("X-Forwarded-Proto");
    if (v.empty()) return std::string(fallback);
    auto comma = v.find(',');   // first hop is the client-facing one
    return std::string(trim(comma==std::string_view::npos ? v : v.substr(0, comma)));
}
std::string Request::forwarded_host(std::string_view fallback) const {
    std::string_view v = header("X-Forwarded-Host");
    if (v.empty()) v = header("Host");
    if (v.empty()) return std::string(fallback);
    auto comma = v.find(',');
    return std::string(trim(comma==std::string_view::npos ? v : v.substr(0, comma)));
}
std::string Request::forwarded_for(std::string_view direct_ip) const {
    std::string_view v = header("X-Forwarded-For");
    if (v.empty()) return std::string(direct_ip);
    auto comma = v.find(',');   // left-most = original client
    return std::string(trim(comma==std::string_view::npos ? v : v.substr(0, comma)));
}

// ── weak ETag (FNV-1a) ───────────────────────────────────────────────────────
std::string weak_etag(std::string_view body) {
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : body) { h ^= c; h *= 1099511628211ull; }
    char buf[32]; std::snprintf(buf, sizeof(buf), "W/\"%016llx\"", (unsigned long long)h);
    return buf;
}

// ── bounded request reader (keep-alive aware) ────────────────────────────────
ReadStatus read_request(int fd, Request& out, std::string& carry, const RequestLimits& lim) {
    out.raw.clear(); out.body.clear();
    std::string& buf = carry;   // may already hold a pipelined next request
    std::size_t hdr_end = std::string::npos;

    // Read until we have the full header block (\r\n\r\n) or hit a bound.
    for (;;) {
        if ((hdr_end = buf.find("\r\n\r\n")) != std::string::npos) break;
        if (buf.size() > lim.max_request) return ReadStatus::TooLarge;
        char tmp[16384];
        ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
        if (r == 0) return buf.empty() ? ReadStatus::Closed : ReadStatus::Malformed;
        if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return buf.empty() ? ReadStatus::Timeout : ReadStatus::Malformed;
            return buf.empty() ? ReadStatus::Closed : ReadStatus::Malformed;
        }
        buf.append(tmp, (std::size_t)r);
    }
    if (hdr_end + 4 > lim.max_request + 4) return ReadStatus::TooLarge;

    out.raw.assign(buf, 0, hdr_end + 4);

    // Validate the request line + per-line/header-count bounds.
    {
        std::string_view r = out.raw;
        auto first_eol = r.find("\r\n");
        if (first_eol == std::string_view::npos || first_eol > lim.max_line) return ReadStatus::Malformed;
        // request line must be METHOD SP TARGET SP VERSION
        std::string_view line = r.substr(0, first_eol);
        auto s1 = line.find(' ');
        if (s1 == std::string_view::npos) return ReadStatus::Malformed;
        auto s2 = line.find(' ', s1+1);
        if (s2 == std::string_view::npos) return ReadStatus::Malformed;
        if (line.substr(s2+1, 5) != "HTTP/") return ReadStatus::Malformed;
        // header lines
        int count = 0;
        std::size_t pos = first_eol + 2;
        while (pos < r.size()) {
            auto eol = r.find("\r\n", pos);
            if (eol == std::string_view::npos || eol == pos) break;
            if (eol - pos > lim.max_line) return ReadStatus::TooLarge;
            if (++count > lim.max_headers) return ReadStatus::TooLarge;
            if (r.find(':', pos) == std::string_view::npos || r.find(':', pos) >= eol)
                return ReadStatus::Malformed;   // header without a colon
            pos = eol + 2;
        }
    }

    // Any bytes after the header block are body and/or a pipelined request.
    std::string rest = buf.substr(hdr_end + 4);
    buf.clear();

    // Body: only for a declared Content-Length within max_body. (chunked request
    // bodies are rejected — an SSR origin behind a proxy that de-chunks.)
    std::string_view te = out.header("Transfer-Encoding");
    if (!te.empty() && te.find("chunked") != std::string_view::npos) return ReadStatus::Malformed;
    std::string_view cl = out.header("Content-Length");
    std::size_t need = 0;
    if (!cl.empty()) {
        char* end = nullptr;
        unsigned long long v = std::strtoull(std::string(cl).c_str(), &end, 10);
        if (!end || *end != '\0') return ReadStatus::Malformed;
        if (v > lim.max_body) return ReadStatus::TooLarge;
        need = (std::size_t)v;
    }
    out.body = std::move(rest);
    while (out.body.size() < need) {
        char tmp[16384];
        ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
        if (r <= 0) return ReadStatus::Malformed;
        out.body.append(tmp, (std::size_t)r);
        if (out.body.size() > lim.max_body + need) return ReadStatus::TooLarge;
    }
    // Split off exactly `need` body bytes; the remainder is the next pipelined
    // request, preserved in carry.
    if (out.body.size() > need) { carry = out.body.substr(need); out.body.resize(need); }
    return ReadStatus::Ok;
}

void on_sigint(int){ int fd=g_fd.exchange(-1); if(fd>=0)::close(fd);
    std::fprintf(stderr,"\nwaya: stopped.\n"); std::_Exit(0); }

std::string lan_ip() {
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return {};
    sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(53);
    to.sin_addr.s_addr = inet_addr("8.8.8.8");
    std::string ip;
    if (::connect(s, (sockaddr*)&to, sizeof(to)) == 0) {
        sockaddr_in me{}; socklen_t len = sizeof(me);
        if (::getsockname(s, (sockaddr*)&me, &len) == 0) {
            char buf[INET_ADDRSTRLEN];
            if (::inet_ntop(AF_INET, &me.sin_addr, buf, sizeof(buf))) ip = buf;
        }
    }
    ::close(s);
    return ip;
}

std::string request_path(std::string_view req){
    auto sp = req.find(' ');
    if (sp == std::string_view::npos) return "/";
    auto start = sp + 1;
    auto end = req.find(' ', start);
    if (end == std::string_view::npos || end <= start) return "/";
    std::string p{req.substr(start, end - start)};
    return p.empty() ? "/" : p;
}

std::string_view request_method(std::string_view req){
    auto sp = req.find(' ');
    return sp == std::string_view::npos ? std::string_view{"GET"} : req.substr(0, sp);
}

std::string sec_headers(){
    return "X-Content-Type-Options: nosniff\r\n"
           "X-Frame-Options: SAMEORIGIN\r\n"
           "Content-Security-Policy: frame-ancestors 'self'\r\n"
           "Referrer-Policy: strict-origin-when-cross-origin\r\n";
}

std::string http_response(const char* status, const std::string& ctype,
                          const std::string& body, const std::string& extra_headers,
                          bool head_only, bool cache, bool keep_alive){
    std::string h = "HTTP/1.1 " + std::string(status) + "\r\n";
    // Date is a MUST for an origin server (RFC 9110 6.6.1). gmtime_r is the
    // thread-safe variant — plain gmtime shares a static tm and races across
    // the worker pool.
    { std::time_t t = std::time(nullptr); std::tm tmv{}; char d[40];
      ::gmtime_r(&t, &tmv);
      std::strftime(d, sizeof d, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
      h += "Date: "; h += d; h += "\r\n"; }
    h += "Server: waya\r\n";
    h += "Content-Type: " + ctype + "\r\n";
    h += cache ? "Cache-Control: public, max-age=3600\r\n"
               : "Cache-Control: no-store\r\n";
    h += sec_headers();
    h += extra_headers;
    h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    // Persistent-connection support: keep the socket open for the next request
    // when the client asked to and we're not draining. Advertise the idle
    // timeout so a proxy can pool correctly.
    if (keep_alive) h += "Connection: keep-alive\r\nKeep-Alive: timeout=60\r\n\r\n";
    else            h += "Connection: close\r\n\r\n";
    if (!head_only) h += body;
    return h;
}

void access_log(std::string_view method, const std::string& path, int status){
    static const bool on = std::getenv("WAYA_LOG") != nullptr;
    if (!on) return;
    std::time_t t = std::time(nullptr); char ts[32];
    std::tm tmv{}; ::gmtime_r(&t, &tmv);
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    std::fprintf(stderr, "waya: %s %.*s %s %d\n", ts, (int)method.size(), method.data(), path.c_str(), status);
}

bool send_all(int fd, const char* data, std::size_t len){
    std::size_t off = 0;
    while (off < len) {
        ssize_t w = ::send(fd, data + off, len - off, WAYA_MSG_NOSIGNAL);
        if (w > 0) { off += (std::size_t)w; continue; }
        if (w < 0 && (errno == EINTR)) continue;
        return false;   // EAGAIN on a blocking socket is rare; treat as failure
    }
    return true;
}

bool accepts_gzip(std::string_view req){
    auto pos = req.find("Accept-Encoding:");
    if (pos == std::string_view::npos) pos = req.find("accept-encoding:");
    if (pos == std::string_view::npos) return false;
    auto eol = req.find("\r\n", pos);
    return req.substr(pos, (eol==std::string_view::npos?req.size():eol) - pos).find("gzip") != std::string_view::npos;
}

#ifdef WAYA_GZIP
std::string gzip(const std::string& in){
    z_stream zs{};
    if (deflateInit2(&zs, Z_BEST_SPEED, Z_DEFLATED, 15+16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};
    zs.next_in = (Bytef*)in.data(); zs.avail_in = (uInt)in.size();
    std::string out; char buf[16384];
    int ret;
    do {
        zs.next_out = (Bytef*)buf; zs.avail_out = sizeof(buf);
        ret = deflate(&zs, Z_FINISH);
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (ret == Z_OK);
    deflateEnd(&zs);
    return ret == Z_STREAM_END ? out : std::string{};
}
#endif

std::string error_html(std::string_view what){
    std::string safe; for(char c : what){ if(c=='<')safe+="&lt;"; else if(c=='>')safe+="&gt;"; else if(c=='&')safe+="&amp;"; else safe+=c; }
    return "<div style=\"min-height:100dvh;display:flex;align-items:center;justify-content:center;"
           "padding:24px;background:#0b1020;color:#e2e8f0;font-family:ui-sans-serif,system-ui,sans-serif\">"
           "<div style=\"max-width:32rem;padding:24px;border-radius:16px;background:#141b2e;"
           "border:1px solid #ef444455\">"
           "<div style=\"font-size:15px;font-weight:700;color:#ef4444;margin-bottom:8px\">"
           "Something went wrong</div>"
           "<div style=\"font-size:13px;color:#94a3b8;line-height:1.6;white-space:pre-wrap\">" + safe +
           "</div></div></div>";
}

const char* build_id(){ return __DATE__ " " __TIME__; }

}  // namespace waya::surface::detail
