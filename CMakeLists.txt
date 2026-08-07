cmake_minimum_required(VERSION 3.28)

project(waya
    VERSION 0.1.0
    DESCRIPTION "A C++26 server-side web framework with a type-state HTML DSL"
    LANGUAGES CXX)

# A waya app is a LIVE server: it runs view/diff/compress on every frame, so an
# unoptimised build feels laggy for real-time UIs (30fps animations, etc.).
# Default to Release when the user didn't pick a config — `./waya build/run` then
# gets -O2 automatically. Override with -DCMAKE_BUILD_TYPE=Debug when debugging.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ── The library: two concerns, one umbrella ─────────────────────────────────
# waya is an SSR/UI-rendering engine. The VIEW layer (node/dom/diff/layout/
# sugar — templated on the app's Program) is header-only and lives in the
# INTERFACE target `waya_dsl`: a consumer that only renders to an HTML string
# pays zero socket/stream compile cost. The RUNTIME (sockets, accept loop,
# sessions, HTTP/WS transport) is a SEPARATED CONCERN compiled ONCE into the
# STATIC target `waya_runtime`, behind a type-erased façade so no app template
# ever drags the transport through a header. `waya::waya` is the umbrella that
# links both, so existing `target_link_libraries(app PRIVATE waya::waya)` is
# unchanged. (Mirrors maya's compiled-lib layout.)

