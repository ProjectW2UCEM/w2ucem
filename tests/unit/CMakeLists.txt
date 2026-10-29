# w2ucem_unit_tests：core 单测。
FetchContent_MakeAvailable(catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

set(W2UCEM_UNIT_TEST_SOURCES
  test_version.cpp
  test_util.cpp
  test_target.cpp
  test_data_dirs.cpp
  test_template.cpp
  test_usertemplate.cpp
  test_net.cpp
  test_symtab.cpp
  test_matrix.cpp
)
# CAPI 关闭时跳过 capi 测试（test_capi 依赖 w2ucem_capi 库）
if(W2UCEM_BUILD_CAPI)
  list(APPEND W2UCEM_UNIT_TEST_SOURCES test_capi.cpp)
endif()

add_executable(w2ucem_unit_tests ${W2UCEM_UNIT_TEST_SOURCES})
# 单测需要定位 mock 服务器脚本与编译默认矩阵表
target_compile_definitions(w2ucem_unit_tests
  PRIVATE TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
         W2UCEM_MATRIX_FILE="${CMAKE_BINARY_DIR}/generated/matrix.toml"
)
target_link_libraries(w2ucem_unit_tests PRIVATE w2ucem_core Catch2::Catch2WithMain)
if(W2UCEM_BUILD_CAPI)
  target_link_libraries(w2ucem_unit_tests PRIVATE w2ucem_capi)
endif()
w2ucem_apply_warnings(w2ucem_unit_tests)
w2ucem_apply_sanitizers(w2ucem_unit_tests)

include(Catch)
catch_discover_tests(w2ucem_unit_tests)
