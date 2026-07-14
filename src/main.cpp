#include "2iren/asset/assets/gltf.hpp"
#include "2iren/rhi/context.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/resources/swapchain.hpp"
#include "2iren/util/camera.hpp"
#include "2iren/util/filesystem.hpp"
#include "2iren/util/time.hpp"
#include "2iren/window.hpp"
#include "bake.hpp"
#include "config.hpp"
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
    auto device    = ctx.create_device(window);
    auto swapchain = device->create_swapchain({
            .label = std::nullopt,
            .vsync = true,
    });
    siren::AssetServer server{ *device };

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
    siren::PerspectiveCameraController controller;

    siren::usize interval = 0;

    while (!window.should_close()) {
        if (!(interval % 60)) {
            window.set_title(std::format("Oiter: {:.5f}ms | {}fps", siren::time::delta_ms(), 1 / siren::time::delta_s()));
        }
        window.poll_events();

        controller.update(camera);

        const auto& image = oit->render(camera, baked);
        device->blit(image.handle(), swapchain.next_image());
        swapchain.present();
        device->flush_delete_queue();
        siren::time::tick();
        interval++;
    }

    device->wait_until_idle();
    std::exit(0);
}
