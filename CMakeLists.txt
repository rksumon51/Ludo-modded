cmake_minimum_required(VERSION 3.10)
project(libmod)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mod SHARED libmod.cpp)

find_library(log-lib log)
target_link_libraries(mod ${log-lib} dl)
