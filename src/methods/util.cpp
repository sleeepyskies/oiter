#include "util.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {
auto GraphicsPipelineResources::create(
    siren::Device& device,
    siren::AssetServer& server,
    const std::string& path,
    siren::GraphicsPipelineDescriptor& descriptor
) -> GraphicsPipelineResources {
    const auto shaderh = server.load<siren::ShaderAsset>(path);
    descriptor.shader = server.get_unsafe(shaderh).shader.handle();
    return GraphicsPipelineResources{
        .shader            = shaderh,
        .graphics_pipeline = device.create_graphics_pipeline(descriptor),
    };
}

auto RenderTargetBuilder::create(
    siren::Device& device,
    const glm::uvec2& extent,
    const std::optional<std::string>& label
) -> RenderTargetBuilder {
    return RenderTargetBuilder{device, extent, label};
}

auto RenderTargetBuilder::add_color(
    const siren::ImageFormat format,
    const siren::BeginOperation begin_op,
    const siren::Rgba clear
) -> RenderTargetBuilder& {
    auto image = m_device.create_image(
        {
            .label = m_label.transform(
                [this](auto l) -> std::string {
                    return std::format("{}_color_{}", l, m_colors.size() + 1);
                }
            ),
            .format        = format,
            .extent        = siren::ImageExtent{m_extent.x, m_extent.y},
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
        }
    );

    m_color_attachments.push_back(
        siren::ColorAttachment{
            .image           = image.handle(),
            .begin_operation = begin_op,
            .clear_color     = clear,
        }
    );

    m_colors.push_back(std::move(image));

    return *this;
}

auto RenderTargetBuilder::add_depth_stencil(
    const siren::ImageFormat format,
    const siren::BeginOperation begin_op,
    const siren::f32 clear_depth,
    const siren::u8 clear_stencil
) -> RenderTargetBuilder& {
    m_depth = m_device.create_image(
        {
            .label = m_label.transform(
                [this](auto l) -> std::string {
                    return std::format("{}_depth_stencil_{}", l, m_colors.size() + 1);
                }
            ),
            .format        = format,
            .extent        = siren::ImageExtent{m_extent.x, m_extent.y},
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
        }
    );

    m_depth_attachment = siren::DepthStencilAttachment{
        .image           = m_depth->handle(),
        .begin_operation = begin_op,
        .clear_depth     = clear_depth,
        .clear_stencil   = clear_stencil,
    };

    return *this;
}

auto RenderTargetBuilder::build() -> RenderTargetResources {
    return RenderTargetResources{
        siren::RenderTarget{
            .colors        = std::move(m_color_attachments),
            .depth_stencil = m_depth_attachment,
        },
        std::move(m_colors),
        std::move(m_depth),
    };
}
} // namespace oiter
