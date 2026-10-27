cmake_minimum_required(VERSION 3.20)
project(w2ucem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(W2UCEM_BUILD_TUI    "构建 FTXUI 前端 w2ucem-tui"        ON)
option(W2UCEM_BUILD_CAPI   "构建 C ABI 共享库 w2ucem_capi"     ON)
option(W2UCEM_BUILD_TESTS  "构建单元测试"                       ON)
option(W2UCEM_ENABLE_SANITIZERS "启用 ASan/UBSan（CI 用）"     OFF)

include(cmake/dependencies.cmake)
include(cmake/warnings.cmake)

add_subdirectory(src/core)
add_subdirectory(src/cli)

if(W2UCEM_BUILD_TUI)
  add_subdirectory(src/tui)
endif()

if(W2UCEM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()
