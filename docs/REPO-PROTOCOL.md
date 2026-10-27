# W2UCEM 仓库协议 v0.1（alpha 只定义不实现服务端）

本文档定义中央仓库、时光机（Archive）、用户模板服务端的协议。
alpha 客户端已实现全部客户端侧逻辑（验签、哈希校验、回退）；
**服务端为 alpha 后工作**，本协议保证届时无歧义对接。

## 1. 中央仓库（repo）

- 索引：`<repo-url>/repo.db`（JSON，`RepoIndex` 结构）+ `repo.db.sig`（ed25519 原始签名，64 字节，PEM 公钥分发）。
- 索引结构：
  ```json
  {
    "schema_version": 1,
    "generated_at": "ISO8601",
    "templates": [
      {"id": "proton-ge", "version": "9.27", "sha256": "<模板原始字节 sha256>",
       "path": "templates/proton-ge/9.27/template.toml",
       "review_status": "reviewed", "channels": ["semi-rolling", "stable", "major"]}
    ]
  }
  ```
- 客户端 sync：拉取 → 验签 → 先解析后落盘（坏索引永不进本地缓存）→ 原子替换。
- 模板字节：`<repo-url>/<path>`，sha256 必须与索引一致。

## 2. 时光机（Archive）

- 内容寻址：`<archive-url>/blobs/<sha256hex>`。
- 客户端回退语义：官方 urls 逐个试（下载 → sha256 校验）→ 全败 → archive_id（`sha256:<hex>`）走 Archive → 成功回写本地缓存。
- ingest 接口（维护者侧，客户端已有 `ArchiveClient::add`）：上传 blob 后按内容寻址存储。

## 3. 用户模板服务端

### 3.1 心跳（每周签到）

```
POST /api/v0/heartbeat
{
  "template_id": "…",
  "maintainer": {"name": "…", "email": "…"},
  "timestamp": "ISO8601",
  "evidence": [
    {"type": "browser", "detail": "<session token 摘要>", "verified_at": "…"},
    {"type": "cookie", "detail": "<cookie 摘要>", "verified_at": "…"},
    {"type": "openpgp", "detail": "<签名>", "verified_at": "…"},
    {"type": "ssh", "detail": "<公钥指纹>", "verified_at": "…"},
    {"type": "ip", "detail": "<来源 IP 网段>", "verified_at": "…"}
  ]
}
```
多方验证原则（规格）：浏览器/Cookie/OpenPGP/SSH/IP 多方交叉验证身份连续性，防止冒领；牺牲一点效率换安全。

### 3.2 审核流程

- 提交：`POST /api/v0/user-templates`（模板 + 元数据）→ 状态 `experimental`（AUR 教训：默认不信任，官方介入测试调查）。
- 官方审核：`experimental → reviewed`；调查发现问题可回退 `reviewed → experimental`。
- 客户端 `list` 默认隐藏 experimental 条目（显式 `--include-experimental` 可见）。

### 3.3 状态机（服务端与客户端同构）

- `active --心跳超 7 天--> half-dead --心跳恢复--> active`
- `half-dead --超 30 天无人认领--> archived`（"人死删包"：**隐藏 + 封存，文件不删除**——registry 与搜索不可见，存档保留）
- `archived --申诉（人工）--> active`

### 3.4 客户端 StubSyncClient

alpha 客户端 `StubSyncClient` 对上述端点返回 `Unsupported`；本地状态机、心跳时间戳、封存隐藏语义已完整实现。
