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
#include "methods/dual_depth_peeling.hpp"

#ifndef OITER_VFS
#define OITER_VFS "."
#endif

struct UniformBuffer {
    glm::mat4 projection_view;
};

auto main(const int argc, const char** argv) -> int {
    const auto config = oiter::parse_cli_args(argc, argv);

    siren::FileSystem::mount("oiter", siren::Path{ OITER_VFS });

    siren::Context ctx{ { .debug = true, .level = siren::log::Level::Trace, .backend = siren::Backend::OpenGL } };
    siren::Window window{ {
            .title        = "Oiter",
            .width        = 1280,
            .height       = 720,
            .vsync        = true,
            .decorated    = true,
            .resizable    = true,
            .transparent  = false,
            .initial_mode = siren::WindowMode::Normal,
    } };
    auto device = ctx.create_device(window);
    oiter::init_imgui(window);  // have to init after device since it loads opengl fn ptrs

    auto swapchain = device->create_swapchain({
            .label = std::nullopt,
            .vsync = true,
    });
    siren::AssetServer server{ *device };
    siren::Input input{ window };

    const auto scene = server.load<siren::Gltf>(config.scene_path);
    while (!server.is_loaded_with_dependencies(scene)) {
        /** wait until loaded */
    }

    auto baked = oiter::bake_scene(scene, server);

    std::unique_ptr<oiter::OitMethod> oit = nullptr;

    if (config.oit_method == oiter::methods::DUAL_DEPTH_PEELING) {
        oit = std::make_unique<oiter::DualDepthPeeling>(*device, window, server);
    } else {
        std::cerr << "OIT method: (" << config.oit_method << ") method not yet supported." << std::endl;
        std::exit(1);
    }

    siren::PerspectiveCamera camera;
    camera.set_position(-glm::vec3{ 0.f, -3.f, -2.f });

    const glm::vec3 target = glm::vec3{ 0.f };
    glm::vec3 dir          = glm::normalize(target - camera.position());

    camera.set_yaw(std::atan2(dir.x, dir.z));
    camera.set_pitch(std::asin(dir.y));

    siren::PerspectiveCameraController controller;

    while (!window.should_close()) {
        window.poll_events();
        controller.update(camera, input);
        input.update();

        const auto& image = oit->render(camera, baked);
        device->blit(image.handle(), swapchain.next_image());
        swapchain.present_overlay([&] { oiter::render_debug_info(device->statistics(), camera); });
        device->flush_delete_queue();
        siren::time::tick();
    }

    device->wait_until_idle();
    std::exit(0);
}
