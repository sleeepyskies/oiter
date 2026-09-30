find_package(lyra REQUIRED)
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)

target_link_libraries(oiter PRIVATE
    bfg::lyra
    imgui::imgui

    2iREN::2iREN
    glfw
)

target_sources(oiter PRIVATE third_party/imgui/backends/imgui_impl_glfw.cpp)

target_include_directories(oiter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(OITER_LINUX OR OITER_WINDOWS)
    find_package(opengl_system REQUIRED)
    find_package(glad REQUIRED)

    target_link_libraries(2iREN PRIVATE glad::glad opengl::opengl)

    target_sources(oiter PRIVATE
        third_party/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()

if(OITER_MACOS)
    find_package(metal-cpp REQUIRED)

    target_link_libraries(2iREN PRIVATE metal-cpp::metal-cpp)

    target_sources(oiter PRIVATE third_party/imgui/backends/imgui_impl_metal.mm)
endif()
