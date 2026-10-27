#include "w2ucem/config/data_dirs.hpp"

#include <cstdlib>

#include "w2ucem/channel/channel.hpp"
#include "w2ucem/channel/distro.hpp"
#include "w2ucem/channel/variant.hpp"
#include "w2ucem/util/fs.hpp"

namespace w2ucem {

namespace {

std::string default_root() {
#if defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (home != nullptr) {
    return std::string(home) + "/Library/Application Support/w2ucem";
  }
#else
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg != nullptr && *xdg != '\0') {
    return std::string(xdg) + "/w2ucem";
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr) {
    return std::string(home) + "/.local/share/w2ucem";
  }
#endif
  return "./.w2ucem-data";
}

}  // namespace

Result<DataDirs> DataDirs::create(const std::string& root_override) {
  DataDirs dirs;
  dirs.root_ = root_override.empty() ? default_root() : root_override;
  if (auto r = fs::ensure_dir(dirs.root_); !r) {
    return tl::unexpected(std::move(r.error()));
  }
  return dirs;
}

std::string DataDirs::runtime_dir(const std::string& template_id,
                                  const BuildTarget& target) const {
  return root_ + "/runtime/" + template_id + "/" + channel_str(target.channel) + "/" +
         family_str(target.distro.family) + "/" + gpu_variant_str(target.gpu);
}

std::string DataDirs::ledger_dir(const std::string& template_id,
                                 const BuildTarget& target) const {
  return runtime_dir(template_id, target) + "/ledger";
}

}  // namespace w2ucem
