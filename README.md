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

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build                                        # 单元测试

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
