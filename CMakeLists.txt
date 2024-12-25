cmake_minimum_required(VERSION 3.12)
project(Serwer)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

cmake_policy(SET CMP0135 NEW)

set(PROJECT_SOURCES
        serwer.cpp
        config.hpp
)

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(Serwer ${PROJECT_SOURCES})

target_link_libraries(Serwer PRIVATE nlohmann_json::nlohmann_json)