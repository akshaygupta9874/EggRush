cmake_minimum_required(VERSION 3.21)
project(EggRush VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(ER_BUILD_APP "Build the Qt game (needs Qt Widgets, OpenGLWidgets, Multimedia)" ON)
option(ER_BUILD_TESTS "Build the QtTest suite" ON)
option(ER_WARNINGS_AS_ERRORS "Treat warnings as errors (CI)" OFF)

# Strict warnings, applied per target so third-party code is never affected.
function(er_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(ER_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
                                             -Wno-sign-conversion)
    if(ER_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# The UI layer allows implicit double -> float narrowing (Qt geometry is double), the
# Sim / Content / Tools layers do not.
function(er_set_warnings_ui target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(ER_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core)
set(ER_DATA_DIR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/data")

add_subdirectory(Sim)
add_subdirectory(Content)

# Copy data next to the binaries so a build tree runs without installing.
add_custom_target(er_copy_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${ER_DATA_DIR_PATH}" "${CMAKE_BINARY_DIR}/bin/data"
  COMMENT "Copying game data")

if(ER_BUILD_TESTS)
  enable_testing()
  find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
  add_subdirectory(Tests)
endif()

add_subdirectory(Tools)

if(ER_BUILD_APP)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)
  add_subdirectory(App)
endif()
