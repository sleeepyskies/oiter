find_package(lyra REQUIRED)
find_package(imgui REQUIRED)

target_link_libraries(oiter PRIVATE bfg::lyra)
target_link_libraries(oiter PRIVATE imgui::imgui)

target_sources(oiter PRIVATE
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
)