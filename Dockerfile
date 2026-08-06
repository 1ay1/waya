# syntax=docker/dockerfile:1
# A production image for a waya app. Multi-stage: build with a C++26 toolchain,
# ship a tiny runtime image with just the static binary.
#
#   docker build -t my-waya-app --build-arg TARGET=counter .
#   docker run -p 8080:8080 my-waya-app
#
# The app binds 0.0.0.0 by default; -p maps it to the host. WAYA_NO_OPEN is set
# so it doesn't try to launch a browser in the container.

# ---- build stage ------------------------------------------------------------
FROM gcc:15 AS build

ARG TARGET=counter

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake ninja-build libssl-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Build ONE target (your app). TLS + gzip on for a real deployment.
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DWAYA_BUILD_TESTS=OFF -DWAYA_BUILD_EXAMPLES=ON \
    && cmake --build build --target "${TARGET}" -j

# ---- runtime stage ----------------------------------------------------------
FROM debian:bookworm-slim AS runtime

ARG TARGET=counter
RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3 zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -m -u 10001 waya

COPY --from=build /src/build/${TARGET} /usr/local/bin/waya-app

USER waya
ENV WAYA_NO_OPEN=1 WAYA_HOST=0.0.0.0 WAYA_PORT=8080
EXPOSE 8080

# Orchestrators check /healthz; SIGTERM triggers a clean shutdown.
HEALTHCHECK --interval=10s --timeout=2s --retries=3 \
    CMD ["/bin/sh", "-c", "wget -qO- http://127.0.0.1:8080/healthz || exit 1"]

ENTRYPOINT ["/usr/local/bin/waya-app"]
