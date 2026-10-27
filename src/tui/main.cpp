// w2ucem-tui：四区界面（顶栏 / 模板列表 / 详情 / 队列+日志）。
// TUI 只做渲染与事件转发，全部逻辑在 Facade（前端无关机制）。
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/ranges.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

#include "w2ucem/api/facade.hpp"
#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/target.hpp"
#include "w2ucem/util/logging.hpp"
#include "w2ucem/version.hpp"

using namespace ftxui;

namespace {

// TUI 日志 sink：环形缓冲（互斥锁保护），FTXUI 按帧消费。
class RingBufferLogSink final : public w2ucem::LogSink {
 public:
  explicit RingBufferLogSink(std::size_t capacity) : capacity_(capacity) {}

  void write(w2ucem::LogLevel level, const std::string& message) override {
    std::lock_guard lock(mutex_);
    const char* prefix = "";
    switch (level) {
      case w2ucem::LogLevel::Debug: prefix = "[d] "; break;
      case w2ucem::LogLevel::Info: prefix = "[i] "; break;
      case w2ucem::LogLevel::Warn: prefix = "[w] "; break;
      case w2ucem::LogLevel::Error: prefix = "[e] "; break;
    }
    lines_.push_back(prefix + message);
    while (lines_.size() > capacity_) {
      lines_.pop_front();
    }
  }

  std::vector<std::string> snapshot(std::size_t max_lines) const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out;
    const auto begin = lines_.size() > max_lines ? lines_.end() - static_cast<long>(max_lines)
                                                 : lines_.begin();
    for (auto it = begin; it != lines_.end(); ++it) {
      out.push_back(*it);
    }
    return out;
  }

 private:
  mutable std::mutex mutex_;
  std::size_t capacity_;
  std::deque<std::string> lines_;
};

constexpr std::array<const char*, 3> kChannels = {"semi-rolling", "stable", "major"};

}  // namespace

