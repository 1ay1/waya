# ── Runtime tests ────────────────────────────────────────────────────────────
add_executable(test_render        test_render.cpp)
add_executable(test_content_model test_content_model.cpp)
add_executable(test_dynamic       test_dynamic.cpp)
add_executable(test_style_general test_style_general.cpp)
add_executable(test_layout        test_layout.cpp)
add_executable(test_attrs         test_attrs.cpp)
add_executable(test_program       test_program.cpp)
add_executable(test_diff          test_diff.cpp)
add_executable(test_diff_perf     test_diff_perf.cpp)
add_executable(test_memo          test_memo.cpp)
add_executable(test_ws            test_ws.cpp)
add_executable(test_live_sound    test_live_sound.cpp)
add_executable(test_surface       test_surface.cpp)

foreach(t test_render test_content_model test_dynamic test_style_general test_layout test_attrs test_program test_diff test_diff_perf test_memo test_ws test_live_sound test_surface)
    target_link_libraries(${t} PRIVATE waya::waya)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# ── Browser-client applier test (jsdom) ──────────────────────────────────────
# Verifies the JS patch applier against a real DOM so the "path indexes text
# nodes but the client used element-only children" bug can't come back. Skips
# cleanly when node/jsdom aren't available (the .js exits 0 with SKIP).
find_program(WAYA_NODE node)
if(WAYA_NODE)
    add_test(NAME client_apply
        COMMAND ${WAYA_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/client_apply.test.js)
endif()

# ── Compile-fail tests: each case MUST NOT compile ───────────────────────────
# We compile tests/fail/cases.cpp once per case; ctest treats a failed compile
# as the expected outcome via WILL_FAIL. This is the golden-error surface.
set(WAYA_FAIL_CASES 17)

foreach(n RANGE 1 ${WAYA_FAIL_CASES})
    add_test(
        NAME fail_case_${n}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                --target waya_fail_${n})
    # Build the target only on demand (EXCLUDE_FROM_ALL), and expect failure.
    add_library(waya_fail_${n} OBJECT EXCLUDE_FROM_ALL fail/cases.cpp)
    target_link_libraries(waya_fail_${n} PRIVATE waya::waya)
    target_compile_definitions(waya_fail_${n} PRIVATE WAYA_CASE=${n})
    set_tests_properties(fail_case_${n} PROPERTIES WILL_FAIL TRUE)
endforeach()
