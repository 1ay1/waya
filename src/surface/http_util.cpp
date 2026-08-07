#include "waya/surface/http_util.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
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
                          bool head_only, bool cache){
    std::string h = "HTTP/1.1 " + std::string(status) + "\r\n";
    h += "Content-Type: " + ctype + "\r\n";
    h += cache ? "Cache-Control: public, max-age=3600\r\n"
               : "Cache-Control: no-store\r\n";
    h += sec_headers();
    h += extra_headers;
    h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    h += "Connection: close\r\n\r\n";
    if (!head_only) h += body;
    return h;
}

void access_log(std::string_view method, const std::string& path, int status){
    static const bool on = std::getenv("WAYA_LOG") != nullptr;
    if (!on) return;
    std::time_t t = std::time(nullptr); char ts[32];
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
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
