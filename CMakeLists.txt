cmake_minimum_required(VERSION 3.12)
project(Serwer)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Wpedantic)

file(STRINGS ${CMAKE_HOME_DIRECTORY}/config.env ENV_FILE)
foreach(VAR ${ENV_FILE})
    string(REGEX MATCH "^[^=]*" KEY ${VAR})
    string(REGEX REPLACE "^[^=]*=" "" VALUE ${VAR})
    set(ENV{${KEY}} ${VALUE})
endforeach()

add_executable(Serwer main.cpp)