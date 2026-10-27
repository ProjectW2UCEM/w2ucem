// W2UCEM C ABI：opaque ctx 风格，供 SwiftUI/QT/GTK 等前端经 FFI 消费。
// alpha 仅验证接口存在性；1.0 前不承诺 ABI 稳定。
//
// 本头必须自包含：外部前端项目可能只拷贝这一个文件，因此 W2UCEM_API
// 在此内联定义，不依赖 w2ucem/export.h。
#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(W2UCEM_BUILD_SHARED)
#    define W2UCEM_API __declspec(dllexport)
#  else
#    define W2UCEM_API __declspec(dllimport)
#  endif
#else
#  define W2UCEM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define W2UCEM_CAPI_VERSION 0

typedef struct w2ucem_ctx w2ucem_ctx;

W2UCEM_API int w2ucem_capi_version(void);

#ifdef __cplusplus
}
#endif
