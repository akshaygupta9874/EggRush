# er_content: JSON loading + validation + save system. Qt Core only, no GUI.
add_library(er_content STATIC
  src/Content.cpp
  src/SaveSystem.cpp)
add_library(er::content ALIAS er_content)
target_include_directories(er_content PUBLIC include)
target_link_libraries(er_content PUBLIC er_sim Qt6::Core)
target_compile_definitions(er_content PRIVATE ER_DATA_DIR="${ER_DATA_DIR_PATH}")
er_set_warnings(er_content)
