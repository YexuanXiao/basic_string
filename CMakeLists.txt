cmake_minimum_required(VERSION 3.29)

project(benchmark)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(benchmark benchmark.cpp)