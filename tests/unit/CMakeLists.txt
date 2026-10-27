# w2ucem_unit_tests：core 单测。
FetchContent_MakeAvailable(catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(w2ucem_unit_tests
  test_version.cpp
  test_util.cpp
  test_target.cpp
  test_data_dirs.cpp
  test_template.cpp
  test_usertemplate.cpp
  test_capi.cpp
)
target_link_libraries(w2ucem_unit_tests PRIVATE w2ucem_core Catch2::Catch2WithMain)
if(W2UCEM_BUILD_CAPI)
  target_link_libraries(w2ucem_unit_tests PRIVATE w2ucem_capi)
endif()
w2ucem_apply_warnings(w2ucem_unit_tests)
w2ucem_apply_sanitizers(w2ucem_unit_tests)

include(Catch)
catch_discover_tests(w2ucem_unit_tests)
