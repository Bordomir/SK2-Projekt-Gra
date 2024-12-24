cmake_minimum_required(VERSION 3.12)
project(Serwer)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Wpedantic)

# file(STRINGS ${CMAKE_HOME_DIRECTORY}/config.env ENV_FILE)
# foreach(VAR ${ENV_FILE})
#     string(REGEX MATCH "^[^=]*" KEY ${VAR})
#     string(REGEX REPLACE "^[^=]*=" "" VALUE ${VAR})
#     set(ENV{${KEY}} ${VALUE})
# endforeach()

# set(ENV{PORT} 8080)

# message($ENV{PORT})
# message($ENV{BUFFER_SIZE})

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(Serwer serwer.cpp)

target_link_libraries(Serwer PRIVATE nlohmann_json::nlohmann_json)