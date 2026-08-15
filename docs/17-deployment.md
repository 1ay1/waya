# Deployment

A waya app is a single self-contained binary that speaks HTTP + WebSocket on one
port. Deploying it is deploying one process — no Node runtime, no asset pipeline,
no separate API server.

## The binary

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWAYA_TLS=ON -DWAYA_GZIP=ON
cmake --build build --target my_app
./build/my_app
```

- **`-DWAYA_TLS=ON`** links OpenSSL so `Cmd::fetch`/`post` can call HTTPS APIs.
- **`-DWAYA_GZIP=ON`** gzip-compresses the SSR first paint.

Runtime knobs are environment variables (no config file):

| Var | Meaning |
|-----|---------|
| `WAYA_PORT` | Listen port (default 8080) |
| `WAYA_HOST` | Bind address (default `0.0.0.0`) |
| `WAYA_NO_OPEN` | Don't try to open a browser (set this in production) |
| `WAYA_WORKERS` | Size of the effect thread pool (default: CPU count) |
| `WAYA_LOG` | Set to enable one-line access logs (method, path, status) to stderr |
| `WAYA_ALLOWED_ORIGINS` | Comma-separated allowlist of `Origin`s permitted to open a WebSocket. Unset = allow all (dev). **Set this in production** to block cross-site WS hijacking. |
| `WAYA_CONN_RATE` | Per-IP new-connection rate, conns/sec (default 20). `0` disables (behind a trusted LB that already limits). |
| `WAYA_CONN_BURST` | Per-IP connection burst allowance (default 40). |
| `WAYA_MAX_CONN` | Global live-connection ceiling (default 10000); excess gets a fast `503`. |
| `WAYA_METRICS` | Set to expose `GET /metrics` (Prometheus). Off by default (leaks traffic shape). |

## HTTP hardening

Every response the runtime serves is production-hardened out of the box — no
middleware to add:

- **Security headers** on every page: `X-Content-Type-Options: nosniff`,
  `X-Frame-Options: SAMEORIGIN`, `Content-Security-Policy: frame-ancestors 'self'`
  (clickjacking defence), and `Referrer-Policy: strict-origin-when-cross-origin`.
- **Correct methods**: the server answers `GET` and `HEAD`; `OPTIONS` returns
  `204` with an `Allow: GET, HEAD, OPTIONS` header; any other verb gets a
  `405 Method Not Allowed` with the same `Allow` (a live app's state changes
  travel over the socket, not HTTP verbs). `HEAD` returns the full headers —
  including the `Content-Length` of the GET body — with no body.
- **Persistent connections**: HTTP/1.1 keep-alive is on — one socket serves many
  requests (the proxy pools the upstream connection). The response advertises
  `Connection: keep-alive` + `Keep-Alive: timeout=60`; a client's
  `Connection: close` is honored.
- **Conditional requests**: the SSR page carries a weak `ETag`; a revalidating
  `If-None-Match` gets a `304 Not Modified` with no body, so a caching proxy or
  browser skips re-downloading an unchanged first paint.
- **Every response carries `Date:` and `Server: waya`** (RFC 9110 origin MUSTs).
- **Bounded, abuse-resistant parsing**: a request is read with hard caps — 64 KB
  of headers, 100 header fields, 8 KB per line, 8 MB body. A slow-loris or
  oversized request is answered `431`/`400` and dropped up front, never pinning
  a worker. Socket timeouts (`SO_RCVTIMEO`/`SNDTIMEO`, 60s) back this at the TCP
  layer, and `TCP_NODELAY` removes Nagle latency on the small live frames.
- **`X-Forwarded-*` aware**: behind a proxy the effective scheme/host/client-IP
  come from `X-Forwarded-Proto`/`-Host`/`-For` — set them at the proxy (the
  bundled configs do) and redirects, cookies and logs stay correct.
- **Caching**: the live HTML is `Cache-Control: no-store` (it's personalised and
  upgrades itself over the socket, so it must never be served stale); the SEO
  files (`/robots.txt`, `/sitemap.xml`) and `/favicon.ico` are cacheable.
- **`/favicon.ico`** is answered with a cheap `204 No Content` so a browser's
  automatic request isn't rendered as the app or logged as a 404.
- **Access logs**: set `WAYA_LOG=1` for a structured line per request
  (`waya: <iso-ts> <method> <path> <status>`) that composes with journald/Docker
  log collection. Off by default so a dev run stays quiet.

## Health check

The runtime serves **`GET /healthz`** → `200 ok` without rendering the app — wire
it to your load balancer / orchestrator liveness probe. On `SIGTERM` (what Docker
and systemd send to stop) the process shuts down cleanly.

## Metrics

Set **`WAYA_METRICS=1`** (or add `static bool expose_metrics(){ return true; }`
to your Program) to expose **`GET /metrics`** in Prometheus exposition format:

```
waya_http_requests_total   counter   every HTTP response
waya_http_errors_total     counter   responses with status >= 500
waya_ws_sessions_total     counter   WebSocket sessions ever opened
waya_ws_sessions_live      gauge     currently-open sessions
waya_rate_limited_total    counter   per-IP limiter rejections
waya_uptime_seconds        gauge
```

It's **off by default** — metrics leak traffic shape, so scrape it on an internal
network or behind auth (a proxy `location /metrics` allowlist, or expose it only
on a private interface).

## Security

- **WebSocket Origin allowlist** — WebSockets are exempt from CORS, so by default
  any origin can open a socket and drive your app as the logged-in user. Set
  `WAYA_ALLOWED_ORIGINS=https://app.example.com` (comma-separated for several)
  and a handshake from any other origin gets a `403`. **Do this in production.**
