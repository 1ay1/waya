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
- **Rate limiting**: each connection is capped (~120 msgs/sec, burst 60) so one
  client can't pin a core.
- **Connection ceiling + backpressure**: past `WAYA_MAX_CONN` (default 10000) new
  connections get a fast `503 Retry-After: 2` instead of the server falling over.

!!! note "The concurrency model"
    The runtime is an **epoll gate + bounded worker pool**. Idle and
    between-request keep-alive connections are *parked in epoll* — they cost a
    socket + a little memory, **not a thread** — and are handed to a fixed worker
    pool (`WAYA_WORKERS`, default `4×CPU`) only when a request is actually ready.
    So tens of thousands of idle keep-alive sockets are cheap. A live WebSocket
    session (long-lived + stateful) runs on its own thread, off the pool. For
    still more throughput, run multiple instances behind the proxy — no sticky
    sessions needed (session resumption is per-tab and in-process).
