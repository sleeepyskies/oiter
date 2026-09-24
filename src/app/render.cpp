#include "render.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>

#include <stb_image_write.h>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/core/context.hpp"
#include "2iREN/graphics/commands.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/scene/camera.hpp"
#include "2iREN/utility/filesystem.hpp"
#include "2iREN/window/window.hpp"

#include "scene_renderer.hpp"

using namespace siren;

namespace oiter {

struct RenderApp::Impl {
    Impl(const RenderAppOptions& options) :
        context(Context::make({.level = options.log_level})),
        window(context.make_window({
            .title       = "Oiter",
            .width       = options.dimensions.x,
            .height      = options.dimensions.y,
            .decorated   = false,
            .resizable   = false,
            .transparent = false,
            .mode        = WindowMode::Normal,
        })),
        device(context.make_device()),
        assets(*device),
        renderer(*device, assets, options.scene_path, options.method, options.dimensions),
        output_path(options.output_path) {
        camera.set_position(options.camera_position);
        camera.lookat(options.camera_lookat);
        camera.set_aspect(
            static_cast<f32>(options.dimensions.x) / static_cast<f32>(options.dimensions.y)
        );
    }

    Context context;
    Window window;
    std::unique_ptr<Device> device;
    AssetServer assets;
    SceneRenderer renderer;
    Camera camera = Camera{{}};
    std::string output_path;

    auto run() -> void {
        const auto output_descriptor = ImageDescriptor{
            .label  = "Rendered Image",
            .format = ImageFormat::sRGBA8,
            .extent = window.extent().to_extent3(),
            .flags  = ImageFlags::make(ImageFlag::RenderAttachment),
        };
        const auto output = device->make_image(output_descriptor);

        // render image
        {
            auto cmds = device->make_command_buffer();
            renderer.render(*cmds, output.handle(), camera);
            device->submit(std::move(cmds));
        }

        const auto shader_handle = assets.load<ShaderAsset>(
            "oiter://assets/shaders/unpremultiply.sshg"
        );
        assets.wait_until_loaded(shader_handle);

        auto cmds           = device->make_command_buffer();
        const auto sampler  = device->make_sampler({});
        const auto pipeline = device->make_graphics_pipeline({
            .label     = "Image Output Pipeline",
            .shader    = assets.get_unsafe(shader_handle).shader.handle(),
            .layout    = FULLSCREEN_VERTEX_LAYOUT,
            .cull_mode = CullMode::None,
        });

        cmds->render_pass(
            {
                .target =
                    RenderTarget{
                        .colors =
                            TargetColorAttachments{
                                TargetColorAttachment{.image = output.handle()},
                            },
                    },
            },
            [&](RenderCommandEncoder& pass) {
                pass.bind_graphics_pipeline(pipeline.handle());
                pass.bind_sampler(sampler.handle(), Slot{0});
                pass.bind_image(output.handle(), Slot{0});
                pass.draw(3);
            }
        );

        const auto img_size = output_descriptor.extent.to_extent2().area()
            * output_descriptor.format.size_bytes();
        auto staging = device->make_buffer({.size = img_size});
        cmds->copy_image_to_buffer(output.handle(), staging.handle(), 0);

        device->submit(std::move(cmds));

        const auto pixels = device->read_buffer(staging.handle());

        const auto& descriptor = output.descriptor();
        std::optional<Path> physical_output;
        if (output_path.find("://") != std::string::npos) {
            physical_output = FileSystem::to_physical(output_path);
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

    FileSystem::mount("oiter", OITER_VFS);
    m_impl = std::make_unique<RenderApp::Impl>(options);
}

RenderApp::~RenderApp() = default;

auto RenderApp::run() -> void {
    m_impl->run();
}

} // namespace oiter
