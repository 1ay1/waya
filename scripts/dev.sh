#!/bin/sh
# scripts/dev.sh - waya live-reload dev loop.
#
# Watches the source tree, rebuilds on any change, and restarts the target.
# The dev server injects a live-reload client, so your browser refreshes itself
# a moment after each successful rebuild - no manual rebuild, no manual refresh.
#
# Usage:
#   scripts/dev.sh [target] [build-dir]
#     target     CMake target / binary to run   (default: counter)
#     build-dir  CMake build directory          (default: build)
#
# Portable POSIX sh: runs on macOS, Linux, and the BSDs. Uses fswatch or
# inotifywait for instant reload when present, else falls back to portable
# mtime polling. (Windows: use scripts/dev.ps1.)
#
# NOTE: this file is intentionally pure ASCII. A non-ASCII byte inside a
# double-quoted string breaks parameter parsing under `set -u` on BSD sh.

set -u

TARGET="${1:-counter}"
BUILD="${2:-build}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

# Only watch directories that actually exist (src/ may be absent).
WATCH=""
for d in include examples tests; do
    [ -d "$d" ] && WATCH="$WATCH $d"
done
WATCH="${WATCH# }"

SERVER_PID=""
FIRST_DONE=""
TMP="${TMPDIR:-/tmp}/waya_build.$$.log"

# Colour only when stdout is a TTY (so piped/CI logs stay clean).
if [ -t 1 ]; then C='\033[36m'; B='\033[1m'; R='\033[31m'; Z='\033[0m'; else C=''; B=''; R=''; Z=''; fi
log() { printf '%bwaya dev%b %b\n' "$C" "$Z" "$*"; }

log "target: ${B}${TARGET}${Z}   (pass a target to pick another: scripts/dev.sh <name>)"

# Ensure the build dir is configured.
[ -f "$BUILD/CMakeCache.txt" ] || cmake -S . -B "$BUILD" >/dev/null

rebuild_and_run() {
    log "building ${TARGET}..."
    # Build FIRST, while the old server keeps running; only swap servers if the
    # build actually succeeded - a failed build must never leave the browser
    # without a server to reconnect to. cmake's exit status is the source of
    # truth (captured before the pipe to tee, via a status file).
    if cmake --build "$BUILD" --target "$TARGET" -j >"$TMP" 2>&1; then
        :
    else
        log "${R}build failed${Z} - keeping the last good server up; errors:"
        # show the tail of the log so the error is visible
        tail -n 20 "$TMP" 2>/dev/null || cat "$TMP"
        return 1
    fi

    BIN="$BUILD/$TARGET"
    if [ ! -x "$BIN" ]; then
        # -perm is GNU/BSD-incompatible; just take the first matching regular file.
        BIN="$(find "$BUILD" -name "$TARGET" -type f 2>/dev/null | head -1)"
    fi
    if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
        log "${R}no binary produced${Z} - keeping the last good server up."
        return 1
    fi

    # Build succeeded - now (and only now) swap the server. The old socket drops,
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
    log "running ${B}${TARGET}${Z} (pid $SERVER_PID) - edit & save to reload"
}

cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
    rm -f "$TMP" 2>/dev/null
    exit 0
}
trap cleanup INT TERM

rebuild_and_run

# ---- watch loop -------------------------------------------------------------
# Prefer a native watcher; fall back to portable mtime polling everywhere else.
if command -v fswatch >/dev/null 2>&1; then
    # fswatch: macOS (brew install fswatch) and Linux. -o batches events.
    log "watching ${WATCH} (fswatch)...  Ctrl-C to stop"
    # shellcheck disable=SC2086
    fswatch -o -r $WATCH | while read -r _; do
        rebuild_and_run
    done
elif command -v inotifywait >/dev/null 2>&1; then
    # inotifywait: Linux (apt install inotify-tools).
    log "watching ${WATCH} (inotify)...  Ctrl-C to stop"
    # shellcheck disable=SC2086
    while inotifywait -qq -r -e modify,create,delete,move $WATCH 2>/dev/null; do
        rebuild_and_run
    done
else
    # Portable polling: a fingerprint of file count + newest mtime. Works on
    # macOS/BSD/Linux with no GNU-only find flags (no -printf, no -newer ref).
    log "watching ${WATCH} (polling; install fswatch or inotify-tools for instant reload)...  Ctrl-C to stop"
    fingerprint() {
        # count of source files + the newest mtime among them. `stat` differs
        # by platform, so try BSD (-f %m) then GNU (-c %Y).
        files=$(find $WATCH -type f \( -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null)
        n=$(printf '%s\n' "$files" | grep -c . 2>/dev/null)
        newest=0
        for f in $files; do
            m=$(stat -f %m "$f" 2>/dev/null || stat -c %Y "$f" 2>/dev/null || echo 0)
            [ "$m" -gt "$newest" ] 2>/dev/null && newest=$m
        done
        printf '%s:%s' "$n" "$newest"
    }
    LAST="$(fingerprint)"
    while :; do
        sleep 1
        NOW="$(fingerprint)"
        if [ "$NOW" != "$LAST" ]; then
            LAST="$NOW"
            rebuild_and_run
        fi
    done
fi
