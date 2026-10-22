cmake_minimum_required(VERSION 3.16)
project(stl_from_scratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# e.g. -DSTL_SANITIZE=address,undefined or -DSTL_SANITIZE=thread
set(STL_SANITIZE "" CACHE STRING "Comma-separated list of sanitizers to enable")
if(STL_SANITIZE)
  add_compile_options(-fsanitize=${STL_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=${STL_SANITIZE})
endif()

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

function(add_stl_test test_name)
  add_executable(${test_name} tests/${test_name}.cc)
  target_link_libraries(${test_name}
    PRIVATE
      stl_from_scratch
      Catch2::Catch2WithMain
  )
  # Fail on deadlock instead of hanging
  catch_discover_tests(${test_name} PROPERTIES TIMEOUT 300)
endfunction()

add_stl_test(test_vector)
add_stl_test(test_unique_ptr)
add_stl_test(test_thread_pool)
add_stl_test(test_lock_free_queue)
