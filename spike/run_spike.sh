#!/bin/sh
# waya Phase 0 spike runner.
#
#   1. Builds and runs the positive tests (must succeed).
#   2. Compiles each negative case (each MUST fail to compile).
#   3. Measures compile time against the DESIGN.md risk gate.

set -u
CXX="${CXX:-g++}"
STD="-std=c++26"
DIR="$(dirname "$0")"
PASS=0
FAIL=0

say()  { printf '%s\n' "$*"; }
ok()   { printf '  \033[32mPASS\033[0m  %s\n' "$1"; PASS=$((PASS+1)); }
bad()  { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; FAIL=$((FAIL+1)); }

say ""
say "=== 1. Positive tests (must compile and run) ==="
if $CXX $STD -O2 "$DIR/test_spike.cpp" -o /tmp/waya_spike 2>/tmp/waya_err.txt; then
    if /tmp/waya_spike >/dev/null 2>&1; then
        ok "test_spike.cpp compiles, all static_asserts hold, runtime asserts pass"
    else
        bad "test_spike.cpp compiled but failed at runtime"
    fi
else
    bad "test_spike.cpp failed to compile"
    head -20 /tmp/waya_err.txt
fi

say ""
say "=== 2. Negative tests (each MUST fail to compile) ==="

# $1 = description, $2 = body of main()
negative() {
    desc="$1"; body="$2"
    {
        printf '#include "waya_dsl.hpp"\n'
        printf 'using namespace waya;\nusing namespace waya::dsl;\n'
        printf 'int main() { %s return 0; }\n' "$body"
    } > /tmp/waya_neg.cpp
    if $CXX $STD -I"$DIR" -fsyntax-only /tmp/waya_neg.cpp 2>/dev/null; then
        bad "$desc  — COMPILED, but should have been rejected!"
    else
        ok "$desc"
    fi
}

negative "<div> inside <p>"                 'auto x = p_(div_()); (void)x;'
negative "<td> outside <tr>"                'auto x = div_(td_()); (void)x;'
negative "<li> outside a list"              'auto x = div_(li_()); (void)x;'
negative "<div> inside <ul>"                'auto x = ul_(div_()); (void)x;'
negative "<p> inside <head>"                'auto x = head_(p_()); (void)x;'
negative "<div> inside <title>"             'auto x = title_(div_()); (void)x;'
negative "children on void <br>"            'auto x = br_(text("no")); (void)x;'
negative "children on void <img>"           'auto x = img_(text("no")); (void)x;'
negative "href on <span>"                   'auto x = span_(text("s")) | href<"/u">; (void)x;'
negative "href on <div>"                    'auto x = div_() | href<"/u">; (void)x;'
negative "<tr> directly inside <div>"       'auto x = div_(tr_()); (void)x;'
negative "<body> inside <div>"              'auto x = div_(body_()); (void)x;'
negative "heading inside <p>"               'auto x = p_(h1_()); (void)x;'
negative "<table> inside <p>"               'auto x = p_(table_()); (void)x;'

say ""
say "=== 3. Compile-time gate (DESIGN.md risk #1: 500 elements < 2s) ==="

gen_page() {
    n="$1"
    {
        printf '#include "waya_dsl.hpp"\n'
        printf 'using namespace waya;\nusing namespace waya::dsl;\n'
        printf 'constexpr auto page = div_(\n'
        i=0
        while [ "$i" -lt "$n" ]; do
            [ "$i" -gt 0 ] && printf ',\n'
            printf '  div_(h3_(text("row")), p_(text("cell")), span_(text("x")))'
            i=$((i+1))
        done
        printf '\n);\n'
        printf 'int main() { return (int)render(page).size() > 0 ? 0 : 1; }\n'
    } > /tmp/waya_bench.cpp
}

for n in 25 100 166; do
    gen_page "$n"
    elems=$((n * 7 + 1))   # 1 outer + n*(div+h3+text+p+text+span+text)
    start=$(date +%s%N)
    if $CXX $STD -O1 -I"$DIR" -fsyntax-only /tmp/waya_bench.cpp 2>/dev/null; then
        end=$(date +%s%N)
        ms=$(( (end - start) / 1000000 ))
        printf '  %5d elements : %6d ms\n' "$elems" "$ms"
        if [ "$elems" -ge 500 ]; then
            if [ "$ms" -lt 2000 ]; then
                ok "500+ element page compiles in ${ms}ms (gate: < 2000ms)"
            else
                bad "500+ element page took ${ms}ms (gate: < 2000ms)"
            fi
        fi
    else
        bad "benchmark page with $elems elements failed to compile"
    fi
done

say ""
say "=== 4. Error message quality (DESIGN.md risk #2) ==="
# waya ships these flags in its CMake preset: the framework's diagnostics are
# authored by us via P2741, so GCC's own instantiation backtrace is noise.
DIAG_FLAGS="-ftemplate-backtrace-limit=1 -fno-diagnostics-show-caret -fno-diagnostics-show-line-numbers -fno-diagnostics-show-template-tree"

check_msg() {
    desc="$1"; body="$2"
    {
        printf '#include "waya_dsl.hpp"\n'
        printf 'using namespace waya;\nusing namespace waya::dsl;\n'
        printf 'int main() { %s return 0; }\n' "$body"
    } > /tmp/waya_msg.cpp
    $CXX $STD -I"$DIR" -fsyntax-only $DIAG_FLAGS /tmp/waya_msg.cpp 2>/tmp/waya_msg.txt
    n=$(grep -c 'error:' /tmp/waya_msg.txt)
    total=$(wc -l < /tmp/waya_msg.txt)
    printf '\n  %s\n' "$desc"
    printf '  %s\n' "$(grep 'error:' /tmp/waya_msg.txt | head -1 | cut -c1-200)"
    if [ "$n" -eq 1 ] && [ "$total" -le 8 ]; then
        ok "one error, $total lines total"
    else
        bad "$n errors, $total lines (gate: 1 error, <= 8 lines)"
    fi
}

check_msg "<div> inside <p>"      'auto x = p_(div_()); (void)x;'
check_msg "<td> outside <tr>"     'auto x = div_(td_()); (void)x;'
check_msg "children on void <br>" 'auto x = br_(text("no")); (void)x;'
check_msg "href on <span>"        'auto x = span_(text("s")) | href<"/u">; (void)x;'

say ""
say "================================================"
printf ' passed: %d   failed: %d\n' "$PASS" "$FAIL"
say "================================================"
[ "$FAIL" -eq 0 ]
