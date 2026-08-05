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

# waya authors its own diagnostics via P2741; the compiler's own instantiation
# backtrace is noise. Consumers get short, readable errors. (See DESIGN §10.1.)
target_compile_options(waya INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-ftemplate-backtrace-limit=1>)

# ── Tests ───────────────────────────────────────────────────────────────────
option(WAYA_BUILD_TESTS "Build waya's test suite" ON)
if(WAYA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Examples ────────────────────────────────────────────────────────────────
option(WAYA_BUILD_EXAMPLES "Build waya's examples" ON)
if(WAYA_BUILD_EXAMPLES)
    add_executable(hello examples/hello.cpp)
    target_link_libraries(hello PRIVATE waya::waya)
endif()

# ── Install ─────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS waya EXPORT wayaTargets)
install(EXPORT wayaTargets
    NAMESPACE waya::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/waya)
