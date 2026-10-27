# w2ucem-cli：只面向 api::Facade（P3 引入），P0 仅 version 骨架。
FetchContent_MakeAvailable(cli11)

add_executable(w2ucem-cli
  main.cpp
  commands/repo_cmds.cpp
  commands/build_cmds.cpp
  commands/user_cmds.cpp
  commands/misc_cmds.cpp
)
target_link_libraries(w2ucem-cli PRIVATE w2ucem_core CLI11::CLI11)
w2ucem_apply_warnings(w2ucem-cli)
w2ucem_apply_sanitizers(w2ucem-cli)
