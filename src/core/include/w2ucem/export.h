// W2UCEM 符号导出宏（C++ 公共头）。
// w2ucem_capi（shared）用 W2UCEM_API 显式导出 C ABI 符号；
// 其余目标为 static，链接时不需导出。
// 注意：w2ucem/capi/w2ucem_capi.h 必须能被外部项目单独包含，
// 因此它在自身内联定义 W2UCEM_API，不依赖本头。
#pragma once

#if defined(_WIN32)
#  if defined(W2UCEM_BUILD_SHARED)
#    define W2UCEM_API __declspec(dllexport)
#  else
#    define W2UCEM_API __declspec(dllimport)
#  endif
#else
#  define W2UCEM_API __attribute__((visibility("default")))
#endif
