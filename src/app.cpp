#include "app.hpp"

#include "2iren/asset/assets/gltf.hpp"
#include "2iren/input/input.hpp"
#include "2iren/rhi/context.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/resources/swapchain.hpp"
#include "2iren/util/camera.hpp"
#include "2iren/util/filesystem.hpp"
#include "2iren/util/time.hpp"
#include "2iren/window.hpp"
#include "bake.hpp"
#include "gui.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"
#include "skybox.hpp"
#include "timer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

namespace {

[[nodiscard]] auto create_swapchain(siren::Device* device, siren::Window& window) -> siren::Swapchain {
    return device->create_swapchain(
        {
            .label  = std::nullopt,
            .vsync  = true,
            .extent = {window.width(), window.height()},
            .window = &window,
        }
    );
}

[[nodiscard]] auto create_method(
    const oiter::MethodKind method_kind,
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& server
) -> std::unique_ptr<oiter::OitMethod> {
    if (method_kind == oiter::MethodKind::DualDepthPeeling) {
        return std::make_unique<oiter::DualDepthPeeling>(device, extent, server);
    }

    if (method_kind == oiter::MethodKind::DepthPeeling) {
        return std::make_unique<oiter::DepthPeeling>(device, extent, server);
    }

    throw std::runtime_error("oit method (" + method_kind.to_string() + ") is not supported.");
}

} // namespace

namespace oiter {

struct App::State {
    explicit State(AppConfig config)
        : scene_path{std::move(config.scene_path)},
          oit_method{config.oit_method},
          camera_position{config.camera_position} {}

    std::string scene_path;
    MethodKind oit_method;
    glm::vec3 camera_position;
    bool show_debug_menu = true;
    bool render_skybox = true;
    gui::State gui_state;
};

App::App(AppConfig config) : m_state{std::make_unique<State>(std::move(config))} {}

App::~App() = default;

auto App::run_interactive() -> void {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});

    const auto ctx = siren::Context::create(
        {
            .debug   = true,
            .level   = siren::log::Level::Debug,
            .backend = siren::Backend::Auto,
        }
    );
    auto window = ctx.create_window(
        {
            .title        = "Oiter",
            .width        = 1280,
            .height       = 720,
            .decorated    = true,
            .resizable    = true,
            .transparent  = false,
            .initial_mode = siren::WindowMode::Normal,
        }
    );

    auto device = ctx.create_device({.window = window});
    device->render_thread().spawn([&] { gui::init(window); });

    auto swapchain = create_swapchain(device.get(), window);
    siren::AssetServer server{*device};
    siren::Input input{window};

    const auto sceneh = server.load<siren::Gltf>(m_state->scene_path);
    server.wait_until_loaded(sceneh);
    auto baked = bake_scene(sceneh, server);

    auto oit_method = create_method(m_state->oit_method, *device, {window.width(), window.height()}, server);

    siren::PerspectiveCamera camera{};
    siren::PerspectiveCameraController controller;
    camera.set_position(m_state->camera_position);
    camera.look_at(glm::vec3{-1, 0, 0});

    const auto skybox = Skybox{"oiter://assets/textures/skybox/skybox.cubemap", *device, server};

    window.on_resize(
        [&](const glm::ivec2 size) {
            if (size.x == 0 || size.y == 0) {
                return;
            }
            camera.set_aspect(static_cast<siren::f32>(size.x) / static_cast<siren::f32>(size.y));
            swapchain = create_swapchain(device.get(), window);
            oit_method->resize({size.x, size.y});
        }
    );

    while (!window.should_close()) {
        m_state->gui_state.frame++;
        SetTimer full_frame_timer{m_state->gui_state.full_frame_ms};

        window.poll_events();
        if (!ImGui::GetIO().WantCaptureMouse) {
            controller.update_look(camera, input);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            controller.update_position(camera, input);
        }

        if (input.keyboard().just_pressed(siren::Key::F1)) {
            m_state->show_debug_menu = !m_state->show_debug_menu;
        }

        if (input.keyboard().just_pressed(siren::Key::F2)) {
            oit_method->reload_shaders();
        }

        if (input.keyboard().just_pressed(siren::Key::F3)) {
            swapchain = create_swapchain(device.get(), window);
        }

        if (input.keyboard().just_pressed(siren::Key::F4)) {
            m_state->render_skybox = !m_state->render_skybox;
        }

        {
            SetTimer oit_render_timer{m_state->gui_state.oit_render_ms};
            const auto& image = oit_method->render(camera, baked);
            if (m_state->render_skybox) {
                skybox.render_behind(image, camera);
            }
            device->blit(image.handle(), swapchain.next_image());
        }

        swapchain.present_overlay(
            [&] {
                if (m_state->show_debug_menu && gui::render_debug_info(
                    device->statistics(), camera, controller, oit_method.get(), m_state->oit_method, m_state->gui_state
                )) {
                    oit_method = create_method(m_state->oit_method, *device, {window.width(), window.height()}, server);
                }
            }
        );
        device->flush_delete_queue();
        siren::time::tick();
        input.update();
    }

}

auto App::run_render() -> void {}

} // namespace oiter