int main(int argc, char** argv) {
  std::string config_path;
  std::string root_override;
  // TUI 参数简化：--config / --root
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::string(argv[i]) == "--root" && i + 1 < argc) {
      root_override = argv[++i];
    }
  }

  auto facade = w2ucem::api::Facade::init(config_path, root_override);
  if (!facade) {
    fmt::print("初始化失败: {}\n", facade.error().message);
    return 1;
  }
  auto log_sink = std::make_shared<RingBufferLogSink>(200);
  w2ucem::Logger::instance().set_sink(log_sink);

  // ---------- 状态 ----------
  int channel_index = 0;
  int selected = 0;
  std::vector<std::string> template_ids;
  std::string detail_text = "(选择模板后显示详情)";
  std::string queue_text = "(无活动构建)";
  std::atomic<bool> building{false};
  std::thread build_thread;

  // ---------- 数据加载 ----------
  const auto refresh_templates = [&]() {
    auto ids = facade->list_available(true);
    template_ids = ids ? *ids : std::vector<std::string>{};
    if (selected >= static_cast<int>(template_ids.size())) {
      selected = template_ids.empty() ? 0 : static_cast<int>(template_ids.size()) - 1;
    }
  };
  const auto show_detail = [&]() {
    if (template_ids.empty()) {
      detail_text = "(无模板——先 `w2ucem sync`)";
      return;
    }
    const std::string ref = template_ids[static_cast<std::size_t>(selected)];
    auto tpl = facade->template_info(ref);
    if (!tpl) {
      detail_text = "详情失败: " + tpl.error().message;
      return;
    }
    const w2ucem::Channel channel = static_cast<w2ucem::Channel>(channel_index);
    w2ucem::BuildTarget target{channel, w2ucem::GpuVariant::Generic,
                               w2ucem::Distro{w2ucem::DistroFamily::Debian, "stable"}, "x86_64"};
    auto plan = facade->preview(ref, target);
    std::string text = tpl->id + " " + tpl->version + "\n";
    text += "上游: " + tpl->upstream + "\n";
    text += "通道: " + fmt::format("{}", fmt::join(tpl->meta.channels, ", ")) + "\n";
    text += "GPU 变体: " + fmt::format("{}", fmt::join(tpl->meta.gpu_variants, ", ")) + "\n";
    text += "发行版族: " + fmt::format("{}", fmt::join(tpl->meta.distro_families, ", ")) + "\n";
    text += "构建镜像: " + tpl->buildenv.image + " @ " + tpl->buildenv.digest.substr(0, 19) +
            "…\n";
    if (plan) {
      text += "\n求值（" + w2ucem::TargetParser::to_string(target) + "）:\n";
      text += "  构建依赖: " + (plan->build_dep_packages.empty()
                                   ? std::string("(无)")
                                   : fmt::format("{}", fmt::join(plan->build_dep_packages, ", "))) +
              "\n";
      text += "  源: " + std::to_string(plan->sources.size()) + "  步骤: " +
              std::to_string(plan->steps.size()) + "\n";
    }
    detail_text = text;
  };
  refresh_templates();
  show_detail();

  // ---------- 组件 ----------
  auto topbar = Renderer([&] {
    const auto ch = std::string(kChannels[static_cast<std::size_t>(channel_index)]);
    return hbox({
               text(" w2ucem-tui " + std::string(w2ucem::W2UCEM_VERSION)) | bold,
               separator(),
               text(" 通道: " + ch) | color(Color::Cyan) | bold,
               separator(),
               text(" GPU: generic"),
               separator(),
               text(" ←/→ 切通道   ↑/↓ 选择   s 同步   b 构建   v 校验   q 退出"),
           }) |
           border;
  });

  auto list_panel = Renderer([&] {
    Elements rows;
    for (std::size_t i = 0; i < template_ids.size(); ++i) {
      const bool is_selected = static_cast<int>(i) == selected;
      rows.push_back(text(template_ids[i]) | (is_selected ? inverted : nothing));
    }
    if (rows.empty()) {
      rows.push_back(text("(空)"));
    }
    return vbox(std::move(rows)) | vscroll_indicator | frame;
  });

  auto detail_panel = Renderer([&] {
    return paragraph(detail_text) | frame;
  });

  auto queue_panel = Renderer([&] {
    return vbox({
               text(queue_text),
               filler(),
           }) |
           frame;
  });

  auto log_panel = Renderer([&] {
    Elements rows;
    for (const auto& line : log_sink->snapshot(15)) {
      rows.push_back(text(line));
    }
    if (rows.empty()) {
      rows.push_back(text("(日志)"));
    }
    return vbox(std::move(rows)) | frame;
  });

  auto main_renderer = Renderer(
      topbar, [&] {
        return vbox({
            topbar->Render(),
            hbox({
                     list_panel->Render() | size(WIDTH, EQUAL, 28) | border | flex,
                     detail_panel->Render() | flex | border,
                 }) |
                flex,
            hbox({
                     queue_panel->Render() | size(HEIGHT, EQUAL, 6) | flex | border,
                     log_panel->Render() | flex | border,
                 }) |
                size(HEIGHT, EQUAL, 8),
        });
      });

  // ---------- 事件 ----------
  auto handle_events = CatchEvent(main_renderer, [&](const Event& event) {
    if (event == Event::Character('q')) {
      if (building.load()) {
        queue_text = "构建进行中，等待完成后再退出";
      } else {
        return false;  // 退出循环
      }
    }
    if (event == Event::ArrowLeft) {
      channel_index = (channel_index + 2) % 3;
      show_detail();
    }
    if (event == Event::ArrowRight) {
      channel_index = (channel_index + 1) % 3;
      show_detail();
    }
    if (event == Event::ArrowUp && selected > 0) {
      --selected;
      show_detail();
    }
    if (event == Event::ArrowDown && selected + 1 < static_cast<int>(template_ids.size())) {
      ++selected;
      show_detail();
    }
    if (event == Event::Character('s')) {
      auto idx = facade->sync();
      queue_text = idx ? "已同步: " + std::to_string(idx->templates.size()) + " 个模板"
                       : "同步失败: " + idx.error().message;
      refresh_templates();
      show_detail();
    }
    if (event == Event::Character('b') && !template_ids.empty() && !building.load()) {
      building.store(true);
      queue_text = "构建中…";
      const std::string ref = template_ids[static_cast<std::size_t>(selected)];
      const w2ucem::Channel channel = static_cast<w2ucem::Channel>(channel_index);
      const w2ucem::BuildTarget target{channel, w2ucem::GpuVariant::Generic,
                                       w2ucem::Distro{w2ucem::DistroFamily::Debian, "stable"},
                                       "x86_64"};
      build_thread = std::thread([&, ref, target]() {
        auto result = facade->build_template(ref, target, "fake", false, true);
        queue_text = result ? "构建完成: " + result->build_id
                            : "构建失败: " + result.error().message;
        building.store(false);
      });
    }
    if (event == Event::Character('v') && !template_ids.empty()) {
      const std::string ref = template_ids[static_cast<std::size_t>(selected)];
      const w2ucem::BuildTarget target{static_cast<w2ucem::Channel>(channel_index),
                                       w2ucem::GpuVariant::Generic,
                                       w2ucem::Distro{w2ucem::DistroFamily::Debian, "stable"},
                                       "x86_64"};
      auto report = facade->verify_template(ref, target);
      queue_text = report ? "校验通过" : "校验失败: " + report.error().message;
    }
    return true;
  });

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(handle_events);

  if (build_thread.joinable()) {
    build_thread.join();
  }
  return 0;
}
