#!/bin/sh
# scripts/dev.sh — waya live-reload dev loop.
#
# Watches the source tree, rebuilds on any change, and restarts the target.
# The dev server injects a live-reload client, so your browser refreshes itself
# a moment after each successful rebuild — no manual rebuild, no manual refresh.
#
# Usage:
#   scripts/dev.sh [target] [build-dir]
#     target     CMake target / binary to run   (default: counter)
#     build-dir  CMake build directory          (default: build)
#
# Requires: cmake. Uses inotifywait if available (instant), else polls mtimes.

set -u
TARGET="${1:-counter}"
BUILD="${2:-build}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

WATCH="include examples src tests"
SERVER_PID=""
FIRST_DONE=""

log() { printf '\033[36mwaya dev\033[0m %b\n' "$*"; }

log "target: \033[1m$TARGET\033[0m   (pass a target to pick another: scripts/dev.sh counter)"

# Ensure the build dir is configured.
[ -f "$BUILD/CMakeCache.txt" ] || cmake -S . -B "$BUILD" >/dev/null

rebuild_and_run() {
    log "building $TARGET…"
    # Build FIRST, while the old server keeps running. Capture the result; only
    # swap servers if the build actually succeeded — a failed build must never
    # leave the browser without a server to reconnect to (that's the "stuck on
    # loading" bug). cmake's exit code is the source of truth.
    if ! cmake --build "$BUILD" --target "$TARGET" -j 2>&1 | tee /tmp/waya_build.log | grep -Ei 'error|warning:' ; then
        : # (grep exit is ignored; we check the build result below)
    fi
    if grep -qiE ' error:|Error [0-9]' /tmp/waya_build.log 2>/dev/null; then
        log "\033[31mbuild failed\033[0m — keeping the last good server up; fix errors above."
        return 1
    fi

    BIN="$BUILD/$TARGET"
    [ -x "$BIN" ] || BIN="$(find "$BUILD" -name "$TARGET" -type f -perm -u+x 2>/dev/null | head -1)"
    if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
        log "\033[31mno binary produced\033[0m — keeping the last good server up."
        return 1
    fi

    # Build succeeded — now (and only now) swap the server. The old socket drops,
    # the browser's WS client reconnects to the fresh process and reloads.
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null   # ensure the port is released before rebind
    fi
    if [ -z "$FIRST_DONE" ]; then
        "$BIN" &
        FIRST_DONE=1
    else
        WAYA_NO_OPEN=1 "$BIN" &
    fi
    SERVER_PID=$!
    log "running \033[1m$TARGET\033[0m (pid $SERVER_PID) — edit & save to reload"
}

cleanup() { [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null; exit 0; }
trap cleanup INT TERM

rebuild_and_run

# ── watch loop ───────────────────────────────────────────────────────────────
if command -v inotifywait >/dev/null 2>&1; then
    log "watching $WATCH (inotify)…  Ctrl-C to stop"
    while inotifywait -qq -r -e modify,create,delete,move $WATCH 2>/dev/null; do
        rebuild_and_run
    done
else
    log "watching $WATCH (polling; install inotify-tools for instant reload)…"
    LAST=""
    while :; do
        NOW="$(find $WATCH -type f \( -name '*.hpp' -o -name '*.cpp' \) \
                 -newer "$BUILD/CMakeCache.txt" 2>/dev/null | wc -l)$(
               find $WATCH -type f \( -name '*.hpp' -o -name '*.cpp' \) \
                 -printf '%T@\n' 2>/dev/null | sort -n | tail -1)"
        if [ "$NOW" != "$LAST" ] && [ -n "$LAST" ]; then
            rebuild_and_run
        fi
        LAST="$NOW"
        sleep 1
    done
fi
