cmake_minimum_required(VERSION 3.28)

project(waya
    VERSION 0.1.0
    DESCRIPTION "A C++26 server-side web framework with a type-state HTML DSL"
    LANGUAGES CXX)

# ── The library (header-only for now) ───────────────────────────────────────
add_library(waya INTERFACE)
add_library(waya::waya ALIAS waya)

target_include_directories(waya INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(waya INTERFACE cxx_std_26)

# The live WebSocket runtime (app/live_ws.hpp) uses std::thread — one thread per
# connection so a single open socket can't block the server. Link Threads for
# any consumer that pulls it in.
find_package(Threads REQUIRED)
target_link_libraries(waya INTERFACE Threads::Threads)

# waya authors its own diagnostics via P2741; the compiler's own instantiation
# backtrace is noise. Consumers get short, readable errors. (See DESIGN §10.1.)
target_compile_options(waya INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-ftemplate-backtrace-limit=1>)

# ── Developer warning / sanitizer options ───────────────────────────────────
# An INTERFACE target carrying strict warnings, linked by waya's OWN targets
# (tests + examples) only — never forced on library consumers. Turn warnings
# into errors in CI with -DWAYA_WERROR=ON.
option(WAYA_WERROR "Treat warnings as errors for waya's own targets" OFF)
option(WAYA_SANITIZE "Build waya's own targets with ASan+UBSan" OFF)

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
    foreach(ex splash orbit studio pulse blog)
        add_executable(${ex} examples/${ex}.cpp)
        target_link_libraries(${ex} PRIVATE waya::waya waya_dev_flags)
    endforeach()

    # Dev-server smoke test: start an app, confirm it serves the shell + client.
    if(WAYA_BUILD_TESTS AND UNIX)
        find_program(WAYA_CURL curl)
        if(WAYA_CURL)
            add_test(
                NAME serve_smoke
                COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/serve_smoke.sh
                        $<TARGET_FILE:splash> 8137)
        endif()
    endif()
endif()

# ── Install ─────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS waya EXPORT wayaTargets)
install(EXPORT wayaTargets
    NAMESPACE waya::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/waya)
