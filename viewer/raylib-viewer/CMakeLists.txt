cmake_minimum_required(VERSION 3.25)
project(raylib_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
)

FetchContent_MakeAvailable(raylib)

add_executable(raylib_viewer main.cpp)
target_include_directories(raylib_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../shared)
target_link_libraries(raylib_viewer PRIVATE raylib m rt pthread)
