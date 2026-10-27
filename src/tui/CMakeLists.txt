# w2ucem-tui：只面向 api::Facade；P5 填充四区界面，P0 仅占位屏。
FetchContent_MakeAvailable(ftxui)

add_executable(w2ucem-tui
  main.cpp
)
target_link_libraries(w2ucem-tui PRIVATE w2ucem_core ftxui::screen ftxui::dom ftxui::component)
w2ucem_apply_warnings(w2ucem-tui)
w2ucem_apply_sanitizers(w2ucem-tui)
