cmake_minimum_required(VERSION 3.30)
project(oiter)

# create compile_commands.json, needed for nvim navigation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(2iREN)

add_executable(oiter
        src/main.cpp

        src/bake.cpp

        src/methods/dual_depth_peeling.cpp
        src/imgui.hpp
)
include(dependencies.cmake)
target_include_directories(oiter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(oiter PRIVATE 2iren)

target_compile_definitions(oiter PRIVATE OITER_VFS="${CMAKE_CURRENT_SOURCE_DIR}")
