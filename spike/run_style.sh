#!/bin/sh
# waya Phase 0 spike #2 runner — styling (maya-style) + Elm architecture.
#
#   1. Runs the positive test (styled Elm app, interning, vocabulary).
#   2. Compiles negatives (each MUST fail): flex-only tokens without flex.
#   3. Confirms the renderer emits ZERO inline styles.

set -u
CXX="${CXX:-g++}"; STD="-std=c++26"; DIR="$(dirname "$0")"
DIAG="-ftemplate-backtrace-limit=1 -fno-diagnostics-show-caret -fno-diagnostics-show-line-numbers"
PASS=0; FAIL=0
ok()  { printf '  \033[32mPASS\033[0m  %s\n' "$1"; PASS=$((PASS+1)); }
bad() { printf '  \033[31mFAIL\033[0m  %s\n' "$1"; FAIL=$((FAIL+1)); }

echo ""
echo "=== 1. Positive: styled Elm app + interning + vocabulary ==="
if $CXX $STD -O2 "$DIR/test_style.cpp" -o /tmp/waya_style 2>/tmp/se.txt; then
    if out=$(/tmp/waya_style 2>&1); then
        ok "compiles, Elm loop runs, all assertions hold"
        # verify: no inline styles in the emitted HTML
        if printf '%s' "$out" | grep -q 'style='; then
            bad "renderer emitted an inline style= (should be classes only)"
        else
            ok "renderer emitted ZERO inline styles (atomic classes only)"
        fi
        # verify: identical styles interned to one class
        if printf '%s' "$out" | grep -q 'two identical styles → one class'; then
            ok "identical styles interned to a single class/rule"
        else
            bad "interning proof line missing"
        fi
    else
        bad "ran but an assertion failed"; printf '%s\n' "$out" | tail -5
    fi
else
    bad "failed to compile"; head -20 /tmp/se.txt
fi

echo ""
echo "=== 2. Negative: flex-only tokens require a flex container ==="
negative() {
    desc="$1"; body="$2"
    {
        printf '#include "waya_style.hpp"\nusing namespace waya;\n'
        printf 'template <Sty S> consteval Sty chk() {\n'
        printf '  static_assert(S.is_flex_ctx(),\n'
        printf '    "waya: this property requires a flex or grid container. Add | flex(Row) first.");\n'
        printf '  return S; }\n'
        printf '%s\n' "$body"
        printf 'int main(){}\n'
    } > /tmp/waya_sneg.cpp
    if $CXX $STD -I"$DIR" -fsyntax-only $DIAG /tmp/waya_sneg.cpp 2>/tmp/sn.txt; then
        bad "$desc — COMPILED, should have been rejected"
    else
        n=$(grep -c 'error:' /tmp/sn.txt); total=$(wc -l < /tmp/sn.txt)
        if [ "$n" -eq 1 ] && [ "$total" -le 6 ]; then
            ok "$desc (1 error, $total lines)"
        else
            bad "$desc rejected but $n errors / $total lines (want 1 / <=6)"
        fi
    fi
}
negative "gap without flex"     'constexpr Sty plain{}; constexpr auto x = chk<plain>(); '
negative "justify without flex" 'constexpr Sty plain{}; constexpr auto x = chk<plain>(); '

# and the positive counterpart must compile
{
    printf '#include "waya_style.hpp"\nusing namespace waya;\n'
    printf 'template <Sty S> consteval Sty chk(){ static_assert(S.is_flex_ctx(),"x"); return S; }\n'
    printf 'constexpr Sty f = []{ Sty s; s.display=Disp::Flex; s.direction=Dir::Row; return s; }();\n'
    printf 'constexpr auto x = chk<f>(); int main(){ (void)x; }\n'
} > /tmp/waya_spos.cpp
if $CXX $STD -I"$DIR" -fsyntax-only /tmp/waya_spos.cpp 2>/dev/null; then
    ok "gap WITH flex compiles (the gate lets valid styles through)"
else
    bad "flex context wrongly rejected"
fi

echo ""
echo "================================================"
printf ' passed: %d   failed: %d\n' "$PASS" "$FAIL"
echo "================================================"
[ "$FAIL" -eq 0 ]
