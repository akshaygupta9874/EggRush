# The Qt game. Sim/Content are linked as static libraries; the Sim never sees Qt GUI types.
add_executable(eggrush
  src/main.cpp
  src/AppController.cpp
  src/MainWindow.cpp
  src/RenderProbe.cpp
  src/Theme.cpp
  src/Art.cpp
  src/Fx.cpp
  src/Audio.cpp
  src/UiKit.cpp
  src/Screens.cpp
  src/GameScreen.cpp)
target_link_libraries(eggrush PRIVATE er_content er_sim Qt6::Widgets Qt6::OpenGLWidgets Qt6::Multimedia)
target_compile_definitions(eggrush PRIVATE QT_NO_CAST_FROM_BYTEARRAY)
er_set_warnings_ui(eggrush)
set_target_properties(eggrush PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  WIN32_EXECUTABLE ON
  MACOSX_BUNDLE ON)
add_dependencies(eggrush er_copy_data)

install(TARGETS eggrush BUNDLE DESTINATION . RUNTIME DESTINATION bin)
install(DIRECTORY "${ER_DATA_DIR_PATH}/" DESTINATION data)
