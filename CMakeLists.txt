cmake_minimum_required(VERSION 3.12)
project(Serwer)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
set(CMAKE_CXX_STANDARD 20)

set(COMPILE_FLAGS "-Wall -Wextra")

add_executable(Serwer main.cpp)