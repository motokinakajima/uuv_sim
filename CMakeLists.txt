cmake_minimum_required(VERSION 3.26)
project(uuv_sim)

set(CMAKE_CXX_STANDARD 17)

add_executable(uuv_sim main.cpp
        vec2.cpp
        vec2.h)
