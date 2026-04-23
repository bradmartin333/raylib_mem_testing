cmake_minimum_required(VERSION 3.25)
project(timestamp_writer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
)

FetchContent_MakeAvailable(raylib)

add_executable(timestamp_writer main.c)
target_include_directories(timestamp_writer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../shared)
target_link_libraries(timestamp_writer PRIVATE raylib m rt pthread)
