#pragma once

#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "2iren/window.hpp"

namespace oiter {

inline auto init_imgui(const siren::Window& window) -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");

    ImGui::StyleColorsDark();
}

inline auto new_imgui_frame() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

inline auto end_imgui_frame() -> void {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

inline auto render_debug_info(const siren::Statistics& statistics, const siren::PerspectiveCamera& camera) -> void {
    new_imgui_frame();

    ImGui::Begin("Debug");

    if (ImGui::BeginTabBar("DebugTabs")) {
        if (ImGui::BeginTabItem("Render Statistics")) {
            static siren::usize interval = 0;
            static auto fps              = interval;
            static auto frametime        = interval;

            interval++;

            if (!(interval % 60)) {
                frametime = siren::time::delta_ms();
                fps       = 1 / siren::time::delta_s();
            }

            ImGui::Text("FPS: %u fps", fps);
            ImGui::Text("Frametime: %u ms", frametime);

            ImGui::Separator();

            ImGui::Text("Bind Graphics Pipeline: %u", statistics.count_bind_graphics_pipeline);
            ImGui::Text("Set Viewport: %u", statistics.count_set_viewport);
            ImGui::Text("Bind Vertex Buffer: %u", statistics.count_bind_vertex_buffer);
            ImGui::Text("Bind Index Buffer: %u", statistics.count_bind_index_buffer);
            ImGui::Text("Bind Uniform Buffer: %u", statistics.count_bind_uniform_buffer);
            ImGui::Text("Bind Sampled Image: %u", statistics.count_bind_sampled_image);
            ImGui::Text("Bind Storage Image: %u", statistics.count_bind_storage_image);
            ImGui::Text("Draw Arrays: %u", statistics.count_draw_arrays);
            ImGui::Text("Draw Indexed: %u", statistics.count_draw_indexed);
            ImGui::Text("Upload Buffer: %u", statistics.count_upload_buffer);
            ImGui::Text("Upload Image: %u", statistics.count_upload_image);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scene")) {
            const auto position = camera.position();

            ImGui::Text("Camera Position: (%f, %f, %f)", position.x, position.y, position.z);
            ImGui::Text("Camera Yaw: %f", camera.yaw());
            ImGui::Text("Camera Pitch: %f", camera.pitch());

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    end_imgui_frame();
}

}  // namespace oiter
