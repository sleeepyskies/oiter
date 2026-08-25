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

#include <utility>

#include "2iren/util/stb_image_write.h"
#include "methods/a_buffer/a_buffer.hpp"
#include "methods/k_buffer/k_buffer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

[[nodiscard]] static auto create_swapchain(siren::Device* device, siren::Window& window) -> siren::Swapchain {
    return device->create_swapchain(
        {
            .label  = std::nullopt,
            .vsync  = true,
            .extent = {window.width(), window.height()},
            .window = &window,
        }
    );
}

[[nodiscard]] static auto create_method(
    const oiter::MethodKind method_kind,
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& server
) -> std::unique_ptr<oiter::OitMethod> {
    // clang-format off
    switch (method_kind) {
        case oiter::MethodKind::DepthPeeling: return std::make_unique<oiter::DepthPeeling>(device, extent, server);
        case oiter::MethodKind::DualDepthPeeling: return std::make_unique<oiter::DualDepthPeeling>(device, extent, server);
        case oiter::MethodKind::ABuffer: return std::make_unique<oiter::ABuffer>(device, extent, server);
        case oiter::MethodKind::KBuffer: return std::make_unique<oiter::KBuffer>(device, extent, server);
        default:  throw std::runtime_error("oit method (" + method_kind.to_string() + ") is not supported.");
    }
    // clang-format on
}

namespace oiter {
InteractiveApp::InteractiveApp(const InteractiveAppOptions& options)
    : m_options(options),
      m_interactive_state{
          .oit_method      = options.method,
          .camera_position = options.camera_position,
      } {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});
}

auto InteractiveApp::run() -> void {
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

    const auto sceneh = server.load<siren::Gltf>(m_options.scene_path);
    server.wait_until_loaded(sceneh);
    auto baked = bake_scene(sceneh, server);

    auto oit_method = create_method(m_interactive_state.oit_method, *device, {window.width(), window.height()}, server);

    siren::PerspectiveCamera camera{};
    siren::PerspectiveCameraController controller;
    camera.set_position(m_interactive_state.camera_position);
    camera.look_at(m_options.camera_lookat);

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
        m_frame_stats.frame++;
        if (!(m_frame_stats.frame % 60)) {
            m_frame_stats.fps = 1 / siren::time::delta_s();
        }
        SetTimer full_frame_timer{m_frame_stats.full_frame_ms};

        window.poll_events();
        if (!ImGui::GetIO().WantCaptureMouse) {
            controller.update_look(camera, input);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            controller.update_position(camera, input);
        }

        if (input.keyboard().just_pressed(siren::Key::F1)) {
            m_interactive_state.debug_menu_visible = !m_interactive_state.debug_menu_visible;
        }

        if (input.keyboard().just_pressed(siren::Key::F2)) {
            oit_method->reload_shaders();
        }

        if (input.keyboard().just_pressed(siren::Key::F3)) {
            m_interactive_state.skybox_visible = !m_interactive_state.skybox_visible;
        }

        {
            SetTimer oit_render_timer{m_frame_stats.oit_render_ms};
            const auto& image = oit_method->render(camera, baked);
            if (m_interactive_state.skybox_visible) {
                skybox.render_behind(image, camera);
            }
            device->blit_image(image.handle(), swapchain.next_image());
        }

        swapchain.present_overlay(
            [&] {
                if (!m_interactive_state.debug_menu_visible) {
                    return;
                }

                const auto actions = gui::render_debug(
                    device->statistics(),
                    camera,
                    controller,
                    *oit_method,
                    m_interactive_state.oit_method,
                    m_frame_stats
                );

                if (actions.oit_method) {
                    m_interactive_state.oit_method = *actions.oit_method;
                    oit_method                     = create_method(
                        m_interactive_state.oit_method,
                        *device,
                        {window.width(), window.height()},
                        server
                    );
                }
            }
        );
        device->flush_delete_queue();
        siren::time::tick();
        input.update();
        m_interactive_state.camera_position = camera.position();
    }
}

RenderApp::RenderApp(const RenderAppOptions& options) : m_options(options) {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});
}

auto RenderApp::run() -> void {
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
            .width        = m_options.dimensions.x,
            .height       = m_options.dimensions.y,
            .decorated    = false,
            .resizable    = false,
            .transparent  = false,
            .initial_mode = siren::WindowMode::Normal,
        }
    );

    auto device = ctx.create_device({.window = window});
    siren::AssetServer server{*device};

    const auto sceneh = server.load<siren::Gltf>(m_options.scene_path);
    server.wait_until_loaded(sceneh);
    auto baked = bake_scene(sceneh, server);

    auto oit_method = create_method(m_options.method, *device, {window.width(), window.height()}, server);

    siren::PerspectiveCamera camera{};
    camera.set_position(m_options.camera_position);
    camera.look_at(m_options.camera_lookat);

    const auto& image = oit_method->render(camera, baked);

    const auto& desc  = image.descriptor();
    if (desc.format != siren::ImageFormat::RGBA8 && desc.format != siren::ImageFormat::sRGBA8) {
        throw std::runtime_error{"Rendered image must be RGBA8"};
    }

    const auto buffer = device->read_image(image.handle());

    stbi_flip_vertically_on_write(true);
    const auto result = stbi_write_png(
        siren::FileSystem::to_physical(m_options.output_path)->c_str(),
        desc.extent.width,
        desc.extent.height,
        4,
        buffer.data(),
        desc.extent.width * 4
    );

    if (result == 0) {
        throw std::runtime_error("stb_image_write has failed.");
    }
}
} // namespace oiter
