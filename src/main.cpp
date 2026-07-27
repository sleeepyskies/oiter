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
#include "imgui.hpp"
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
    if (config.oit_method == oiter::methods::DUAL_DEPTH_PEELING) {
        return std::make_unique<oiter::DualDepthPeeling>(device, extent, server);
    }
    throw std::runtime_error("Oit method (" + config.oit_method + ") is not supported.");
}

[[nodiscard]] auto create_swapchain(siren::Device* device, siren::Window& window) -> siren::Swapchain {
    return device->create_swapchain({
        .label = std::nullopt,
        .vsync = true,
        .extent = {window.width(), window.height()},
        .window = &window,
    });
}

auto main(const int argc, const char** argv) -> int {
    const auto config = oiter::parse_cli_args(argc, argv);

    siren::FileSystem::mount("oiter", siren::Path{OITER_VFS});

    const auto ctx = siren::Context::create({
        .debug = true,
        .level = siren::log::Level::Trace,
        .backend = siren::Backend::Auto,
    });
    auto window = ctx.create_window({
        .title = "Oiter",
        .width = 1280,
        .height = 720,
        .decorated = true,
        .resizable = true,
        .transparent = false,
        .initial_mode = siren::WindowMode::Normal,
    });

    // todo: maybe not pass window here? kinda sucks lmao, maybe just pass in create_swapchain?
    auto device = ctx.create_device({
        .window = window,
    });
    oiter::init_imgui(window); // have to init after device since it loads opengl fn ptrs

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

    bool show_debug_menu = false;

    window.on_resize([&](const glm::ivec2 size) {
        if (size.x == 0 || size.y == 0) {
            return;
        }
        camera.set_aspect(static_cast<float>(size.x) / size.y);
        swapchain = create_swapchain(device.get(), window);
        oit_method->resize({size.x, size.y});
    });

    while (!window.should_close()) {
        window.poll_events();
        controller.update(camera, input);
        if (input.keyboard().just_pressed(siren::Key::F1)) {
            show_debug_menu = !show_debug_menu;
        }

        const auto& image = oit_method->render(camera, baked);
        device->blit(image.handle(), swapchain.next_image());
        swapchain.present_overlay([&] {
            if (show_debug_menu) {
                oiter::render_debug_info(device->statistics(), camera, controller, oit_method.get());
            }
        });
        device->flush_delete_queue();
        siren::time::tick();
        input.update();
    }

    device->wait_until_idle();
    return 0;
}
