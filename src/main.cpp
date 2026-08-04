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
#include "config.hpp"
#include "gui.hpp"
#include "timer.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

[[nodiscard]] auto create_method(
    const oiter::Config& config,
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& server
) -> std::unique_ptr<oiter::OitMethod> {
    if (config.oit_method == oiter::MethodKind::DualDepthPeeling) {
        return std::make_unique<oiter::DualDepthPeeling>(device, extent, server);
    }

    if (config.oit_method == oiter::MethodKind::DepthPeeling) {
        return std::make_unique<oiter::DepthPeeling>(device, extent, server);
    }

    throw std::runtime_error("oit method (" + config.oit_method.to_string() + ") is not supported.");
}

[[nodiscard]] auto create_swapchain(siren::Device* device, siren::Window& window) -> siren::Swapchain {
    return device->create_swapchain(
        {
            .label  = std::nullopt,
            .vsync  = false,
            .extent = {window.width(), window.height()},
            .window = &window,
        }
    );
}

auto main(const int argc, const char** argv) -> int {
    auto config = oiter::parse_cli_args(argc, argv);

    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});

    const auto ctx = siren::Context::create(
        {
            .debug   = true,
            .level   = siren::log::Level::Trace,
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

    // todo: maybe not pass window here? kinda sucks lmao, maybe just pass in create_swapchain?
    auto device = ctx.create_device({.window = window});
    gui::init(window); // have to init after device since it loads opengl fn ptrs

    auto swapchain = create_swapchain(device.get(), window);
    siren::AssetServer server{*device};
    siren::Input input{window};

    const auto sceneh = server.load<siren::Gltf>(config.scene_path);
    while (!server.is_loaded_with_dependencies(sceneh)) {
        /** wait until loaded */ // todo: this eats cpu lol
    }

    auto baked = oiter::bake_scene(sceneh, server, 0.5f);

    auto oit_method = create_method(config, *device, {window.width(), window.height()}, server);

    siren::PerspectiveCamera camera{};
    siren::PerspectiveCameraController controller;
    camera.set_position(glm::vec3{0.f, 3.f, 2.f});
    camera.look_at(glm::vec3{0.f});

    bool show_debug_menu = true;

    window.on_resize(
        [&](const glm::ivec2 size) {
            if (size.x == 0 || size.y == 0) {
                return;
            }
            camera.set_aspect(static_cast<float>(size.x) / size.y);
            swapchain = create_swapchain(device.get(), window);
            oit_method->resize({size.x, size.y});
        }
    );

    gui::State guistate{};

    while (!window.should_close()) {
        guistate.frame++;
        oiter::SetTimer _1{guistate.full_frame_ms};

        window.poll_events();
        controller.update(camera, input);

        if (input.keyboard().just_pressed(siren::Key::F1)) {
            show_debug_menu = !show_debug_menu;
        }

        if (input.keyboard().just_pressed(siren::Key::F2)) {
            oit_method->reload_shaders();
        }

        if (input.keyboard().just_pressed(siren::Key::F3)) {
            // todo: toggle vsync mode
            swapchain = create_swapchain(device.get(), window);
        }

        {
            oiter::SetTimer _2{guistate.oit_render_ms};
            const auto& image = oit_method->render(camera, baked);
            device->blit(image.handle(), swapchain.next_image());
        }

        swapchain.present_overlay(
            [&] {
                if (show_debug_menu) {
                    if (gui::render_debug_info(
                        device->statistics(),
                        camera,
                        controller,
                        oit_method.get(),
                        config,
                        guistate
                    )) {
                        oit_method = create_method(config, *device, {window.width(), window.height()}, server);
                    }
                }
            }
        );
        device->flush_delete_queue();
        siren::time::tick();
        input.update();
    }

    device->wait_until_idle();
    return 0;
}
