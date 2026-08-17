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
#include "cli_args.hpp"
#include "gui.hpp"
#include "skybox.hpp"
#include "timer.hpp"
#include "methods/depth_peeling/depth_peeling.hpp"
#include "methods/dual_depth_peeling/dual_depth_peeling.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

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

[[nodiscard]] inline auto create_method(
    const oiter::CliArgs& cli,
    siren::Device& device,
    glm::uvec2 extent,
    siren::AssetServer& server
) -> std::unique_ptr<oiter::OitMethod> {
    if (cli.oit_method == oiter::MethodKind::DualDepthPeeling) {
        return std::make_unique<oiter::DualDepthPeeling>(device, extent, server);
    }

    if (cli.oit_method == oiter::MethodKind::DepthPeeling) {
        return std::make_unique<oiter::DepthPeeling>(device, extent, server);
    }

    throw std::runtime_error("oit method (" + cli.oit_method.to_string() + ") is not supported.");
}

auto main(const int argc, const char** argv) -> int {
    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});

    auto cli = oiter::parse_cli_args(argc, argv);

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

    // todo: maybe not pass window here? kinda sucks lmao, maybe just pass in create_swapchain?
    auto device = ctx.create_device({.window = window});
    gui::init(window); // have to init after device since it loads opengl fn ptrs

    auto swapchain = create_swapchain(device.get(), window);
    siren::AssetServer server{*device};
    siren::Input input{window};

    const auto sceneh = server.load<siren::Gltf>(cli.scene_path);
    server.wait_until_loaded(sceneh);

    auto baked = oiter::bake_scene(sceneh, server);

    auto oit_method = create_method(cli, *device, {window.width(), window.height()}, server);

    siren::PerspectiveCamera camera{};
    siren::PerspectiveCameraController controller;
    camera.set_position(cli.camera_position);
    camera.look_at(glm::vec3{-1, 0, 0});

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
    bool render_skybox = true;
    const auto skybox = oiter::Skybox{"oiter://assets/textures/skybox/skybox.cubemap", *device, server};

    while (!window.should_close()) {
        guistate.frame++;
        oiter::SetTimer s1{guistate.full_frame_ms};

        window.poll_events();
        if (!ImGui::GetIO().WantCaptureMouse) {
            controller.update_look(camera, input);
        }

        if (!ImGui::GetIO().WantCaptureKeyboard) {
            controller.update_position(camera, input);
        }

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

        if (input.keyboard().just_pressed(siren::Key::F4)) {
            render_skybox = !render_skybox;
        }

        {
            oiter::SetTimer s2{guistate.oit_render_ms};
            const auto& image = oit_method->render(camera, baked);
            if (render_skybox) {
                skybox.render_behind(image, camera);
            }
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
                        cli,
                        guistate
                    )) {
                        oit_method = create_method(cli, *device, {window.width(), window.height()}, server);
                    }
                }
            }
        );
        device->flush_delete_queue();
        siren::time::tick();
        input.update();
    }
}
