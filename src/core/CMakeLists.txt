# w2ucem_core：纯库，无 UI 依赖（CI 检查依赖图，禁止 FTXUI 出现）。
# 模块实现与 include/w2ucem/<module>/ 一一对应，依赖方向见 docs/ARCHITECTURE.md。

FetchContent_MakeAvailable(toml11 nlohmann_json fmt tl_expected)

add_library(w2ucem_core STATIC
  src/version.cpp
  src/util/error.cpp
  src/util/sha256.cpp
  src/util/logging.cpp
  src/util/fs.cpp
  src/util/net.cpp
  src/util/process.cpp
  src/channel/target.cpp
  src/config/data_dirs.cpp
  src/config/config.cpp
  src/template/parser.cpp
  src/template/validator.cpp
  src/template/evaluator.cpp
  src/template/support.cpp
  src/repo/repo_index.cpp
  src/repo/repo_client.cpp
  src/archive/archive_client.cpp
  src/archive/fallback_resolver.cpp
  src/build/container_backends.cpp
  src/build/engine_probe.cpp
  src/build/image_lock.cpp
  src/build/matrix.cpp
  src/build/runner.cpp
  src/build/ledger.cpp
  src/build/orchestrator.cpp
  src/verify/verifier.cpp
  src/verify/symtab.cpp
  src/plugin/plugin.cpp
  src/plugin/engine.cpp
  src/usertemplate/meta.cpp
  src/usertemplate/state_machine.cpp
  src/usertemplate/registry.cpp
  src/api/facade.cpp
)
target_include_directories(w2ucem_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# 矩阵表（SDK 层锁定）：拷贝到构建目录并以宏指向
configure_file(${CMAKE_SOURCE_DIR}/ci/matrix.toml ${CMAKE_BINARY_DIR}/generated/matrix.toml COPYONLY)
target_compile_definitions(w2ucem_core
  PRIVATE W2UCEM_MATRIX_FILE="${CMAKE_BINARY_DIR}/generated/matrix.toml"
)
target_link_libraries(w2ucem_core
  PUBLIC
    toml11::toml11
    nlohmann_json::nlohmann_json
    fmt::fmt
    tl::expected
    CURL::libcurl
    OpenSSL::Crypto
    ZLIB::ZLIB
)
w2ucem_apply_warnings(w2ucem_core)
w2ucem_apply_sanitizers(w2ucem_core)

# --- C ABI 共享库：仅供 SwiftUI 等前端消费，alpha 仅验证存在性 ---
if(W2UCEM_BUILD_CAPI)
  add_library(w2ucem_capi SHARED
    src/capi.cpp
  )
  target_include_directories(w2ucem_capi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(w2ucem_capi PRIVATE w2ucem_core)
  set_target_properties(w2ucem_capi PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
  )
  w2ucem_apply_warnings(w2ucem_capi)
endif()
