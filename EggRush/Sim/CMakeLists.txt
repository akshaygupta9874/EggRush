# er_sim: the deterministic, Qt-free simulation library (Section 35 / 48).
add_library(er_sim STATIC
  src/FairnessValidator.cpp
  src/Scoring.cpp
  src/World.cpp
  src/Bot.cpp)
add_library(er::sim ALIAS er_sim)
target_include_directories(er_sim PUBLIC include)
target_compile_features(er_sim PUBLIC cxx_std_20)
er_set_warnings(er_sim)
