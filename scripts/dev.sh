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
# Environment:
#   WAYA_PORT   port the app serves on   (passed through to the app)
#   WAYA_HOST   host the app binds       (passed through to the app)
#   JOBS        parallel build jobs      (default: auto-detected core count)
#
# Portability: pure POSIX sh. Runs on Linux, macOS, the BSDs, and Windows
# under MSYS2 / Git-Bash / Cygwin (handles the .exe suffix and multi-config
# build layouts). Uses fswatch or inotifywait for instant reload when present,
# else falls back to portable mtime polling. (Native Windows: scripts/dev.ps1.)
#
# NOTE: this file is intentionally pure ASCII. A non-ASCII byte inside a
# double-quoted string breaks parameter parsing under `set -u` on BSD sh.

set -u

TARGET="${1:-counter}"
BUILD="${2:-build}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

# ── Colours (only on a TTY, so piped/CI logs stay clean) ─────────────────────
if [ -t 1 ]; then C='\033[36m'; B='\033[1m'; R='\033[31m'; Y='\033[33m'; Z='\033[0m'
else C=''; B=''; R=''; Y=''; Z=''; fi
log()  { printf '%bwaya dev%b %b\n' "$C" "$Z" "$*"; }
die()  { printf '%bwaya dev%b %berror%b %b\n' "$C" "$Z" "$R" "$Z" "$*" >&2; exit 1; }
warn() { printf '%bwaya dev%b %bwarn%b  %b\n'  "$C" "$Z" "$Y" "$Z" "$*" >&2; }

# ── Preflight: the tools we cannot run without ───────────────────────────────
command -v cmake >/dev/null 2>&1 || die "cmake not found on PATH - install CMake >= 3.28."

# ── Parallel job count (auto-detect; overridable via JOBS) ───────────────────
if [ -n "${JOBS:-}" ]; then
    :
elif command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc 2>/dev/null)"
elif command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null)"   # macOS / BSD
fi
case "${JOBS:-}" in ''|*[!0-9]*) JOBS=4 ;; esac   # sane fallback

# ── Directories worth watching (some may be absent) ──────────────────────────
WATCH=""
for d in include examples tests src; do
    [ -d "$d" ] && WATCH="$WATCH $d"
done
WATCH="${WATCH# }"
[ -n "$WATCH" ] || die "nothing to watch: no include/ examples/ tests/ src/ under $ROOT."

SERVER_PID=""
FIRST_DONE=""
TMP="${TMPDIR:-/tmp}/waya_build.$$.log"

# ── Configure the build dir if needed (surface configure errors!) ────────────
ensure_configured() {
    [ -f "$BUILD/CMakeCache.txt" ] && return 0
    # A waya app is a live server, so build optimised by default — an -O0 build
    # feels laggy for real-time UIs. Override with WAYA_BUILD_TYPE=Debug.
    _btype="${WAYA_BUILD_TYPE:-Release}"
    log "configuring ${B}${BUILD}${Z} (${_btype}, first run)..."
    if ! cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE="$_btype" >"$TMP" 2>&1; then
        tail -n 25 "$TMP" 2>/dev/null
        die "cmake configure failed (see above). Fix the error, then re-run."
    fi
}

# ── Locate the produced binary across layouts / platforms ────────────────────
#   single-config:  build/<target>            (Unix Makefiles, single Ninja)
#   multi-config:   build/<cfg>/<target>      (Ninja Multi-Config, VS, Xcode)
#   windows:        any of the above + .exe suffix
find_binary() {
    for cand in \
        "$BUILD/$TARGET" "$BUILD/$TARGET.exe" \
        "$BUILD"/*/"$TARGET" "$BUILD"/*/"$TARGET.exe"
    do
        [ -f "$cand" ] && [ -x "$cand" ] && { printf '%s' "$cand"; return 0; }
    done
    # Last resort: search the tree for an executable of that name.
    _b="$(find "$BUILD" \( -name "$TARGET" -o -name "$TARGET.exe" \) -type f 2>/dev/null | head -1)"
    [ -n "$_b" ] && [ -x "$_b" ] && { printf '%s' "$_b"; return 0; }
    return 1
}

# ── Validate the target is real, with a helpful list on typo ─────────────────
validate_target() {
    # Targets are examples/*.cpp (CMake auto-globs them). Derive the known list
    # the same way so a typo is caught early with a helpful hint.
    known=""
    for _f in examples/*.cpp; do
        [ -f "$_f" ] || continue
        _n="${_f##*/}"; known="$known ${_n%.cpp}"
    done
    [ -n "$known" ] || return 0                       # can't introspect; skip
    for t in $known; do [ "$t" = "$TARGET" ] && return 0; done
    # Not a known example - could still be a custom target, so warn (don't die).
    warn "'${B}${TARGET}${Z}' is not a known example target."
    warn "known targets:${known}"
    warn "continuing anyway in case it's a custom target..."
}

