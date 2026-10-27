// 用户模板服务端同步接口：心跳上报 / 审核拉取。
// alpha 不实现服务端——StubSyncClient 返回 Unsupported，
// 协议字段级定义见 docs/REPO-PROTOCOL.md。
#pragma once

#include <string>
#include <vector>

#include "w2ucem/usertemplate/meta.hpp"
#include "w2ucem/util/result.hpp"

namespace w2ucem {

class UserTemplateSyncClient {
 public:
  virtual ~UserTemplateSyncClient() = default;

  // 心跳上报：服务端据此判定活跃度并做多方身份验证。
  [[nodiscard]] virtual Result<void> push_heartbeat(const std::string& id,
                                                    const std::string& now_iso) = 0;

  // 拉取待审核列表（官方侧）。
  [[nodiscard]] virtual Result<std::vector<UserTemplateMeta>> fetch_pending_review() = 0;
};

class StubSyncClient final : public UserTemplateSyncClient {
 public:
  [[nodiscard]] Result<void> push_heartbeat(const std::string& id,
                                            const std::string& now_iso) override;
  [[nodiscard]] Result<std::vector<UserTemplateMeta>> fetch_pending_review() override;
};

}  // namespace w2ucem
