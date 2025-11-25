cmake_minimum_required(VERSION 3.10)

project(mod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
    mod
    SHARED
    mod.cpp
)

target_link_libraries(
    mod
    log
)
