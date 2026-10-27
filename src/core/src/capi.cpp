// C ABI 占位实现：P6 补齐完整接口（opaque ctx + init/shutdown/...）。
#include "w2ucem/capi/w2ucem_capi.h"

extern "C" W2UCEM_API int w2ucem_capi_version(void) {
  return W2UCEM_CAPI_VERSION;
}
