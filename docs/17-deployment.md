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
- **Correct methods**: the server answers `GET` and `HEAD`; any other verb gets a
  `405 Method Not Allowed` with an `Allow: GET, HEAD` header (a live app's state
  changes travel over the socket, not HTTP verbs). `HEAD` returns the full
  headers — including the `Content-Length` of the GET body — with no body.
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

waya needs the WebSocket upgrade to pass through. nginx:

```nginx
location / {
    proxy_pass http://127.0.0.1:8080;
    proxy_http_version 1.1;
    proxy_set_header Upgrade    $http_upgrade;   # WebSocket upgrade
    proxy_set_header Connection "upgrade";
    proxy_set_header Host       $host;
    proxy_read_timeout 3600s;                    # keep long-lived sockets open
}
```

Caddy does it automatically (`reverse_proxy 127.0.0.1:8080`). Fly.io / Render /
Railway: expose port 8080, set the health check to `/healthz`, done.

## Scaling & resilience

- **Session resumption** is built in: a client that reconnects within 15 minutes
  (wifi blip, slept laptop) rebinds to its retained `Model` instead of resetting.
  No sticky sessions needed for a single instance; for multiple instances, pin
  by cookie at the proxy (a session's state lives in its owning process).
- **Bounded effect pool**: `Cmd::fetch`/`task` run on a fixed worker pool
  (`WAYA_WORKERS`), so a burst of async work can't exhaust threads.
- **Rate limiting**: each connection is capped (~120 msgs/sec, burst 60) so one
  client can't pin a core.

!!! note "The concurrency model today"
    The runtime is thread-per-connection: fine for typical apps and thousands of
    connections. For very high fan-out (tens of thousands of idle sockets), an
    epoll/kqueue reactor is the planned next step. Until then, run multiple
    instances behind a proxy to scale horizontally.
