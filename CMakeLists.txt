cmake_minimum_required(VERSION 3.16)
project(mc_options_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native")

add_executable(mc_engine src/main.cpp)

target_include_directories(mc_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
)

find_package(Threads REQUIRED)
target_link_libraries(mc_engine Threads::Threads)