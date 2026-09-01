#pragma once

#include <format>
#include <optional>
#include <utility>

#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "2iREN/window.hpp"
#include "app/interactive.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"

namespace oiter {
class OitMethod;
}

namespace gui {
struct DebugPanelActions {
    std::optional<oiter::MethodKind> oit_method;
};

inline auto init(const siren::Window& window) -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");

    ImGui::StyleColorsDark();
}

inline auto shutdown() -> void {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
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

/// @brief Draws the debug overlay and returns requested state changes.
[[nodiscard]] inline auto render_debug(
    const siren::Statistics& statistics,
    const oiter::FrameStats& frame_stats,
    oiter::OitMethod& oit_method
) -> DebugPanelActions {
    DebugPanelActions actions;

    new_frame();

    const auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(350, io.DisplaySize.y), ImGuiCond_Always);

    ImGui::Begin(
        "Debug Information",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
    );

    if (ImGui::CollapsingHeader("OIT Method", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto method = static_cast<siren::i32>(oit_method.kind());

        ImGui::RadioButton(
            "Depth Peeling", &method, std::to_underlying(oiter::MethodKind::DepthPeeling)
        );

        ImGui::RadioButton(
            "Dual Depth Peeling", &method, std::to_underlying(oiter::MethodKind::DualDepthPeeling)
        );

        ImGui::RadioButton("A-Buffer", &method, std::to_underlying(oiter::MethodKind::ABuffer));

        ImGui::RadioButton("K-Buffer", &method, std::to_underlying(oiter::MethodKind::KBuffer));

        if (method != std::to_underlying(oit_method.kind().value)) {
            actions.oit_method = static_cast<oiter::MethodKind::Value>(method);
        }
    }

    if (ImGui::CollapsingHeader("Render Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current Frame: %lu", frame_stats.frame);
        ImGui::Text("FPS: %.1f fps", frame_stats.fps);

        ImGui::Separator();

        ImGui::Text("Frame took %ums", frame_stats.full_frame_ms);
        ImGui::Text("Oit Render took %ums", frame_stats.oit_render_ms);

        ImGui::Separator();

        ImGui::Text("Bind Graphics Pipeline: %u", statistics.count_bind_graphics_pipeline);
        ImGui::Text("Set Viewport: %u", statistics.count_set_viewport);
        ImGui::Text("Bind Vertex Buffer: %u", statistics.count_bind_vertex_buffer);
        ImGui::Text("Bind Index Buffer: %u", statistics.count_bind_index_buffer);
        ImGui::Text("Bind Uniform Buffer: %u", statistics.count_bind_uniform_buffer);
        ImGui::Text("Bind Uniform Buffer: %u", statistics.count_bind_shader_storage_buffer);
        ImGui::Text("Bind Sampled Image: %u", statistics.count_bind_sampled_image);
        ImGui::Text("Bind Storage Image: %u", statistics.count_bind_storage_image);
        ImGui::Text("Draw Arrays: %u", statistics.count_draw_arrays);
        ImGui::Text("Draw Indexed: %u", statistics.count_draw_indexed);
        ImGui::Text("Upload Buffer: %u", statistics.count_upload_buffer);
        ImGui::Text("Upload Image: %u", statistics.count_upload_image);
        ImGui::Text("Draw Calls: %u", statistics.count_draw_calls);
        ImGui::Text("Render Passes: %u", statistics.count_render_passes);
    }

    const auto title = std::format("{} Controls", oit_method.name().data());
    if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        oit_method.render_debug_info();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 175, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(175, 100), ImGuiCond_Always);

    ImGui::Begin(
        "Debug Controls",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground
    );

    ImGui::Text("F1 - TOGGLE DEBUG   ");
    ImGui::Text("F2 - RELOAD SHADERS ");
    ImGui::Text("F3 - RENDER SKYBOX  ");
    ImGui::End();

    end_frame();

    return actions;
}
} // namespace gui
