find_package(lyra REQUIRED)
find_package(imgui REQUIRED)

target_link_libraries(oiter PRIVATE
        bfg::lyra
        imgui::imgui
        2iREN::2iREN
)

target_sources(oiter PRIVATE
        third_party/imgui/backends/imgui_impl_glfw.cpp
        third_party/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(oiter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
