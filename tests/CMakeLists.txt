# ── Runtime tests ────────────────────────────────────────────────────────────
add_executable(test_render        test_render.cpp)
add_executable(test_content_model test_content_model.cpp)
add_executable(test_dynamic       test_dynamic.cpp)
add_executable(test_style_general test_style_general.cpp)

foreach(t test_render test_content_model test_dynamic test_style_general)
    target_link_libraries(${t} PRIVATE waya::waya)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

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
