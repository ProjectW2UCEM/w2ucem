# W2UCEM 架构

W2UCEM（Windows to Unix Compatibility Environment Manager）以 AUR/pacman-makepkg 为原型：
**不发布兼容层，只发布构建模板**，由用户机器求值 + 容器化构建出带完美依赖的兼容层。

## 核心原则

1. **模板 = 纯数据（TOML），函数体锁定在 `TemplateEvaluator`**：同 `(模板原始字节, BuildTarget, evaluator_version)` 必然产出同一 `BuildPlan`——可复现性是构造性保证，不是约定。模板格式见 [SPEC.md](SPEC.md)。
2. **网络只在宿主，容器零网络**：源/补丁/插件由宿主下载并 sha256 校验，容器只挂载已验证字节（只读）。
3. **可复现性账本（ledger）= "源头→用户"证据链**：记录模板/源/镜像/插件/runner 全部输入哈希与产物哈希，任何 verify 都能回答"每个输入字节从哪来、hash 是什么"。
4. **前端无关**：核心是纯库（无 UI 依赖）；CLI/TUI/CAPI 只面向 `api::Facade`；日志走 `LogSink` 抽象。
5. **安全分工**：隔离由系统沙箱/容器负责，W2UCEM 只负责可复现性。

## 模块划分（依赖无环）

```
util            Result/Sha256/日志/网络/进程/文件（一切的基础）
channel         Channel/GpuVariant/DistroFamily/BuildTarget（纯数据）
config          Config + DataDirs（XDG 布局，--root 重定向）
template        模板引擎：Parser（严格 schema）/Validator（红线）/Evaluator（纯函数）/BuildPlan
plugin          插件：manifest 解析 + hook 调度（payload 哈希入账本）
verify          校验链：产物 manifest（pin/record 双模式）+ dep-manifest schema
repo            中央仓库：签名索引（ed25519）+ 模板原始字节校验
archive         时光机：内容寻址 blob + FallbackSourceResolver（镜像轮询 → Archive 回退）
build           构建编排：容器后端（docker/podman/fake）+ digest 锁 + 矩阵 + runner + ledger + orchestrator
usertemplate    用户模板体系：元数据 + 状态机（心跳/半死/封存）+ 本地注册表
api/facade      单一入口（CLI/TUI/CAPI 唯一依赖面）
capi            C ABI（SwiftUI 等前端经 FFI 消费；alpha 仅接口存在性）
```

## 关键数据流

```
w2ucem sync     → 拉取 repo.db + repo.db.sig → ed25519 验签 → 原子写本地缓存
w2ucem build    → resolve_template（原始字节 sha256 校验）
                → TemplateEvaluator::evaluate → BuildPlan
                → FallbackSourceResolver（urls 轮询校验 → 全败走 Archive → 回写缓存）
                → ImageDigestLock（pull-by-digest → inspect 比对）
                → RunnerScript（确定性生成，sha256 入账本）
                → 容器装配（buildenv，零网络，只读挂载源）
                → dep-scan（矩阵容器：readelf DT_NEEDED → 每发行版依赖清单）
                → Verifier（manifest pin/record + dep-manifest schema）
                → Ledger 落账本（build_id = sha256(全部输入)）
```

## 关键机制

- **digest 锁**：模板声明 `buildenv.image + digest`，pull 后 inspect 实际 digest 比对，mismatch = 硬失败；矩阵镜像锁在编译进二进制的 `ci/matrix.toml`（SDK 层随版本锁定）。
- **manifest 双模式**：pin（声明 sha256）与 record（账本首建记录/复建比对）；stable/major 通道禁 record。
- **符号表校验**：`symtab` 作用域选择器（dyn/full）→ 容器内只跑一条原始 `readelf -Ws`（原始输出落 `.w2ucem/symtab-raw/`）→ 宿主规范化器（与 verify 共用同一实现：丢弃地址列、Ndx 归一化、排序去重）→ 快照 `.w2ucem/symtab/` → sha256 入账本；verify 用账本记录的 `buildenv_ref` 起容器重新提取比对。macOS 实验路径（fake 后端/无 readelf）跳过提取并在 verify 显式报告。
- **断点续传**：`net::get_to_file` 状态机——目标非空时发 Range；206 续传完成；`CURLE_RANGE_ERROR`（libcurl 保证未写入）或 416 回退全量；4xx 删除；网络中断保留 partial。最终由 FallbackSourceResolver 全量 sha256 校验兜底（续传无法绕过内容寻址）。
- **矩阵失败语义**：目标族 dep-scan 镜像锁定失败 = 硬失败；辅助族 = 跳过并记入账本 `dep_scan_skipped`（"跳过即记录"）。
- **变体合并**：`[variant.<gpu>]` 在 TOML 层深合并（标量/数组替换、表递归、未提及继承）——"按发行版/GPU 特调"是同一模板的不同求值。
- **dep-scan**：装配产物字节跨发行版相同，差异收敛到矩阵容器内的 ELF 依赖扫描 → 每发行版专属依赖清单（"带完美依赖"的机制载体）。
- **用户模板状态机**：Active --心跳超 7 天--> HalfDead --恢复--> Active；HalfDead --超 30 天--> Archived（"人死删包"=隐藏封存，文件不删除）。服务端协议见 [REPO-PROTOCOL.md](REPO-PROTOCOL.md)。
- **插件**：模板内声明式 hook（alpha 实现 pre-assemble/post-assemble），payload 注入容器 runner 并哈希入账本，不破坏可复现性。

## 构建与测试

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build                      # 单元测试
tests/cli/smoke.sh build/src/cli/w2ucem-cli # CLI 冒烟（fixtures 仓库）
tests/e2e/e2e.sh build/src/cli/w2ucem-cli   # 容器 e2e（Linux + docker）
```

## alpha 后置（明确不做，非缺陷）

服务端全套（仓库/Archive/审核/心跳 API）、编译型模板（kind=compile）、真实 GPU 特调运行、deb/rpm 打包、QT/GTK/SwiftUI 前端、插件市场、WASM script 步骤。

## 已知限制

- 并发构建共享源缓存时对同一 `.download` 文件的 append 竞争（跨进程续传）：后续以 O_EXCL 锁文件解决。
- macOS 实验路径（fake 后端）跳过 dep-scan 与 symtab 提取：跨工具链（llvm-nm vs GNU readelf）比对假阳性大于无校验；HostSymtabExtractor 留有后门。
- `--distro-matrix` 下 fedora/archlinux 基础镜像为滚动 latest：e2e 运行期非完全密封，官方发布矩阵镜像后以 digest 锁定。
