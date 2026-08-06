#!/bin/sh
# scripts/create-waya-app.sh - scaffold a new waya app in one command.
#
# Usage:
#   scripts/create-waya-app.sh <app-name> [dir]
#
# Creates <dir>/<app-name>/ with a CMakeLists.txt that fetches waya, a working
# counter main.cpp, and a README. Then:
#   cd <app-name> && cmake -S . -B build && cmake --build build && ./build/<app-name>
#
# Pure POSIX sh, ASCII only (see scripts/dev.sh for why).

set -u

NAME="${1:?usage: create-waya-app <app-name> [dir]}"
DIR="${2:-.}"
ROOT="$DIR/$NAME"

if [ -e "$ROOT" ]; then
    echo "error: $ROOT already exists" >&2
    exit 1
fi

mkdir -p "$ROOT"

# ---- CMakeLists.txt ---------------------------------------------------------
cat > "$ROOT/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.28)
project($NAME LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(waya
    GIT_REPOSITORY https://github.com/1ay1/waya.git
    GIT_TAG        master)
FetchContent_MakeAvailable(waya)

add_executable($NAME main.cpp)
target_link_libraries($NAME PRIVATE waya::waya)
target_compile_features($NAME PRIVATE cxx_std_26)

# For HTTPS in Cmd::fetch, configure with -DWAYA_TLS=ON (needs OpenSSL).
EOF

# ---- main.cpp ---------------------------------------------------------------
cat > "$ROOT/main.cpp" <<'EOF'
// A waya app: three pure functions (init/update/view) become a live website.
#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

using namespace waya::surface;
using namespace waya::ui;

struct App {
    struct Model { int n = 0; };

    struct Inc {}; struct Dec {}; struct Reset {};
    using Msg = std::variant<Inc, Dec, Reset>;

    static Model init() { return {}; }

    static Model update(Model m, Msg msg) {
        std::visit(overload{
            [&](Inc)   { ++m.n; },
            [&](Dec)   { --m.n; },
            [&](Reset) { m.n = 0; },
        }, msg);
        return m;
    }

    static NodeRef view(const Model& m) {
        return card(
            text("Count") | fg_muted,
            text(m.n) | font(72) | bold,
            row(
                button("-",     Dec{},   Variant::secondary),
                button("reset", Reset{}, Variant::ghost),
                button("+",     Inc{})
            ) | gap(10) | center
        ) | gap(20) | center | css("margin", "10vh auto") | css("max-width", "360px")
          | css("min-height", "100dvh") | css("justify-content", "center")
          | theme(midnight());
    }
};

int main() { return live<App>({ .port = 8080 }); }
EOF

# ---- .gitignore + README ----------------------------------------------------
printf 'build/\n' > "$ROOT/.gitignore"

cat > "$ROOT/README.md" <<EOF
# $NAME

A [waya](https://github.com/1ay1/waya) app.

## Run

\`\`\`sh
cmake -S . -B build && cmake --build build
./build/$NAME          # then open http://localhost:8080
\`\`\`

Edit \`main.cpp\` and rebuild. You write three pure functions
(\`init\` / \`update\` / \`view\`); waya renders them to the browser and streams
only the delta on every interaction.
EOF

echo "Created $ROOT"
echo
echo "  cd $ROOT"
echo "  cmake -S . -B build && cmake --build build"
echo "  ./build/$NAME     # http://localhost:8080"
