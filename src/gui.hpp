#pragma once

#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "2iren/window.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"

namespace oiter {
class OitMethod;
}

namespace gui {
struct State {
    bool show                = true;
    siren::u32 full_frame_ms = 0;
    siren::u32 oit_render_ms = 0;
    siren::u64 frame         = 0;
};


inline auto init(const siren::Window& window) -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");

    ImGui::StyleColorsDark();
}

inline auto new_frame() -> void {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

inline auto end_frame() -> void {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/** @brief Returns true if method should be updated. */
[[nodiscard]] inline auto render_debug_info(
    const siren::Statistics& statistics,
    const siren::PerspectiveCamera& camera,
    siren::PerspectiveCameraController& controller,
    oiter::OitMethod* oit_method,
    oiter::MethodKind& method_kind,
    State& guistate
) -> bool {
    bool changed = false;

    new_frame();
    static float speed       = controller.speed();
    static float sensitivity = controller.sensitivity();
    static auto fps          = guistate.frame;

    if (!(guistate.frame % 60)) {
        fps = 1 / siren::time::delta_s();
    }

    const auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(350, io.DisplaySize.y), ImGuiCond_Always);

    ImGui::Begin(
        "Debug Information",
        nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse
    );

    if (ImGui::CollapsingHeader("OIT Method", ImGuiTreeNodeFlags_DefaultOpen)) {
        static auto method = static_cast<int>(method_kind.value);
        const auto old     = method;

        ImGui::RadioButton(
            "Dual Depth Peeling",
            &method,
            std::to_underlying(oiter::MethodKind::DualDepthPeeling)
        );

        ImGui::RadioButton(
            "Depth Peeling",
            &method,
            std::to_underlying(oiter::MethodKind::DepthPeeling)
        );

        method_kind = static_cast<oiter::MethodKind::Value>(method);

        if (method != old) {
            changed = true;
        }
    }

    if (ImGui::CollapsingHeader("Render Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current Frame: %lu", guistate.frame);
        ImGui::Text("FPS: %lu fps", fps);

        ImGui::Separator();

        ImGui::Text("Frame took %ums", guistate.full_frame_ms);
        ImGui::Text("Oit Render took %ums", guistate.oit_render_ms);

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
        ImGui::Text("Draw Calls: %u", statistics.count_draw_calls);
        ImGui::Text("Render Passes: %u", statistics.count_render_passes);
    }

    if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto position = camera.position();

        ImGui::Text("Camera Position: (%f, %f, %f)", position.x, position.y, position.z);
        ImGui::Text("Camera Yaw: %f", camera.yaw());
        ImGui::Text("Camera Pitch: %f", camera.pitch());
        ImGui::SliderFloat("Camera Speed", &speed, 0.f, 20.f);
        ImGui::SliderFloat("Camera Sensitivity", &sensitivity, 0.f, 1.f);

        if (speed != controller.speed()) { controller.set_speed(speed); }
        if (sensitivity != controller.sensitivity()) { controller.set_sensitivity(sensitivity); }
    }

    const auto title = std::format("{} Controls", oit_method->name().data());
    if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        oit_method->render_debug_info();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 175, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(175, 100), ImGuiCond_Always);

    ImGui::Begin(
        "Debug Controls",
        nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBackground
    );

    ImGui::Text("F1 - TOGGLE DEBUG      ");
    ImGui::Text("F2 - RELOAD SHADERS    ");
    ImGui::Text("F3 - TOGGLE VSYNC      ");
    ImGui::Text("F4 - RENDER SKYBOX     ");
    ImGui::End();

    end_frame();

    return changed;
}
} // namespace gui
