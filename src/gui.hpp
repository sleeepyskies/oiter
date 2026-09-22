#pragma once

#include <Metal/MTLRenderPass.hpp>
#include <format>
#include <optional>
#include <utility>

#include <imgui.h>

#include <imgui/backends/imgui_impl_glfw.h>
#include "2iREN/core/base.hpp"
#if defined(OITER_LINUX) || defined(OITER_WINDOWS)
#include <imgui/backends/imgui_impl_opengl3.h>
#elifdef OITER_MACOS
#include <imgui/backends/imgui_impl_metal.h>

#include "2iREN/graphics/backend/metal/commands.hpp"
#include "2iREN/graphics/backend/metal/device.hpp"
#include "2iREN/graphics/backend/metal/util.hpp"
#endif

#include <GLFW/glfw3.h>

#include "2iREN/utility/time.hpp"
#include "2iREN/window/window.hpp"

#include "app/interactive.hpp"
#include "methods/method_kind.hpp"
#include "methods/oit_method.hpp"

class OitMethod;
namespace oiter { }

namespace gui {
struct DebugPanelActions {
    std::optional<oiter::MethodKind> oit_method;
};

inline auto init(const siren::Window& window, [[maybe_unused]] siren::Device& device) -> void {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

#if defined(OITER_LINUX) || defined(OITER_WINDOWS)
    ImGui_ImplGlfw_InitForOpenGL(window.native_handle(), true);
    ImGui_ImplOpenGL3_Init("#version 460");
#elifdef OITER_MACOS
    ImGui_ImplGlfw_InitForOther(window.native_handle(), true);
    ImGui_ImplMetal_Init(dynamic_cast<siren::MetalDevice&>(device).metal_device());
#endif

    ImGui::StyleColorsDark();
}

inline auto shutdown() -> void {
#if defined(OITER_LINUX) || defined(OITER_WINDOWS)
    ImGui_ImplOpenGL3_Shutdown();
#elifdef OITER_MACOS
    ImGui_ImplMetal_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

inline auto new_frame(
    [[maybe_unused]] siren::Device& device,
    [[maybe_unused]] siren::CommandBuffer& cmds,
    [[maybe_unused]] const siren::ImageHandle backbuffer
) -> void {
#if defined(OITER_LINUX) || defined(OITER_WINDOWS)
    ImGui_ImplOpenGL3_NewFrame();
#elifdef OITER_MACOS
    auto mtldevice      = static_cast<siren::MetalDevice&>(device);
    auto* mtlbackbuffer = mtldevice.metal_texture(backbuffer);
    auto descriptor     = siren::metal::transfer_ptr(MTL::RenderPassDescriptor::alloc()->init());
    descriptor->colorAttachments()->object(0)->setTexture(mtlbackbuffer);
    ImGui_ImplMetal_NewFrame(descriptor.get());
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

inline auto end_frame(
    [[maybe_unused]] siren::Device& device,
    [[maybe_unused]] siren::CommandBuffer& cmds,
    [[maybe_unused]] const siren::ImageHandle backbuffer
) -> void {
    ImGui::Render();

#if defined(OITER_LINUX) || defined(OITER_WINDOWS)
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#elifdef OITER_MACOS
    AUTORELEASE {
        auto& mtl_cmds      = static_cast<siren::metal::CommandBuffer&>(cmds);
        auto mtldevice      = static_cast<siren::MetalDevice&>(device);
        auto* mtlbackbuffer = mtldevice.metal_texture(backbuffer);
        auto descriptor = siren::metal::transfer_ptr(MTL::RenderPassDescriptor::alloc()->init());
        descriptor->colorAttachments()->object(0)->setTexture(mtlbackbuffer);
        descriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);
        descriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
        auto encoder = siren::metal::retain_ptr(
            mtl_cmds.metal_commandbuffer()->renderCommandEncoder(descriptor.get())
        );
        ImGui_ImplMetal_RenderDrawData(
            ImGui::GetDrawData(),
            mtl_cmds.metal_commandbuffer(),
            encoder.get()
        );
        encoder->endEncoding();
    }
#endif
}

/// @brief Draws the debug overlay and returns requested state changes.
[[nodiscard]]
inline auto render_debug(
    siren::Device& device,
    siren::CommandBuffer& cmds,
    siren::ImageHandle backbuffer,
    const siren::Statistics& statistics,
    const oiter::FrameStats& frame_stats,
    oiter::OitMethod& oit_method
) -> DebugPanelActions {
    DebugPanelActions actions;

    new_frame(device, cmds, backbuffer);

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
            "Depth Peeling",
            &method,
            std::to_underlying(oiter::MethodKind::DepthPeeling)
        );

        ImGui::RadioButton(
            "Dual Depth Peeling",
            &method,
            std::to_underlying(oiter::MethodKind::DualDepthPeeling)
        );

        ImGui::RadioButton("A-Buffer", &method, std::to_underlying(oiter::MethodKind::ABuffer));

        ImGui::RadioButton(
            "Screen Door",
            &method,
            std::to_underlying(oiter::MethodKind::ScreenDoor)
        );

        if (method != (siren::i32)std::to_underlying(oit_method.kind().value)) {
            actions.oit_method = static_cast<oiter::MethodKind::Value>(method);
        }
    }

    if (ImGui::CollapsingHeader("Render Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current Frame: %lu", (long)siren::time::current_frame());
        ImGui::Text("FPS: %.1f fps", frame_stats.fps);

        ImGui::Separator();

        ImGui::Text("Frame took %ums", frame_stats.full_frame_ms);
        ImGui::Text("Oit Render took %ums", frame_stats.oit_render_ms);

        ImGui::Separator();

        ImGui::Text("Bind Graphics Pipeline: %u", statistics.count_bind_graphics_pipeline);
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
        ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoBackground
    );

    ImGui::Text("F1 - TOGGLE DEBUG   ");
    ImGui::Text("F2 - RELOAD SHADERS ");
    ImGui::Text("F3 - RENDER SKYBOX  ");
    ImGui::End();

    end_frame(device, cmds, backbuffer);

    return actions;
}
} // namespace gui
