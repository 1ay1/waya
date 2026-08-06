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

foreach(t test_layout test_ws test_surface test_binary test_effect test_input test_scale test_seo test_assets test_ui test_safety test_widgets)
    target_link_libraries(${t} PRIVATE waya::waya waya_dev_flags)
    add_test(NAME ${t} COMMAND ${t})
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
