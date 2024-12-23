cmake_minimum_required(VERSION 3.12)
project(Serwer)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(Serwer main.cpp)