#include "render.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>

#include <stb/stb_image_write.h>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/context.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"

#include "2iREN/window.hpp"
#include "scene_renderer.hpp"

namespace oiter {

struct RenderApp::Impl {
    Impl(const RenderAppOptions& options) :
        context(
            siren::Context::create({
                .debug   = true,
                .level   = options.log_level,
                .backend = siren::Backend::Auto,
            })
        ),
        window(context.create_window({
            .title        = "Oiter",
            .width        = options.dimensions.x,
            .height       = options.dimensions.y,
            .decorated    = false,
            .resizable    = false,
            .transparent  = false,
            .initial_mode = siren::WindowMode::Normal,
        })),
        device(context.create_device({.window = window})), assets(*device),
        renderer(*device, assets, options.scene_path, options.method, options.dimensions),
        output_path(options.output_path) {
        camera.set_position(options.camera_position);
        camera.lookat(options.camera_lookat);
        camera.set_aspect(
            static_cast<siren::f32>(options.dimensions.x)
            / static_cast<siren::f32>(options.dimensions.y)
        );
    }

    siren::Context context;
    siren::Window window;
    std::unique_ptr<siren::Device> device;
    siren::AssetServer assets;
    SceneRenderer renderer;
    siren::Camera camera = siren::Camera{{}};
    std::string output_path;

    auto run() -> void {
        auto& image                 = renderer.render(camera);
        const auto image_descriptor = image.descriptor();

        const auto sampler = device->create_sampler({});
        const auto output  = device->create_image({
            .label         = "Rendered Image",
            .format        = siren::ImageFormat::sRGBA8,
            .extent        = image_descriptor.extent,
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
        });

        const auto shader_handle =
            assets.load<siren::ShaderAsset>("oiter://assets/shaders/unpremultiply.sshg");
        assets.wait_until_loaded(shader_handle);

        const auto pipeline = device->create_graphics_pipeline({
            .label  = "Image Output Pipeline",
            .layout = siren::FULLSCREEN_VERTEX_LAYOUT,
            .shader = assets.get_unsafe(shader_handle).shader.handle(),
        });

        device->render_pass(
            {
                .label = "Unpremultiply and Encode sRGBA",
                .target =
                    {
                        .colors        = {{
                            .image           = output.handle(),
                            .begin_operation = siren::BeginOperation::Clear,
                            .clear_color     = siren::Rgba::ZERO(),
                        }},
                        .depth_stencil = std::nullopt,
                        .is_srgb       = true,
                    },
            },
            [&](siren::RenderPassRecorder& pass) {
                pass.bind_graphics_pipeline(pipeline.handle());
                pass.bind_sampled_image(image.handle(), sampler.handle(), 0);
                pass.draw_fullscreen();
            }
        );

        const auto pixels      = device->read_image(output.handle());
        const auto& descriptor = output.descriptor();
        std::optional<siren::Path> physical_output;
        if (output_path.find("://") != std::string::npos) {
            physical_output = siren::FileSystem::to_physical(output_path);
        } else {
            physical_output = std::filesystem::absolute(output_path);
        }

        stbi_flip_vertically_on_write(true);
        const auto result = stbi_write_png(
            physical_output->c_str(),
            static_cast<int>(descriptor.extent.x),
            static_cast<int>(descriptor.extent.y),
            4,
            pixels.data(),
            static_cast<int>(descriptor.extent.x * 4)
        );

        ASSERT(result != 0);
    }
};

RenderApp::RenderApp(const RenderAppOptions& options) {
    if (options.dimensions.x == 0 || options.dimensions.y == 0) {
        throw std::invalid_argument("Render dimensions must be greater than zero");
    }

    siren::FileSystem::mount("oiter", OITER_VFS);
    m_impl = std::make_unique<RenderApp::Impl>(options);
}

RenderApp::~RenderApp() = default;

auto RenderApp::run() -> void { m_impl->run(); }

} // namespace oiter
