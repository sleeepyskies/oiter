#include "gui.hpp"

#include <format>
#include <stdexcept>
#include <utility>

#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include "2iREN/graphics/device.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/window.hpp"

#include "methods/oit_method.hpp"

namespace {
auto render_method_selector(const oiter::MethodKind current_method, gui::Actions& actions) -> void {
    if (!ImGui::CollapsingHeader("OIT Method", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto method = static_cast<int>(current_method.value);

    ImGui::RadioButton(
        "Depth Peeling", &method, std::to_underlying(oiter::MethodKind::DepthPeeling)
    );
    ImGui::RadioButton(
        "Dual Depth Peeling", &method, std::to_underlying(oiter::MethodKind::DualDepthPeeling)
    );
    ImGui::RadioButton("A-Buffer", &method, std::to_underlying(oiter::MethodKind::ABuffer));
    ImGui::RadioButton("K-Buffer", &method, std::to_underlying(oiter::MethodKind::KBuffer));

    if (method != std::to_underlying(current_method.value)) {
        actions.oit_method = static_cast<oiter::MethodKind::Value>(method);
    }
}

auto render_statistics(const siren::Statistics& statistics, const oiter::FrameStats& frame_stats)
    -> void {
    if (!ImGui::CollapsingHeader("Render Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto frame = std::format("Current Frame: {}", frame_stats.frame);
    ImGui::TextUnformatted(frame.c_str());
    ImGui::Text("FPS: %.1f fps", frame_stats.fps);

    ImGui::Separator();

    ImGui::Text("Frame took %ums", frame_stats.full_frame_ms);
    ImGui::Text("OIT render took %ums", frame_stats.oit_render_ms);

    ImGui::Separator();

    ImGui::Text("Bind Graphics Pipeline: %u", statistics.count_bind_graphics_pipeline);
    ImGui::Text("Set Viewport: %u", statistics.count_set_viewport);
    ImGui::Text("Bind Vertex Buffer: %u", statistics.count_bind_vertex_buffer);
    ImGui::Text("Bind Index Buffer: %u", statistics.count_bind_index_buffer);
    ImGui::Text("Bind Uniform Buffer: %u", statistics.count_bind_uniform_buffer);
    ImGui::Text("Bind Shader Storage Buffer: %u", statistics.count_bind_shader_storage_buffer);
    ImGui::Text("Bind Sampled Image: %u", statistics.count_bind_sampled_image);
    ImGui::Text("Bind Storage Image: %u", statistics.count_bind_storage_image);
    ImGui::Text("Draw Arrays: %u", statistics.count_draw_arrays);
    ImGui::Text("Draw Indexed: %u", statistics.count_draw_indexed);
    ImGui::Text("Upload Buffer: %u", statistics.count_upload_buffer);
    ImGui::Text("Upload Image: %u", statistics.count_upload_image);
    ImGui::Text("Draw Calls: %u", statistics.count_draw_calls);
    ImGui::Text("Render Passes: %u", statistics.count_render_passes);
}

auto render_scene_controls(
    siren::PerspectiveCamera& camera, siren::PerspectiveCameraController& controller
) -> void {
    if (!ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto position = camera.position();
    auto speed          = controller.speed();
    auto sensitivity    = controller.sensitivity();
    auto fov            = camera.fov();

    ImGui::Text("Camera Position: (%f, %f, %f)", position.x, position.y, position.z);
    ImGui::Text("Camera Yaw: %f", camera.yaw());
    ImGui::Text("Camera Pitch: %f", camera.pitch());
    ImGui::SliderFloat("Camera Fov", &fov, 30.f, 120.f);
    ImGui::SliderFloat("Camera Speed", &speed, 0.f, 20.f);
    ImGui::SliderFloat("Camera Sensitivity", &sensitivity, 0.f, 1.f);

    controller.set_speed(speed);
    controller.set_sensitivity(sensitivity);
    camera.set_fov(fov);
}

auto render_debug_panel(const gui::DebugPanel& panel, gui::Actions& actions) -> void {
    const auto& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(350, io.DisplaySize.y), ImGuiCond_Always);

    ImGui::Begin(
        "Debug Information",
        nullptr,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
    );

    render_method_selector(panel.method_kind, actions);
    render_statistics(panel.statistics, panel.frame_stats);
    render_scene_controls(panel.camera, panel.controller);

    const auto title = std::format("{} Controls", panel.oit_method.name());
    if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        panel.oit_method.render_debug_info();
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

    ImGui::TextUnformatted("F1 - TOGGLE DEBUG");
    ImGui::TextUnformatted("F2 - RELOAD SHADERS");
    ImGui::TextUnformatted("F3 - RENDER SKYBOX");

    ImGui::End();
}
} // namespace

namespace gui {
Session::Session(const siren::Window& window, siren::Device& device) : m_device(device) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    if (!ImGui_ImplGlfw_InitForOpenGL(window.handle(), true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui GLFW backend.");
    }

    ImGui::StyleColorsDark();

    auto renderer_initialized = false;
    try {
        m_device.render_thread().spawn([&renderer_initialized] {
            renderer_initialized = ImGui_ImplOpenGL3_Init("#version 460");
        });
        m_device.wait_idle();
    } catch (...) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw;
    }

    if (!renderer_initialized) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Failed to initialize the ImGui OpenGL backend.");
    }
}

Session::~Session() {
    m_device.wait_idle();
    m_device.render_thread().spawn([] { ImGui_ImplOpenGL3_Shutdown(); });
    m_device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

auto wants_mouse_input() -> bool { return ImGui::GetIO().WantCaptureMouse; }

auto wants_keyboard_input() -> bool { return ImGui::GetIO().WantCaptureKeyboard; }

auto prepare_frame() -> void { ImGui_ImplOpenGL3_NewFrame(); }

auto build_frame(const bool debug_panel_visible, const DebugPanel& panel) -> Actions {
    auto actions = Actions{};

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (debug_panel_visible) {
        render_debug_panel(panel, actions);
    }
    ImGui::Render();

    return actions;
}

auto draw_frame() -> void { ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
} // namespace gui
