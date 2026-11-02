cmake_minimum_required(VERSION 3.14)
project(residency LANGUAGES CXX)

include(FetchContent)

# --- Dependencies: task (coroutines) and crc32c (CRC-32C) ---
set(TASK_ASIO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(task GIT_REPOSITORY https://github.com/Kronuz/task.git GIT_TAG 8cad661e0dd3bd1c74b964cdc0ee42066f568838)
FetchContent_Declare(crc32c GIT_REPOSITORY https://github.com/Kronuz/crc32c.git GIT_TAG 0d5460d277170e92f33ffc148746f1a7c03eabe1)
FetchContent_MakeAvailable(task crc32c)

# Header-only: residency.hh is the whole implementation.
add_library(residency INTERFACE)
target_include_directories(residency INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(residency INTERFACE cxx_std_20)
target_link_libraries(residency INTERFACE task::task crc32c::crc32c)
add_library(residency::residency ALIAS residency)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(residency_test test/test.cc)
  target_link_libraries(residency_test PRIVATE residency)
  add_test(NAME residency COMMAND residency_test)
endif()
