cmake_minimum_required(VERSION 3.25)
project(sdl2_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 QUIET CONFIG)

set(SDL2_FALLBACK_CFLAGS "")
set(SDL2_FALLBACK_LIBS "")
if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2::SDL2-static)
    find_program(SDL2_CONFIG_EXECUTABLE NAMES sdl2-config)
    if(SDL2_CONFIG_EXECUTABLE)
        execute_process(
            COMMAND ${SDL2_CONFIG_EXECUTABLE} --cflags
            OUTPUT_VARIABLE SDL2_CFLAGS_RAW
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE SDL2_CFLAGS_STATUS)
        execute_process(
            COMMAND ${SDL2_CONFIG_EXECUTABLE} --libs
            OUTPUT_VARIABLE SDL2_LIBS_RAW
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE SDL2_LIBS_STATUS)

        if(SDL2_CFLAGS_STATUS EQUAL 0 AND SDL2_LIBS_STATUS EQUAL 0)
            separate_arguments(SDL2_FALLBACK_CFLAGS NATIVE_COMMAND "${SDL2_CFLAGS_RAW}")
            separate_arguments(SDL2_FALLBACK_LIBS NATIVE_COMMAND "${SDL2_LIBS_RAW}")
        endif()
    endif()

    if(NOT SDL2_FALLBACK_LIBS)
        if(EXISTS "/usr/bin/pkg-config")
            set(PKG_CONFIG_EXECUTABLE "/usr/bin/pkg-config" CACHE FILEPATH "pkg-config executable" FORCE)
        endif()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    endif()
endif()

add_executable(sdl2_viewer main.cpp)
target_include_directories(sdl2_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../shared)

if(TARGET SDL2::SDL2)
    target_link_libraries(sdl2_viewer PRIVATE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(sdl2_viewer PRIVATE SDL2::SDL2-static)
elseif(SDL2_FALLBACK_LIBS)
    target_compile_options(sdl2_viewer PRIVATE ${SDL2_FALLBACK_CFLAGS})
    target_link_libraries(sdl2_viewer PRIVATE ${SDL2_FALLBACK_LIBS})
elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(sdl2_viewer PRIVATE PkgConfig::SDL2)
else()
    target_include_directories(sdl2_viewer PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(sdl2_viewer PRIVATE ${SDL2_LIBRARIES})
endif()

target_link_libraries(sdl2_viewer PRIVATE rt pthread)