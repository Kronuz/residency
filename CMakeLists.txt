cmake_minimum_required(VERSION 3.14)
project(residency LANGUAGES CXX)

include(FetchContent)

# --- Dependencies: task (coroutines) and hashes (CRC-32C) ---
set(TASK_ASIO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(task GIT_REPOSITORY https://github.com/Kronuz/task.git GIT_TAG bbb2e14aecb0fe49bcfac2685d64408ef6177c23)
FetchContent_Declare(hashes GIT_REPOSITORY https://github.com/Kronuz/hashes.git GIT_TAG 7ab0c23325debfbaf4104044987ba6583d14808f)
FetchContent_MakeAvailable(task hashes)

# Header-only: residency.hh is the whole implementation.
add_library(residency INTERFACE)
target_include_directories(residency INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(residency INTERFACE cxx_std_20)
target_link_libraries(residency INTERFACE task::task hashes::hashes)
add_library(residency::residency ALIAS residency)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(residency_test test/test.cc)
  target_link_libraries(residency_test PRIVATE residency)
  add_test(NAME residency COMMAND residency_test)
endif()