# The pure, header-only view/DSL engine.
add_library(waya_dsl INTERFACE)
add_library(waya::dsl ALIAS waya_dsl)
target_include_directories(waya_dsl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# The compiled runtime — sources are added here as non-templated code moves out
# of the headers into src/. Empty is legal (an OBJECT-less STATIC lib) until the
# first .cpp lands; kept explicit (not GLOB) like maya's MAYA_LIB_SOURCES.
set(WAYA_RUNTIME_SOURCES
    src/net/http.cpp
    src/net/ws.cpp
    # src/json.cpp
    # src/surface/http_util.cpp
    # src/surface/session.cpp
    # src/surface/hub.cpp
    # src/surface/pool.cpp
    # src/surface/runtime.cpp
)
if(WAYA_RUNTIME_SOURCES)
    add_library(waya_runtime STATIC ${WAYA_RUNTIME_SOURCES})
else()
    # No sources yet: a header-only shim so linking waya::runtime is always valid.
    add_library(waya_runtime INTERFACE)
endif()
add_library(waya::runtime ALIAS waya_runtime)
# C++26 module scanning is pure overhead here and has bitten maya with
# nondeterministic rebuilds — waya uses no modules, so turn it off.
if(NOT TARGET waya_runtime OR NOT WAYA_RUNTIME_SOURCES)
else()
    set_target_properties(waya_runtime PROPERTIES CXX_SCAN_FOR_MODULES OFF)
endif()

# waya_runtime is the ONE target that must see the sockets/transport concern; it
# builds on the view engine.
if(WAYA_RUNTIME_SOURCES)
    target_link_libraries(waya_runtime PUBLIC waya_dsl)
else()
    target_link_libraries(waya_runtime INTERFACE waya_dsl)
endif()

# The umbrella: view engine + runtime. This is what apps link.
add_library(waya INTERFACE)
add_library(waya::waya ALIAS waya)
target_link_libraries(waya INTERFACE waya_dsl waya_runtime)
target_include_directories(waya INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# ── C++26, requested so it NEVER breaks configure ───────────────────────────
# We can't just do `target_compile_features(waya INTERFACE cxx_std_26)`: if the
# running CMake predates C++26 awareness (or the compiler's feature table lacks
# the entry, as Apple clang 21 does), the *Generate* step hard-fails with
# "feature cxx_std_26 is not known" - even though the compiler happily accepts
# -std=c++2c. So we request the feature only when this CMake actually knows it,
# and otherwise inject the raw standard flag ourselves. Either path lands on
# C++26; neither can abort configuration.
if("cxx_std_26" IN_LIST CMAKE_CXX_KNOWN_FEATURES)
    target_compile_features(waya_dsl INTERFACE cxx_std_26)
else()
    if(MSVC)
        target_compile_options(waya_dsl INTERFACE /std:c++latest)
    else()
        # -std=c++26 on compilers that spell it that way, -std=c++2c on the
        # ones that only know the pre-ratification name (Apple clang, older
        # Clang/GCC). We probe once and cache the winner.
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-std=c++26" WAYA_HAS_STD_CXX26)
        if(WAYA_HAS_STD_CXX26)
            target_compile_options(waya_dsl INTERFACE -std=c++26)
        else()
            target_compile_options(waya_dsl INTERFACE -std=c++2c)
        endif()
    endif()
endif()

# The live runtime (surface/live.hpp) uses std::thread — one thread per
# connection so a single open socket can't block the server. Link Threads for
# any consumer that pulls it in.
find_package(Threads REQUIRED)
target_link_libraries(waya_dsl INTERFACE Threads::Threads)

# ── Optional runtime features ───────────────────────────────────────
# TLS enables HTTPS in Cmd::fetch/post (real external APIs). Off by default so
# the base library has zero external deps; turn on with -DWAYA_TLS=ON.
option(WAYA_TLS "Enable HTTPS in the HTTP client (requires OpenSSL)" OFF)
if(WAYA_TLS)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(waya_dsl INTERFACE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(waya_dsl INTERFACE WAYA_TLS)
endif()

# GZIP compresses the SSR first paint (smaller first byte). Requires zlib.
option(WAYA_GZIP "Gzip-compress the SSR HTML (requires zlib)" OFF)
if(WAYA_GZIP)
    find_package(ZLIB REQUIRED)
    target_link_libraries(waya_dsl INTERFACE ZLIB::ZLIB)
    target_compile_definitions(waya_dsl INTERFACE WAYA_GZIP)
endif()

# ── Developer warning / sanitizer options ───────────────────────────────────
# An INTERFACE target carrying strict warnings, linked by waya's OWN targets
# (tests + examples) only — never forced on library consumers. Turn warnings
# into errors in CI with -DWAYA_WERROR=ON.
option(WAYA_WERROR "Treat warnings as errors for waya's own targets" OFF)
option(WAYA_SANITIZE "Build waya's own targets with ASan+UBSan" OFF)
# ThreadSanitizer for the concurrent live runtime (mutually exclusive with
# ASan/UBSan). Catches data races in the per-connection threads + broadcast Hub.
option(WAYA_TSAN "Build waya's own targets with ThreadSanitizer" OFF)
# Strict surface validation: refuse to render a structurally-invalid tree
# (aborts with a spec-quoting message on the first bad view). Makes "impossible
# UIs can't ship" literal. On for tests; opt-in for apps via -DWAYA_STRICT=ON.
option(WAYA_STRICT "Abort on structural surface violations at render time" OFF)
# Pure vocabulary only: make the raw css()/var() escape hatches COMPILE ERRORS,
# so a team can guarantee its UI is built entirely from named mods — never a
# hand-written CSS property or HTML string. If it compiles, no web leaked in.
option(WAYA_NO_RAW_CSS "Disable the raw css()/var() escape hatches (compile error on use)" OFF)

add_library(waya_dev_flags INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(waya_dev_flags INTERFACE
        -Wall -Wextra
        # Designated-initializer app code (e.g. Meta{.title=...}) legitimately
        # leaves later members defaulted; that's not a bug in this codebase.
        -Wno-missing-field-initializers
        $<$<BOOL:${WAYA_WERROR}>:-Werror>)
    if(WAYA_SANITIZE)
        target_compile_options(waya_dev_flags INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(waya_dev_flags INTERFACE
            -fsanitize=address,undefined)
    endif()
    if(WAYA_TSAN)
        target_compile_options(waya_dev_flags INTERFACE
            -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(waya_dev_flags INTERFACE
            -fsanitize=thread)
    endif()
endif()
if(WAYA_STRICT)
    target_compile_definitions(waya_dev_flags INTERFACE WAYA_STRICT)
endif()
if(WAYA_NO_RAW_CSS)
    target_compile_definitions(waya_dev_flags INTERFACE WAYA_NO_RAW_CSS)
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
option(WAYA_BUILD_TESTS "Build waya's test suite" ON)
if(WAYA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Examples ────────────────────────────────────────────────────────────────
option(WAYA_BUILD_EXAMPLES "Build waya's examples" ON)
if(WAYA_BUILD_EXAMPLES)
    # Every example is a waya app: a surface Program run with waya::surface::live.
    # Targets are derived from examples/*.cpp automatically, so dropping a new
    # file in examples/ creates a new target with no edit here. CONFIGURE_DEPENDS
    # re-globs on build, so added/removed files are picked up without re-running
    # cmake by hand.
    file(GLOB WAYA_EXAMPLE_SRCS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    set(WAYA_EXAMPLE_TARGETS "")
    foreach(ex_src ${WAYA_EXAMPLE_SRCS})
        get_filename_component(ex ${ex_src} NAME_WE)
        add_executable(${ex} ${ex_src})
        target_link_libraries(${ex} PRIVATE waya::waya waya_dev_flags)
        list(APPEND WAYA_EXAMPLE_TARGETS ${ex})
    endforeach()

    # Multi-file example: examples/matrix/ — a Matrix-style hacker terminal.
    # One component per header (rain / terminal / hud / boot), composed in
    # app.cpp. Shows a heavily component-based waya app.
    file(GLOB WAYA_MATRIX_SRCS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/examples/matrix/*.cpp")
    if(WAYA_MATRIX_SRCS)
        add_executable(matrix ${WAYA_MATRIX_SRCS})
        target_link_libraries(matrix PRIVATE waya::waya waya_dev_flags)
        list(APPEND WAYA_EXAMPLE_TARGETS matrix)
    endif()

    # Multi-file example: examples/airwave/ — an online radio player. Real live
    # <audio> streams, a station browser, now-playing bar, animated visualizer.
    # One component per header; app.cpp composes them.
    file(GLOB WAYA_AIRWAVE_SRCS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/examples/airwave/*.cpp")
    if(WAYA_AIRWAVE_SRCS)
        add_executable(airwave ${WAYA_AIRWAVE_SRCS})
        target_link_libraries(airwave PRIVATE waya::waya waya_dev_flags)
        list(APPEND WAYA_EXAMPLE_TARGETS airwave)
    endif()

    # Multi-file example: examples/autorace/ — a fully-playable homage to Mattel
    # Auto Race (1976), the first handheld electronic game. A red-LED grid
    # racer: steer between lanes, shift gears, dodge oncoming cars. Real
    # keyboard controls via global hotkeys + a server game-loop tick.
    file(GLOB WAYA_AUTORACE_SRCS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/examples/autorace/*.cpp")
    if(WAYA_AUTORACE_SRCS)
        add_executable(autorace ${WAYA_AUTORACE_SRCS})
        target_link_libraries(autorace PRIVATE waya::waya waya_dev_flags)
        list(APPEND WAYA_EXAMPLE_TARGETS autorace)
    endif()

    # Dev-server smoke test: start an app, confirm it serves the shell + client.
    # Uses the first discovered example as the subject.
    if(WAYA_BUILD_TESTS AND UNIX AND WAYA_EXAMPLE_TARGETS)
        find_program(WAYA_CURL curl)
        if(WAYA_CURL)
            list(GET WAYA_EXAMPLE_TARGETS 0 WAYA_SMOKE_TARGET)
            add_test(
                NAME serve_smoke
                COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/serve_smoke.sh
                        $<TARGET_FILE:${WAYA_SMOKE_TARGET}> 8137)
        endif()
    endif()
endif()

# ── Benchmarks ────────────────────────────────────────────────────────
option(WAYA_BUILD_BENCH "Build waya's benchmarks" OFF)
if(WAYA_BUILD_BENCH)
    add_executable(bench_frames bench/frames.cpp)
    target_link_libraries(bench_frames PRIVATE waya::waya)
    target_compile_options(bench_frames PRIVATE $<$<CONFIG:>:-O2>)
endif()

# ── Install ─────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS waya waya_dsl waya_runtime EXPORT wayaTargets)
install(EXPORT wayaTargets
    NAMESPACE waya::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/waya)
