cmake_minimum_required(VERSION 3.10)
project(libmod_patch32)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mod SHARED mod_arm32.cpp)

find_library(log-lib log)
target_link_libraries(mod ${log-lib} dl)