- **Per-IP connection rate limiting** — a single source can't flood the accept
  path (each connection costs an SSR render). `WAYA_CONN_RATE` conns/sec per IP
  (default 20), burst `WAYA_CONN_BURST` (40); excess gets a fast `429`. Set rate
  to `0` if a trusted LB already rate-limits.
- **Security headers** on every response: CSP, HSTS, `X-Frame-Options`,
  `nosniff`, referrer policy (see *HTTP hardening* above).

## Docker

A production `Dockerfile` ships in the repo root: a multi-stage build (a C++26
toolchain image builds the binary; a slim runtime image ships just it, as a
non-root user, with a `HEALTHCHECK` on `/healthz`).

```sh
docker build -t my-waya-app --build-arg TARGET=my_app .
docker run -p 8080:8080 my-waya-app
```

## Behind a reverse proxy

waya is an **origin** server: it speaks clean HTTP/1.1 + WebSocket on one port
and expects a reverse proxy in front to terminate TLS, do HTTP/2 & /3, serve
edge static, and rate-limit at the edge. Ready-to-use configs ship in
[`deploy/`](https://github.com/) — `Caddyfile` (auto-HTTPS), `nginx.conf`, and a
`docker-compose.yml` that runs the app behind Caddy.

The two things a proxy MUST do: pass the WebSocket **Upgrade** through, and set
the **`X-Forwarded-*`** headers. nginx:

```nginx
location / {
    proxy_pass http://127.0.0.1:8080;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;        # WebSocket upgrade
    proxy_set_header Connection $connection_upgrade;  # map: keep-alive vs upgrade
    proxy_set_header Host              $host;
    proxy_set_header X-Forwarded-Proto $scheme;       # so waya knows https
    proxy_set_header X-Forwarded-Host  $host;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_read_timeout 3600s;                         # keep long-lived sockets open
}
```

Caddy does the upgrade + `X-Forwarded-*` automatically (`reverse_proxy
127.0.0.1:8080`). Fly.io / Render / Railway: expose port 8080, set the health
check to `/healthz`, done.

## Scaling & resilience

- **Session resumption** is built in: a client that reconnects within 15 minutes
  (wifi blip, slept laptop) rebinds to its retained `Model` instead of resetting.
  No sticky sessions needed for a single instance; for multiple instances, pin
  by cookie at the proxy (a session's state lives in its owning process).
- **Bounded effect pool**: `Cmd::fetch`/`task` run on a fixed worker pool
  (`WAYA_WORKERS`), so a burst of async work can't exhaust threads.
- **Rate limiting**: each WebSocket connection is capped (~120 msgs/sec, burst
  60) so one client can't pin a core; and new connections are rate-limited
  **per IP** at accept (`WAYA_CONN_RATE`/`WAYA_CONN_BURST`) so one source can't
  flood the SSR path.
- **Connection ceiling + backpressure**: past `WAYA_MAX_CONN` (default 10000) new
  connections get a fast `503` instead of the server falling over; a slow
  WebSocket reader that stalls its socket is dropped (bounded send timeout) so it
  can't pin a render thread.
- **Error boundary**: a throwing `view()`/`update()` renders an error card for
  that one session instead of taking down the process or other sessions.
- **Timed effects on a shared scheduler**: `Cmd::after` and subscription timers
  run on a single min-heap scheduler thread, not one OS thread each, so many
  timers across many sessions stay cheap.

!!! note "The concurrency model"
    The runtime is an **epoll gate + bounded worker pool**. Idle and
    between-request keep-alive connections are *parked in epoll* — they cost a
    socket + a little memory, **not a thread** — and are handed to a fixed worker
    pool (`WAYA_WORKERS`, default `4×CPU`) only when a request is actually ready.
    So tens of thousands of idle keep-alive sockets are cheap. A live WebSocket
    session (long-lived + stateful) runs on its own thread, off the pool. For
    still more throughput, run multiple instances behind the proxy — no sticky
    sessions needed (session resumption is per-tab and in-process).
