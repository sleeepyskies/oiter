cmake_minimum_required(VERSION 3.30)
project(oiter)

# create compile_commands.json, needed for nvim navigation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SIREN_BUILD_EXAMPLES OFF CACHE BOOL "Build 2iREN examples")
set(SIREN_BUILD_TESTS OFF CACHE BOOL "Build 2iREN tests")
set(SIREN_SINGLE_THREADED ON CACHE BOOL "Compile 2iREN without multithreading")

add_subdirectory(2iREN)

add_executable(oiter
        src/main.cpp
        src/app.cpp

        src/bake.cpp

        src/methods/dual_depth_peeling/dual_depth_peeling.cpp
        src/methods/depth_peeling/depth_peeling.cpp
        src/methods/oit_method.cpp
        src/skybox.hpp
        src/skybox.cpp
        src/methods/k_buffer/k_buffer.cpp
        src/methods/a_buffer/a_buffer.cpp
        src/cli.cpp
)

target_compile_features(oiter PRIVATE cxx_std_23)
set_target_properties(oiter PROPERTIES
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

include(dependencies.cmake)

target_include_directories(
    oiter
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(oiter PRIVATE
    OITER_VFS="${CMAKE_CURRENT_SOURCE_DIR}"
)
