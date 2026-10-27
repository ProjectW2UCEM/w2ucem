# 统一警告策略：所有 W2UCEM 目标一致启用。

function(w2ucem_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endfunction()

# 开发期 sanitizer（CI 用）：W2UCEM_ENABLE_SANITIZERS=ON
function(w2ucem_apply_sanitizers target)
  if(W2UCEM_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
