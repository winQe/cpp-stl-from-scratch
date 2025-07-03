cmake_minimum_required(VERSION 3.16)
project(stl_from_scratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Catch2
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Core library
file(GLOB_RECURSE STLLIB_SOURCES
    src/*.cc
    include/stl/*.h
)
add_library(stl_from_scratch INTERFACE)
target_include_directories(stl_from_scratch
  INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


# Tests
enable_testing()

add_executable(run_tests
  tests/test_vector.cc
  tests/test_unique_ptr.cc
  tests/test_thread_pool.cc
)

target_link_libraries(run_tests
  PRIVATE
    stl_from_scratch   
    Catch2::Catch2WithMain
)
catch_discover_tests(run_tests)
