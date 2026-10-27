# W2UCEM 模板格式规范 v0.1

本文档是模板格式（`format = "0.1"`）的权威定义。解析器按本文档实现；实现与本文档不一致时以本文档为准（并视为 bug）。

## 1. 设计哲学：函数式模板

- **模板是纯数据**：模板文件（TOML）不包含任何可执行代码，只声明来源、依赖、装配步骤与校验基准。
- **函数体在求值器**：`TemplateEvaluator::evaluate(template, BuildTarget) -> BuildPlan` 是随 libw2ucem 版本锁定的纯函数。同 `(模板原始字节, BuildTarget, evaluator_version)` 必然产出同一 `BuildPlan`，跨机器可证明。
- **不发布兼容层，只发布模板**：用户机器上求值 + 构建出产物；"按发行版/GPU 特调"是同一模板的不同求值。

## 2. 版本

- `[template].format` 声明模板遵循的规范版本，解析器硬校验，当前仅接受 `"0.1"`。
- 未来扩展（编译型 build 步骤、WASM script 步骤）通过递增 format 版本引入，不破坏 v0.1。

## 3. 顶层结构

| 键 | 必填 | 说明 |
|---|---|---|
| `template` | 是 | 标识与元信息 |
| `meta` | 是 | 取值域声明（通道/GPU/发行版族） |
| `buildenv` | 是 | 不可变构建镜像 + digest 锁 |
| `sources` | 否 | 命名的源表 `[sources.<id>]` |
| `patches` | 否 | 补丁数组 `[[patches]]` |
| `build_depends` | 否 | family → 包名映射 |
| `assemble` | 否 | 装配步骤 `[[assemble.steps]]` |
| `variant` | 否 | GPU 变体覆盖 `[variant.<gpu>]` |
| `hooks` | 否 | 插件 hook 声明 |
| `manifest` | 否 | 产物校验基准 |

**未知键 = 硬错误（SchemaViolation）**：拼错键静默忽略在可复现系统里就是构建漂移。

## 4. 各节字段

### 4.1 `[template]`

`format`、`id`、`version`、`kind` 必填；`name`（缺省 = id）、`upstream` 可选。
`kind` 当前仅接受 `"assembly"`；`"compile"`/`"hybrid"` 为 alpha 后功能（Unsupported）。

### 4.2 `[meta]`

`channels`、`gpu_variants`、`distro_families` 必填非空，是求值的**取值域**——域外参数求值 → `EvaluationError`。
取值：`channels ∈ {semi-rolling, stable, major}`；`gpu_variants ∈ {generic, nvidia, amd, intel}`；`distro_families ∈ {arch, deb, rpm, alpine}`。
`maintainer`、`license` 可选。

### 4.3 `[buildenv]`

- `image`：不可变构建镜像名（必填）。
- `digest`：镜像 digest 锁，必填，形式 `sha256:<64hex>`。构建时 pull-by-digest 并比对实际 digest，mismatch = 硬失败。

### 4.4 `[sources.<id>]`

- `urls`：有序镜像列表（必填非空）。下载时逐个尝试，每个都做 sha256 校验。
- `sha256`：必填 64 位 hex——alpha 无哈希即拒绝。
- `archive_id`：可选，形式 `sha256:<hex>`。全部 urls 失败时回退时光机仓库（内容寻址）下载。

### 4.5 `[[patches]]`

`path`（相对模板目录）与 `sha256` 均必填。

### 4.6 `[build_depends]`

键：`default` + 四族 `arch` / `deb` / `rpm` / `alpine`，值为包名数组。
查询规则：目标 family 有显式条目取之，否则回退 `default`。

### 4.7 `[[assemble.steps]]`

有序数组，按序执行。每步 `type` 必填：

| type | 字段 | 说明 |
|---|---|---|
| `extract` | `source`, `to` | 解包源到 `$build/...` 路径 |
| `patch` | `patch`, `dir` | 应用补丁（sha256 已校验） |
| `dll-override` | `rules`, `output` | DLL 复写：确定性生成 override 清单 |
| `copy` | `from`, `to` | 拷贝（路径支持 `$build`/`$prefix` 变量） |
| `dep-scan` | `elfs`, `output` | 在发行版矩阵容器内解析 ELF 依赖 → 每发行版专属依赖清单 |
| `manifest` | `required` | 汇总断言：列出的路径必须存在 |

### 4.8 `[variant.<gpu>]`

GPU 特调覆盖，键必须已在 `gpu_variants` 声明（否则 SchemaViolation）。
覆盖内容为任意顶层键的**部分视图**，求值时在 TOML 层深合并后重新解析校验：

- 标量 / 数组：整体替换（**数组不拼接**）
- 表：递归合并
- 未提及键：继承 base

### 4.9 `[hooks]`

键 ∈ `{pre-fetch, post-fetch, pre-assemble, post-assemble, pre-build, post-build, post-install}`，值形如 `{ plugins = ["<plugin-id>"] }`。插件自身受哈希校验并入账本。

### 4.10 `[manifest]`

`[[manifest.files]]` 每项：`path`（必填，相对产物根）、`sha256`、`symtab`（预留）。

**双模式**：
- **pin**：`sha256` 声明具体值，构建后必须完全一致；
- **record**：`sha256` 为空，首次构建将实际哈希记入账本，复建时与账本比对。

**通道策略红线：声明 `stable` 或 `major` 通道的模板禁止 record 模式**（validator 拒绝）。

## 5. 确定性保证

- 求值器与 runner 脚本生成器不读环境变量、时间、宿主文件系统；一切变量来自模板、`BuildTarget` 或编译进二进制的矩阵表。
- 下载在宿主完成并 sha256 校验；容器只挂载已验证字节（只读），容器零网络。
- 账本记录全部输入哈希（模板/源/补丁/镜像/插件/runner），任何 verify 都能回答"每个输入字节从哪来、hash 是什么"。

## 6. 完整示例

见 `templates/official/proton-ge/template.toml`。