rebuild_and_run() {
    log "building ${B}${TARGET}${Z}..."
    # Build FIRST, while the old server keeps running; only swap servers if the
    # build actually succeeded - a failed build must never leave the browser
    # without a server to reconnect to.
    if ! cmake --build "$BUILD" --target "$TARGET" -j "$JOBS" >"$TMP" 2>&1; then
        log "${R}build failed${Z} - keeping the last good server up; errors:"
        tail -n 25 "$TMP" 2>/dev/null || cat "$TMP"
        return 1
    fi

    BIN="$(find_binary)" || {
        log "${R}no binary produced${Z} for '${TARGET}' - keeping the last good server up."
        return 1
    }

    # Build succeeded - now (and only now) swap the server. The old socket drops,
    # the browser's WS client reconnects to the fresh process and reloads.
    stop_server
    if [ -z "$FIRST_DONE" ]; then
        "$BIN" &
        FIRST_DONE=1
    else
        WAYA_NO_OPEN=1 "$BIN" &   # don't reopen the browser on every reload
    fi
    SERVER_PID=$!
    log "running ${B}${TARGET}${Z} (pid $SERVER_PID) - edit & save to reload"
}

# ── Stop the running server and wait for the port to be released ─────────────
stop_server() {
    [ -n "$SERVER_PID" ] || return 0
    kill "$SERVER_PID" 2>/dev/null
    # Give it a moment; escalate to KILL if it ignores TERM (e.g. blocked in a
    # syscall) so the port is definitely free before we rebind.
    i=0
    while kill -0 "$SERVER_PID" 2>/dev/null; do
        i=$((i + 1))
        [ "$i" -ge 20 ] && kill -9 "$SERVER_PID" 2>/dev/null && break
        sleep 0.1 2>/dev/null || sleep 1
    done
    wait "$SERVER_PID" 2>/dev/null
    SERVER_PID=""
}

cleanup() {
    stop_server
    rm -f "$TMP" 2>/dev/null
    exit 0
}
trap cleanup INT TERM HUP
trap 'stop_server; rm -f "$TMP" 2>/dev/null' EXIT

# ── Go ───────────────────────────────────────────────────────────────────────
log "target: ${B}${TARGET}${Z}   jobs: ${JOBS}   (pass a target: scripts/dev.sh <name>)"
ensure_configured
validate_target

rebuild_and_run || die "initial build failed - fix the errors above and re-run."

# ── Watch loop ────────────────────────────────────────────────────────────────
# Prefer a native watcher; fall back to portable mtime polling everywhere else.
if command -v fswatch >/dev/null 2>&1; then
    # fswatch: macOS (brew install fswatch) and Linux. -o batches events.
    log "watching ${WATCH} (fswatch)...  Ctrl-C to stop"
    # Feed events through a FIFO instead of a pipe: a `cmd | while` puts the
    # loop in a subshell, so rebuild_and_run's SERVER_PID / FIRST_DONE updates
    # would be lost (stale pid on Ctrl-C, browser reopened every reload). The
    # FIFO keeps the loop in THIS shell where that state persists.
    FIFO="${TMPDIR:-/tmp}/waya_fswatch.$$.fifo"
    mkfifo "$FIFO" 2>/dev/null || die "could not create FIFO for fswatch at $FIFO"
    trap 'stop_server; rm -f "$TMP" "$FIFO" 2>/dev/null' EXIT
    # shellcheck disable=SC2086
    fswatch -o -r $WATCH > "$FIFO" &
    FSWATCH_PID=$!
    while read -r _; do
        rebuild_and_run
    done < "$FIFO"
    kill "$FSWATCH_PID" 2>/dev/null
elif command -v inotifywait >/dev/null 2>&1; then
    # inotifywait: Linux (apt install inotify-tools).
    log "watching ${WATCH} (inotify)...  Ctrl-C to stop"
    # shellcheck disable=SC2086
    while inotifywait -qq -r -e modify,create,delete,move,close_write $WATCH 2>/dev/null; do
        rebuild_and_run
    done
else
    # Portable polling: a fingerprint of file count + newest mtime. No GNU-only
    # find flags. `stat` output format differs by platform, so we probe once and
    # cache which variant works (avoids a failed exec per file every second).
    log "watching ${WATCH} (polling; install fswatch or inotify-tools for instant reload)...  Ctrl-C to stop"

    STAT_MODE=""
    probe_stat() {
        _t="$(find $WATCH -type f 2>/dev/null | head -1)"
        [ -n "$_t" ] || { STAT_MODE="none"; return; }
        if stat -f %m "$_t" >/dev/null 2>&1;   then STAT_MODE="bsd"
        elif stat -c %Y "$_t" >/dev/null 2>&1; then STAT_MODE="gnu"
        else STAT_MODE="none"; fi
    }
    file_mtime() {
        case "$STAT_MODE" in
            bsd) stat -f %m "$1" 2>/dev/null ;;
            gnu) stat -c %Y "$1" 2>/dev/null ;;
            *)   echo 0 ;;
        esac
    }
    # shellcheck disable=SC2086
    fingerprint() {
        n=0; newest=0
        # Read NUL-safe-ish: source files rarely contain newlines in their names.
        for f in $(find $WATCH -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' -o -name '*.cc' \) 2>/dev/null); do
            n=$((n + 1))
            m="$(file_mtime "$f")"
            case "$m" in ''|*[!0-9]*) m=0 ;; esac
            [ "$m" -gt "$newest" ] && newest="$m"
        done
        printf '%s:%s' "$n" "$newest"
    }

    probe_stat
    [ "$STAT_MODE" = "none" ] && warn "stat unsupported here; polling on file-count only."
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
