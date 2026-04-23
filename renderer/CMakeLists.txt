cmake_minimum_required(VERSION 3.25)
project(raylib_mem_testing LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
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

add_library(shared_layout INTERFACE)
target_include_directories(shared_layout INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/common)

add_executable(timestamp_writer src/writer/main.c)
target_link_libraries(timestamp_writer PRIVATE shared_layout raylib m rt pthread)

add_executable(framebuffer_viewer src/viewer/main.cpp)
target_link_libraries(framebuffer_viewer PRIVATE shared_layout raylib m rt pthread)
