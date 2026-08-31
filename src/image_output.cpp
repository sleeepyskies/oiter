#include "image_output.hpp"

#include <optional>
#include <stdexcept>

#include <stb/stb_image_write.h>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/asset/shader.hpp"
#include "2iREN/core/assert.hpp"
#include "2iREN/graphics/device.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/layout.hpp"
#include "2iREN/graphics/sampler.hpp"
#include "2iREN/utility/filesystem.hpp"

namespace oiter {
auto write_rendered_image(
    siren::Device& device,
    siren::AssetServer& assets,
    const siren::Image& source,
    const std::string& output_path
) -> void {
    const auto& source_descriptor = source.descriptor();
    ASSERT(source_descriptor.format == siren::ImageFormat::RGBA8, "Output must be RGBA8 format.");

    const auto sampler = device.create_sampler({});
    const auto output  = device.create_image({
        .label         = "Rendered Image",
        .format        = siren::ImageFormat::sRGBA8,
        .extent        = source_descriptor.extent,
        .dimension     = siren::ImageDimension::D2,
        .mipmap_levels = 1,
    });

    const auto shader_handle =
        assets.load<siren::ShaderAsset>("oiter://assets/shaders/unpremultiply.sshg");
    assets.wait_until_loaded(shader_handle);

    const auto pipeline = device.create_graphics_pipeline({
        .label  = "Image Output Pipeline",
        .layout = siren::FULLSCREEN_VERTEX_LAYOUT,
        .shader = assets.get_unsafe(shader_handle).shader.handle(),
    });

    device.render_pass(
        {
            .label = "Unpremultiply and Encode sRGB",
            .target =
                {
                    .colors        = {{
                        .image           = output.handle(),
                        .begin_operation = siren::BeginOperation::Clear,
                        .clear_color     = siren::Rgba::ZERO,
                    }},
                    .depth_stencil = std::nullopt,
                    .is_srgb       = true,
                },
        },
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(pipeline.handle());
            pass.bind_sampled_image(source.handle(), sampler.handle(), 0);
            pass.draw_fullscreen();
        }
    );

    const auto pixels          = device.read_image(output.handle());
    const auto& descriptor     = output.descriptor();
    const auto physical_output = siren::FileSystem::to_physical(siren::Path{output_path});

    if (!physical_output) {
        throw std::runtime_error("Could not resolve output path: " + output_path);
    }

    stbi_flip_vertically_on_write(true);
    const auto result = stbi_write_png(
        physical_output->c_str(),
        static_cast<int>(descriptor.extent.width),
        static_cast<int>(descriptor.extent.height),
        4,
        pixels.data(),
        static_cast<int>(descriptor.extent.width * 4)
    );

    if (result == 0) {
        throw std::runtime_error("failed to write png: " + physical_output->string());
    }
}
} // namespace oiter
