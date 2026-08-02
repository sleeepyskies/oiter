#include "depth_peeling.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {
DepthPeeling::DepthPeeling(
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) : m_device(device),
    m_assets(assets) {
    create_images(extent);
    create_render_targets();
    create_pipelines();
}

auto DepthPeeling::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& { PANIC(); }

auto DepthPeeling::resize(const glm::uvec2 extent) -> void {
    create_images(extent);
    create_render_targets();
}

auto DepthPeeling::render_debug_info() -> void {
    PANIC();
}

auto DepthPeeling::create_images(const glm::uvec2 extent) -> void {
    // create color images
    for (const auto i : siren::range(LAYER_COUNT)) {
        m_colors[i] = std::make_unique<siren::Image>(
            m_device.create_image(
                {
                    .label         = std::format("Depth Peeling Color {}", i),
                    .format        = siren::ImageFormat::RGBA8,
                    .extent        = {.width = extent.x, .height = extent.y},
                    .dimension     = siren::ImageDimension::D2,
                    .mipmap_levels = 1,
                }
            )
        );
    }

    // create depth images
    for (const auto i : siren::range(LAYER_COUNT)) {
        m_depths[i] = std::make_unique<siren::Image>(
            m_device.create_image(
                {
                    .label         = std::format("Depth Peeling Depth {}", i),
                    .format        = siren::ImageFormat::RGBA8,
                    .extent        = {.width = extent.x, .height = extent.y},
                    .dimension     = siren::ImageDimension::D2,
                    .mipmap_levels = 1,
                }
            )
        );
    }
}

auto DepthPeeling::create_render_targets() -> void {
    // create render targets
    // we create DepthPeeling::LAYER_COUNT targets. each target has a unique color attachment,
    // but the depth attachments are cycled between only DepthPeeling::DEPTH_COUNT
    for (const auto i : siren::range(LAYER_COUNT)) {
        const auto color = siren::ColorAttachment{
            .image           = m_colors[i]->handle(),
            .begin_operation = siren::BeginOperation::Clear,
            .clear_color     = siren::Rgba::zero(),
        };
        const auto depth = siren::DepthStencilAttachment{
            .image           = m_depths[i % DEPTH_COUNT]->handle(),
            .begin_operation = siren::BeginOperation::Clear,
            .clear_depth     = 1,
            .clear_stencil   = 0,
        };
        m_targets[i] = siren::RenderTarget{
            .colors        = {color},
            .depth_stencil = depth,
        };
    }
}

auto DepthPeeling::create_pipelines() -> void {
    // create our graphics pipelines
    {
        m_gather_first_shader = m_assets.load<siren::ShaderAsset>("oiter://shaders/depth_peeling/gather_first.sshg");
        const auto shader     = m_assets.get_unsafe(m_gather_first_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Depth Peeling Gather First",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Greater,
                    .back_face_culling = false,
                    .depth_test        = true,
                    .depth_write       = true,
                }
            )
        );
    }

    {
        m_gather_shader   = m_assets.load<siren::ShaderAsset>("oiter://shaders/depth_peeling/gather.sshg");
        const auto shader = m_assets.get_unsafe(m_gather_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Depth Peeling Gather",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Greater,
                    .back_face_culling = false,
                    .depth_test        = true,
                    .depth_write       = true,
                }
            )
        );
    }

    {
        m_combine_shader  = m_assets.load<siren::ShaderAsset>("oiter://shaders/depth_peeling/combine.sshg");
        const auto shader = m_assets.get_unsafe(m_combine_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label       = "Depth Peeling Combine",
                    .layout      = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader      = shader,
                    .topology    = siren::PrimitiveTopology::Triangles,
                    .alpha_mode  = siren::AlphaMode::Blend,
                    .color_blend = {
                        .function      = siren::BlendFunction::Add,
                        .source_factor = siren::BlendFactor::SourceAlpha,
                        .dest_factor   = siren::BlendFactor::OneMinusSourceAlpha
                    },
                    .alpha_blend = {
                        .function      = siren::BlendFunction::Add,
                        .source_factor = siren::BlendFactor::One,
                        .dest_factor   = siren::BlendFactor::Zero,
                    },
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }

    {
        m_background_shader = m_assets.load<siren::ShaderAsset>("oiter://shaders/depth_peeling/background.sshg");
        const auto shader   = m_assets.get_unsafe(m_background_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Depth Peeling Background",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Greater,
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }
}
} // namespace oiter
