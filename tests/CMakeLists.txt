# ── Runtime tests ────────────────────────────────────────────────────────────
# The surface stack (surface/ + net/ + core/) is the whole framework; every
# test below drives that one public API.
add_executable(test_layout   test_layout.cpp)
add_executable(test_ws       test_ws.cpp)
add_executable(test_surface  test_surface.cpp)
add_executable(test_binary   test_binary.cpp)
add_executable(test_effect   test_effect.cpp)
add_executable(test_scale    test_scale.cpp)
add_executable(test_seo      test_seo.cpp)
add_executable(test_input    test_input.cpp)
add_executable(test_assets   test_assets.cpp)
add_executable(test_ui       test_ui.cpp)
add_executable(test_safety   test_safety.cpp)
add_executable(test_widgets  test_widgets.cpp)
add_executable(test_net      test_net.cpp)
add_executable(test_typed    test_typed.cpp)
add_executable(test_mod      test_mod.cpp)

foreach(t test_layout test_ws test_surface test_binary test_effect test_input test_scale test_seo test_assets test_ui test_safety test_widgets test_net test_typed test_mod)
    target_link_libraries(${t} PRIVATE waya::waya waya_dev_flags)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# ── Type-state gate suite: each case MUST NOT compile ─────────────────────
# The signature "impossible layout doesn't compile" guarantee, proven: we build
# tests/fail/typed.cpp once per case with -DWAYA_CASE=N; ctest expects the build
# to FAIL (WILL_FAIL). If a bad case ever compiles, the gate has regressed.
set(WAYA_TYPED_FAIL_CASES 5)
foreach(n RANGE 1 ${WAYA_TYPED_FAIL_CASES})
    add_test(NAME typed_fail_${n}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target waya_typed_fail_${n})
    add_library(waya_typed_fail_${n} OBJECT EXCLUDE_FROM_ALL fail/typed.cpp)
    target_link_libraries(waya_typed_fail_${n} PRIVATE waya::waya)
    target_compile_definitions(waya_typed_fail_${n} PRIVATE WAYA_CASE=${n})
    set_tests_properties(typed_fail_${n} PROPERTIES WILL_FAIL TRUE)
endforeach()

# ── Browser-client applier test (jsdom) ──────────────────────────────────────
# Verifies the JS surface-patch applier against a real DOM so the "path indexes
# text nodes but the client used element-only children" bug can't come back.
# Skips cleanly when node/jsdom aren't available (the .js exits 0 with SKIP).
find_program(WAYA_NODE node)
if(WAYA_NODE)
    add_test(NAME surface_apply
        COMMAND ${WAYA_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/surface_apply.test.js)
endif()
