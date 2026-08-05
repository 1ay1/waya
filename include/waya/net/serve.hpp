#pragma once
/// \file serve.hpp
/// A minimal, dependency-free HTTP/1.1 server for previewing waya pages in a
/// browser during development.
///
/// This is deliberately small: a blocking accept loop on a POSIX socket, one
/// request at a time, enough to render a `view()` and see it live. It is the
/// SEED of the Phase 2 `net/` layer (the real reactor — io_uring/epoll,
/// keep-alive, HTTP/2, routing — lands there). For now it does one job well:
/// turn `serve(view)` into a clickable localhost URL.
///
///   #include <waya/net/serve.hpp>
///   waya::serve([]{ return my_page(); });   // http://localhost:8080
///
/// POSIX only (Linux/macOS/BSD). Windows support arrives with the real net layer.

#include "../render/html.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace waya {

struct ServeConfig {
    int          port  = 8080;
    const char*  host  = "127.0.0.1";   ///< bind address (loopback by default)
    bool         open  = true;          ///< best-effort: open the browser
    bool         log   = true;          ///< print each request to stderr
};

namespace detail {

inline std::atomic<int> g_listen_fd{-1};

inline void on_sigint(int) {
    int fd = g_listen_fd.exchange(-1);
    if (fd >= 0) ::close(fd);
    std::fprintf(stderr, "\nwaya: stopped.\n");
    std::_Exit(0);
}

/// Best-effort "open this URL in the default browser". Silent if unavailable.
inline void try_open_browser(const std::string& url) {
#if defined(__APPLE__)
    std::string cmd = "open '" + url + "' >/dev/null 2>&1 &";
#else
    std::string cmd = "xdg-open '" + url + "' >/dev/null 2>&1 &";
#endif
    (void)std::system(cmd.c_str());
}

inline std::string_view method_and_path(std::string_view req, std::string_view& path_out) {
    // "GET /foo HTTP/1.1\r\n..." → method="GET", path="/foo"
    auto sp1 = req.find(' ');
    if (sp1 == std::string_view::npos) { path_out = "/"; return {}; }
    auto sp2 = req.find(' ', sp1 + 1);
    std::string_view method = req.substr(0, sp1);
    path_out = (sp2 == std::string_view::npos)
                 ? req.substr(sp1 + 1)
                 : req.substr(sp1 + 1, sp2 - sp1 - 1);
    return method;
}

} // namespace detail

/// A request handed to a serve() handler. Phase-1 minimal: method + path.
struct Request {
    std::string_view method;
    std::string_view path;
};

/// Serve a handler that maps a Request to an HTML document string.
/// Blocks until Ctrl-C. Returns non-zero only on a fatal setup error.
inline int serve(std::function<std::string(const Request&)> handler,
                 ServeConfig cfg = {}) {
    // Env overrides make the dev server scriptable (used by the smoke test):
    //   WAYA_PORT=<n>     bind a specific port
    //   WAYA_NO_OPEN=1    do not spawn a browser
    if (const char* p = std::getenv("WAYA_PORT"))    cfg.port = std::atoi(p);
    if (std::getenv("WAYA_NO_OPEN"))                 cfg.open = false;

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { std::perror("waya: socket"); return 1; }

    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(cfg.port));
    addr.sin_addr.s_addr = (std::strcmp(cfg.host, "0.0.0.0") == 0)
                             ? INADDR_ANY : inet_addr(cfg.host);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::fprintf(stderr, "waya: cannot bind %s:%d (%s). Is the port in use?\n",
                     cfg.host, cfg.port, std::strerror(errno));
        ::close(fd);
        return 1;
    }
    if (::listen(fd, 16) < 0) { std::perror("waya: listen"); ::close(fd); return 1; }

    detail::g_listen_fd = fd;
    std::signal(SIGINT, detail::on_sigint);
    std::signal(SIGPIPE, SIG_IGN);

    std::string url = "http://" + std::string(cfg.host) + ":" + std::to_string(cfg.port);
    std::fprintf(stderr, "waya: serving on %s  (Ctrl-C to stop)\n", url.c_str());
    if (cfg.open) detail::try_open_browser(url);

    for (;;) {
        int conn = ::accept(fd, nullptr, nullptr);
        if (conn < 0) { if (detail::g_listen_fd < 0) break; continue; }

        char buf[8192];
        ssize_t n = ::recv(conn, buf, sizeof(buf) - 1, 0);
        if (n <= 0) { ::close(conn); continue; }
        buf[n] = '\0';

        std::string_view path;
        std::string_view method = detail::method_and_path({buf, static_cast<size_t>(n)}, path);

        // Browsers ask for /favicon.ico; answer 204 so the log stays clean.
        std::string body, status = "200 OK", ctype = "text/html; charset=utf-8";
        if (path == "/favicon.ico") {
            status = "204 No Content"; ctype = "text/plain";
        } else {
            body = handler(Request{method, path});
        }

        if (cfg.log)
            std::fprintf(stderr, "  %.*s %.*s -> %s\n",
                         (int)method.size(), method.data(),
                         (int)path.size(), path.data(), status.c_str());

        std::string resp =
            "HTTP/1.1 " + status + "\r\n"
            "Content-Type: " + ctype + "\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Cache-Control: no-store\r\n"
            "Connection: close\r\n"
            "\r\n" + body;

        ::send(conn, resp.data(), resp.size(), 0);
        ::close(conn);
    }
    return 0;
}

/// Convenience overload: serve a single page (ignores the path). The most
/// common case for previewing an example — `serve([]{ return my_page(); })`.
template <typename Fn>
    requires requires (Fn f) { { f() }; } && (!std::is_invocable_v<Fn, const Request&>)
inline int serve(Fn view, ServeConfig cfg = {}) {
    return serve(
        [view = std::move(view)](const Request&) {
            return render::render_document(view());
        },
        cfg);
}

} // namespace waya
