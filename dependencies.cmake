find_package(lyra REQUIRED)
find_package(imgui REQUIRED)

target_link_libraries(oiter PRIVATE
        bfg::lyra
        imgui::imgui
)

target_sources(oiter PRIVATE
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(oiter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)
