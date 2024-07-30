cmake_minimum_required(VERSION 3.22)
project(risc_v)

set(CMAKE_CXX_STANDARD 20)

add_executable(risc_v src/main.cpp
        src/processor.cpp)
