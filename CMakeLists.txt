cmake_minimum_required(VERSION 3.16)
project(mc_options_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build: full optimisation, no debug symbols
# Change to Debug for -g -O0 when debugging
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

add_executable(mc_engine src/main.cpp)

target_include_directories(mc_engine PRIVATE include)

# Link threading library (required for std::async)
find_package(Threads REQUIRED)
target_link_libraries(mc_engine Threads::Threads)