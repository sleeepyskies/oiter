#include "2iren/asset/assets/gltf.hpp"
#include "2iren/rhi/context.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/rhi/resources/swapchain.hpp"
#include "2iren/util/filesystem.hpp"
#include "2iren/window.hpp"
#include "bake.hpp"
#include "config.hpp"
#include "methods/dual_depth_peeling.hpp"

#ifndef  OITER_VFS
#define OITER_VFS "."
#endif


auto main(const int argc, const char** argv) -> int {
    const auto config = oiter::parse_cli_args(argc, argv);

    siren::FileSystem::mount("oiter", siren::Path{ OITER_VFS });

    siren::Context ctx{ { .debug = true, .level = siren::log::Level::Trace, .backend = siren::Backend::OpenGL } };
    siren::Window window{ {
            .title       = "Oiter",
            .width       = 1280,
            .height      = 720,
            .fullscreen  = false,
            .vsync       = true,
            .decorated   = true,
            .resizable   = true,
            .transparent = false,
    } };
    auto device    = ctx.create_device(window);
    auto swapchain = device->create_swapchain({
            .label = std::nullopt,
            .vsync = true,
    });
    siren::AssetServer server{ *device };

    auto scene = server.load<siren::Gltf>(config.scene_path);
    while (!server.is_loaded_with_dependencies(scene)) {
        /** wait until loaded */
    }

    auto baked = oiter::bake_scene(scene, server);

    std::unique_ptr<oiter::OitMethod> oit = nullptr;

    if (config.oit_method == "ddp") {
        oit = std::make_unique<oiter::DualDepthPeeling>(*device, window, server);
    } else {
        std::cerr << "OIT method not yet supported." << std::endl;
        std::exit(1);
    }

    while (!window.should_close()) {
        window.poll_events();

        const auto& image = oit->render(baked);
        device->blit(image.handle(), swapchain.next_image().handle());
        swapchain.present();
        device->flush_delete_queue();
    }

    device->wait_until_idle();
    std::exit(0);
}
