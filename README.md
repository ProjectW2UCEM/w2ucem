# w2ucem

**Windows to Unix\* Compatible Environment Manager** —— 没错，几天开发出 alpha 并发布。

以 **AUR / pacman-makepkg 构建系统为原型**的 Windows→Unix 兼容层管理器：上游摄取 WineHQ / Proton / Soda / Caffe / Lutris 等 WtL 方案，在不可变容器镜像（Alpine 基板）内按发行版矩阵构建测试——但**不发布兼容层，只发布构建模板**（PKGBUILD 思想），由用户机器求值出带完美依赖的兼容层。

## 设计承诺

- **函数式模板**：模板 = 纯数据（TOML），求值 = 库内锁定的纯函数。同输入必然同产物——可复现性是构造性保证。规范见 [docs/SPEC.md](docs/SPEC.md)。
- **可复现性校验链**：源/补丁/模板 sha256 强制 + 仓库索引 ed25519 验签 + 构建镜像 digest 锁 + 产物 manifest（pin/record 双模式）+ **符号表快照比对**（readelf 规范化，跨构建导出面断言）+ 全量账本（"源头→用户"证据链）。
- **时光机仓库**：依赖内容寻址存档；官方源失效自动回退（`archive-check` 诊断）；大文件下载**断点续传**（Range 206，服务器不支持自动回退全量，最终全量哈希兜底）。
- **发行版矩阵**：deb/rpm/arch/alpine 四族 dep-scan（readelf 依赖清单），跨族输出确定性；辅助族失败跳过留痕（`dep_scan_skipped`），目标族硬失败。
- **三通道**（Debian 哲学）：semi-rolling / stable / major。
- **GPU 特调**：`[variant.<gpu>]` 深合并——同一模板的不同求值。
- **用户模板体系**（AUR 式）：审核后发布、实验性标记、心跳签到（7 天）、"人死删包"（隐藏封存，文件不删除）。协议见 [docs/REPO-PROTOCOL.md](docs/REPO-PROTOCOL.md)。
- **前端矩阵**：CLI（已发布）/ TUI（已发布）/ QT-GUI / GTK-GUI / SwiftUI（核心库 C ABI 就绪，alpha 后）。

## 快速开始（Linux 第一公民）

```bash
# 依赖：cmake ≥3.20、C++20 编译器、libcurl/OpenSSL/zlib 开发包
sudo apt install libcurl4-openssl-dev libssl-dev zlib1g-dev   # Debian 系

# 方式一：CMake Presets（推荐，CLion / VS Code 原生支持）
cmake --preset Debug                 # Debug / Release / Sanitize
cmake --build --preset Debug -j
ctest --preset Debug                 # 单元测试

# 方式二：手动 configure
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build               # 单元测试

# 使用（以测试仓库为例）
tests/fixtures/gen_fixture_repo.sh tests/fixtures             # 生成测试仓库
mkdir -p ~/.local/share/w2ucem/trust
cp tests/fixtures/keys/pubkey.pem ~/.local/share/w2ucem/trust/pubkey.pem
build/src/cli/w2ucem-cli sync --repo file://$PWD/tests/fixtures/repo
build/src/cli/w2ucem-cli search mock
build/src/cli/w2ucem-cli info mock-proton --target semi-rolling:generic:deb:stable
build/src/cli/w2ucem-cli fetch mock-proton
build/src/cli/w2ucem-cli build mock-proton --engine docker    # 或 podman；无引擎用 fake
build/src/cli/w2ucem-cli verify mock-proton
build/src/cli/w2ucem-cli ledger
```

macOS 上 CLI 可运行，容器构建路径（需 colima/Docker Desktop）为实验性；无引擎时 `--engine fake` 在宿主直跑（dep-scan 跳过）。

## Dev Container 开发（Ubuntu 26.04）

`.devcontainer/` 提供开箱即用的开发容器，基础镜像 **Ubuntu 26.04 LTS**，兼容三种宿主运行时：**Docker / Docker Rootless / Podman**。

```bash
# 任一运行时启动（以 devcontainer CLI 为例，自动探测 Docker / Podman）
devcontainer up --workspace-folder .
```

容器内预装：gcc/g++（C++20）、CMake ≥3.20、Ninja、libcurl/OpenSSL/zlib 开发包、git、binutils（readelf）、gdb、python3，以及 rootless podman（`podman-docker` 提供 `docker` 别名）。

关键兼容设计：

- **全程非 root 用户** `ubuntu`（UID 1000）——rootless 运行时下挂载卷文件属主正确的前提。
- **用 `build.dockerfile` 而非 docker-compose**——避免 Podman 的 compose 插件依赖。
- **不用需要特权或 Docker 特有特性的 features**——保证三种运行时行为一致。

进入容器后：

```bash
cmake -B build -G Ninja
cmake --build build -j
ctest --test-dir build                              # 单元测试
./tests/cli/smoke.sh build/src/cli/w2ucem-cli       # CLI 冒烟
```

> 容器内再起容器（`w2ucem-cli build --engine docker/podman`、`tests/e2e/e2e.sh`）依赖宿主的嵌套 user-ns 与 `/dev/fuse` 支持；不满足时用 `--engine fake`（宿主直跑），或在 `devcontainer.json` 中挂载宿主 docker/podman socket。全量容器 e2e 建议在 CI（`.github/workflows/ci.yml`）跑。
>
> 首次 `cmake` configure 会通过 FetchContent 从 GitHub 拉取锁定依赖（toml11 / nlohmann-json / fmt / tl-expected / CLI11 / FTXUI / Catch2），需要容器内可访问 GitHub。若宿主经代理访问外网，需在 `devcontainer.json` 的 `containerEnv` 中设置 `HTTP_PROXY`/`HTTPS_PROXY`（并注意容器内无法直连宿主的 `127.0.0.1`，改用 `host.docker.internal` 或 host 网络）。

## 命令面

`sync` `search` `info` `list` `fetch` `build` `verify` `ledger` `archive-check` `plugin list` `user-template check/heartbeat/list` `doctor` `version`

## 仓库结构

```
src/core    libw2ucem（纯库：模板引擎/仓库/Archive/构建编排/账本/校验/插件/用户模板）
src/cli     w2ucem-cli（CLI11）
src/tui     w2ucem-tui（FTXUI 四区）
templates/  官方模板（proton-ge 装配型、nvidia-smoke GPU 特调示例）+ 用户插件
docs/       SPEC.md（模板规范）/ ARCHITECTURE.md / REPO-PROTOCOL.md
tests/      单测 + CLI 冒烟 + 容器 e2e（fixtures 自生成）
ci/         发行版矩阵表 + 构建镜像 Dockerfile
```

## alpha 后置（明确路线，非缺陷）

服务端全套（仓库/Archive/审核/心跳 API）、编译型模板（`kind=compile`）、符号表快照比对、矩阵全量、断点续传、真实 GPU 特调运行、QT/GTK/SwiftUI 前端、插件市场。
