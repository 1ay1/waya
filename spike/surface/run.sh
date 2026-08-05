#!/bin/sh
# spike/surface — proves waya's core thesis:
#   You describe WHAT to render with a tiny vocabulary; waya owns HOW (HTML,
#   CSS, canvas — whatever renders best). Same view, any backend, unchanged.
#   Powerful enough for anything (a chart is one primitive), simple as hell.

set -u
CXX="${CXX:-g++}"; DIR="$(dirname "$0")"
echo ""
echo "=== surface spike: one view(), two backends, minimal deltas ==="
if $CXX -std=c++26 -O2 "$DIR/test_surface.cpp" -o /tmp/waya_surf 2>/tmp/se.txt; then
    /tmp/waya_surf
else
    echo "compile failed:"; head -20 /tmp/se.txt; exit 1
fi
