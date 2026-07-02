#include "2iren/asset/assets/gltf.hpp"
#include "2iren/rhi/context.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/window.hpp"
#include "methods/dual_depth_peeling.hpp"
#include "render_params.hpp"
#include "util/config.hpp"
#include "util/load_scene.hpp"

auto main(const int argc, const char** argv) -> int {
    const auto config = oiter::load_cli_config(argc, argv);

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
    auto device = ctx.create_device(window);
    siren::AssetServer server{ *device };

    oiter::RenderParams params{
        .device = std::move(device),
        .scene  = oiter::load_scene(config.scene_path, server),
    };

    if (config.oit_method == "ddp") {
        oiter::DualDepthPeeling ddp;
        ddp.render(params);
    } else {
        std::cerr << "OIT method not yet supported." << std::endl;
        return 1;
    }

    return 0;
}
