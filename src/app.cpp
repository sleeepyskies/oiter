#include "app.hpp"

#include <stb/stb_image_write.h>

#include "2iREN/asset/gltf.hpp"
#include "2iREN/context.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/swapchain.hpp"
#include "2iREN/input/input.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"
#include "2iREN/utility/time.hpp"
#include "2iREN/window.hpp"

#include "bake.hpp"
#include "gui.hpp"
#include "skybox.hpp"
#include "timer.hpp"

#include "methods/a_buffer/a_buffer.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"
#include "methods/k_buffer/k_buffer.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

[[nodiscard]] static auto create_swapchain(siren::Device* device, siren::Window& window)
    -> siren::Swapchain {
    return device->create_swapchain({
        .label  = std::nullopt,
        .vsync  = true,
        .extent = {window.width(), window.height()},
        .window = &window,
    });
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
InteractiveApp::InteractiveApp(const InteractiveAppOptions& options) :
    m_options(options), m_interactive_state{
                            .oit_method      = options.method,
                            .camera_position = options.camera_position,
                        } {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});
}

auto InteractiveApp::run() -> void {
    const auto ctx = siren::Context::create({
        .debug   = true,
        .level   = m_options.log_level,
        .backend = siren::Backend::Auto,
    });
    auto window    = ctx.create_window({
        .title        = "Oiter",
        .width        = 1280,
        .height       = 720,
        .decorated    = true,
        .resizable    = true,
        .transparent  = false,
        .initial_mode = siren::WindowMode::Normal,
    });

    auto device = ctx.create_device({.window = window});
    device->render_thread().spawn([&] { gui::init(window); });

    auto swapchain = create_swapchain(device.get(), window);
    siren::AssetServer server{*device};
    siren::Input input{window};

    const auto sceneh = server.load<siren::Gltf>(m_options.scene_path);
    server.wait_until_loaded(sceneh);
    auto baked = bake_scene(sceneh, server);

    auto oit_method = create_method(
        m_interactive_state.oit_method, *device, {window.width(), window.height()}, server
    );

    siren::PerspectiveCamera camera{};
    siren::PerspectiveCameraController controller;
    camera.set_position(m_interactive_state.camera_position);
    camera.look_at(m_options.camera_lookat);

    const auto skybox = Skybox{"oiter://assets/textures/skybox/skybox.cubemap", *device, server};

    window.on_resize([&](const glm::ivec2 size) {
        if (size.x == 0 || size.y == 0) {
            return;
        }
        camera.set_aspect(static_cast<siren::f32>(size.x) / static_cast<siren::f32>(size.y));
        swapchain = create_swapchain(device.get(), window);
        oit_method->resize({size.x, size.y});
    });

    while (!window.should_close()) {
        m_frame_stats.frame++;
        if (!(m_frame_stats.frame % 60)) {
            m_frame_stats.fps = 1.f / static_cast<siren::f32>(siren::time::delta().seconds());
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

        swapchain.present_overlay([&] {
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
        });
        device->flush_delete_queue();
        siren::time::step();
        input.update();
        m_interactive_state.camera_position = camera.position();
    }
}

RenderApp::RenderApp(const RenderAppOptions& options) : m_options(options) {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});
}

auto RenderApp::run() -> void {
    const auto ctx = siren::Context::create({
        .debug   = true,
        .level   = m_options.log_level,
        .backend = siren::Backend::Auto,
    });
    auto window    = ctx.create_window({
        .title        = "Oiter",
        .width        = m_options.dimensions.x,
        .height       = m_options.dimensions.y,
        .decorated    = false,
        .resizable    = false,
        .transparent  = false,
        .initial_mode = siren::WindowMode::Normal,
    });
    auto device    = ctx.create_device({.window = window});
    siren::AssetServer server{*device};

    const auto sceneh = server.load<siren::Gltf>(m_options.scene_path);
    server.wait_until_loaded(sceneh);
    const auto baked = bake_scene(sceneh, server);

    const auto oit_method =
        create_method(m_options.method, *device, {window.width(), window.height()}, server);

    auto camera = siren::PerspectiveCamera{};
    camera.set_position(m_options.camera_position);
    camera.look_at(m_options.camera_lookat);

    const auto& rendered_image           = oit_method->render(camera, baked);
    const auto rendered_image_descriptor = rendered_image.descriptor();

    ASSERT(
        rendered_image_descriptor.format == siren::ImageFormat::RGBA8,
        "Render output must be using RGBA8 color space."
    );

    // convert linear to srgb color space
    const auto sampler     = device->create_sampler({});
    const auto final_image = device->create_image({
        .label         = "Final Image",
        .format        = siren::ImageFormat::sRGBA8,
        .extent        = siren::ImageExtent{window.width(), window.height()},
        .dimension     = siren::ImageDimension::D2,
        .mipmap_levels = 1,
    });
    const auto unpremultiply_shaderh =
        server.load<siren::ShaderAsset>("oiter://assets/shaders/unpremultiply.sshg");
    server.wait_until_loaded(unpremultiply_shaderh);
    const auto unpremultiply_pipeline = device->create_graphics_pipeline({
        .layout = siren::FULLSCREEN_VERTEX_LAYOUT,
        .shader = server.get(unpremultiply_shaderh)->shader.handle(),
    });
    device->render_pass(
        siren::RenderPassDescriptor{
            .label = "Linear to sRGB",
            .target =
                siren::RenderTarget{
                    .colors        = {siren::ColorAttachment{
                        .image           = final_image.handle(),
                        .begin_operation = siren::BeginOperation::Clear,
                        .clear_color     = siren::Rgba::ZERO,
                    }},
                    .depth_stencil = std::nullopt,
                    .is_srgb       = true,
                },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(unpremultiply_pipeline.handle());
            pass.bind_sampled_image(rendered_image.handle(), sampler.handle(), 0);
            pass.draw_fullscreen();
        }
    );

    const auto buffer = device->read_image(final_image.handle());

    stbi_flip_vertically_on_write(true);
    const auto result = stbi_write_png(
        siren::FileSystem::to_physical(m_options.output_path)->c_str(),
        rendered_image_descriptor.extent.width,
        rendered_image_descriptor.extent.height,
        4,
        buffer.data(),
        rendered_image_descriptor.extent.width * 4
    );

    ASSERT(result != 0, "stbi_write_png failed");
}
} // namespace oiter
