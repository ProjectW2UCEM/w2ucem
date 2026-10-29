#include "w2ucem/build/image_lock.hpp"

#include "w2ucem/util/error.hpp"

namespace w2ucem {

Result<std::string> ImageDigestLock::pull_and_verify(ContainerBackend& backend,
                                                     const std::string& image,
                                                     const std::string& declared_digest) {
  const auto actual = backend.pull_image(image, declared_digest);
  if (!actual) {
    return tl::unexpected(std::move(actual.error()));
  }
  if (*actual != declared_digest) {
    return tl::unexpected(Error::make(
        ErrorCode::HashMismatch,
        "镜像 digest 与模板声明不符（实际 " + *actual + " != 声明 " + declared_digest + "）",
        image));
  }
  return actual;
}

}  // namespace w2ucem
