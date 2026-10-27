# W2UCEM 依赖清单：全部 FetchContent 锁定 tag，禁止使用未锁定版本。
# 网络只在宿主（设计决策 2），这里的库全部是构建期依赖。

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# 依赖库自身的测试/示例不构建（省时；W2UCEM 的测试在 tests/ 下）
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(TL_EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# --- toml11：模板格式解析（header-only，严格 schema 在解析器层实现） ---
FetchContent_Declare(
  toml11
  GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
  GIT_TAG        v4.4.0
  GIT_SHALLOW    TRUE
)

# --- nlohmann/json：仓库索引 / 账本 / dep-manifest 序列化 ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

# --- fmt：日志与格式化 ---
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.1.0
  GIT_SHALLOW    TRUE
)

# --- tl::expected：Result<T> 错误传播 ---
FetchContent_Declare(
  tl_expected
  GIT_REPOSITORY https://github.com/TartanLlama/expected.git
  GIT_TAG        v1.1.0
  GIT_SHALLOW    TRUE
)

# --- CLI11：w2ucem-cli 参数解析 ---
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2
  GIT_SHALLOW    TRUE
)

# --- FTXUI：w2ucem-tui（仅 W2UCEM_BUILD_TUI=ON 时可用） ---
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v6.1.8
  GIT_SHALLOW    TRUE
)

# --- Catch2：单测（仅 W2UCEM_BUILD_TESTS=ON 时可用） ---
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
)

# --- 系统依赖：libcurl（下载）、OpenSSL（sha256/ed25519 验签）、zlib ---
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
