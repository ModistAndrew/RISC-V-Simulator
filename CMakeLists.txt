cmake_minimum_required(VERSION 3.22)
project(risc_v)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-g -O2")

add_executable(code src/main.cpp)