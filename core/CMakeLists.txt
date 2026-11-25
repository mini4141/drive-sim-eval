cmake_minimum_required(VERSION 3.28)
project(simcore LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SIMCORE_BUILD_TESTS "Build GoogleTest suite" ON)
option(SIMCORE_BUILD_PYTHON "Build the pybind11 module" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# System packages first (apt), pinned downloads as fallback.
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Eigen3
  URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  FIND_PACKAGE_ARGS 3.4 NO_MODULE)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  FIND_PACKAGE_ARGS NO_MODULE)
set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
  URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz)
FetchContent_MakeAvailable(Eigen3 nlohmann_json spdlog)

add_library(simcore_core STATIC src/util.cpp src/components.cpp src/engine.cpp)
target_include_directories(simcore_core PUBLIC include)
target_link_libraries(simcore_core PUBLIC Eigen3::Eigen nlohmann_json::nlohmann_json spdlog::spdlog)
# Determinism: no fast-math, no FMA contraction differences between builds.
target_compile_options(simcore_core PUBLIC -O2 -ffp-contract=off -Wall -Wextra)

if(SIMCORE_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    FIND_PACKAGE_ARGS NAMES GTest)
  FetchContent_MakeAvailable(googletest)
  add_executable(simcore_tests tests/test_core.cpp)
  target_link_libraries(simcore_tests PRIVATE simcore_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(simcore_tests)
endif()

if(SIMCORE_BUILD_PYTHON)
  add_subdirectory(../bindings bindings)
endif()
