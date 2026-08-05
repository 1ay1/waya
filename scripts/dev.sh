#!/bin/sh
# scripts/dev.sh — waya live-reload dev loop.
#
# Watches the source tree, rebuilds on any change, and restarts the target.
# The dev server injects a live-reload client, so your browser refreshes itself
# a moment after each successful rebuild — no manual rebuild, no manual refresh.
#
# Usage:
#   scripts/dev.sh [target] [build-dir]
#     target     CMake target / binary to run   (default: hello)
#     build-dir  CMake build directory          (default: build)
#
# Requires: cmake. Uses inotifywait if available (instant), else polls mtimes.

set -u
TARGET="${1:-hello}"
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
    if cmake --build "$BUILD" --target "$TARGET" -j 2>&1 | grep -Ei 'error|warning:' ; then
        :  # surface compiler output
    fi
    # Did the build actually produce the binary?
    BIN="$BUILD/$TARGET"
    [ -x "$BIN" ] || BIN="$(find "$BUILD" -name "$TARGET" -type f -perm -u+x 2>/dev/null | head -1)"
    if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
        log "\033[31mbuild failed\033[0m — fix errors above; watching for changes…"
        return 1
    fi

    # Restart the server. The old socket drops → the browser's WS client
    # reconnects to the fresh process and reloads (see live_ws_client), or the
    # HTTP dev server's live-reload heartbeat fires — either way the page updates.
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        # give the port a moment to free (SO_REUSEADDR makes this instant anyway)
        sleep 0.2
    fi
    # Only the first launch opens a browser tab; restarts reuse the open one.
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
