#include "depth_peeling.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {
DepthPeeling::DepthPeeling(
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    create_images(extent);
    create_sampler();
    create_render_targets();
    create_pipelines();
}

auto DepthPeeling::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    update_buffers(camera, scene);

    const auto buffer_alignment = siren::align_up(
        sizeof(MeshUniforms),
        m_device.limits().uniform_buffer_offset_alignment
    );

    auto draw_scene = [&](siren::RenderPassRecorder& pass) {
        for (const auto& [index, surface] : std::views::enumerate(scene.transparent)) {
            pass.bind_uniform_buffer_range(
                m_mesh_buffer->handle(),
                0,
                buffer_alignment * (scene.opaque.size() + index),
                sizeof(MeshUniforms)
            );
            pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
            pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
            pass.draw_indexed(surface.index.count, 0);
        }
    };

    // one render pass per layer we peel
    for (const auto& [index, target] : std::views::enumerate(m_targets)) {
        m_device.render_pass(
            {.target = target},
            [&](siren::RenderPassRecorder& pass) {
                const auto first_pass = index == 0;

                const auto pipeline_handle =
                    first_pass ? m_gather_first_pipeline->handle() : m_gather_pipeline->handle();

                if (!first_pass) {
                    const auto previous = m_depths[(index + DEPTH_COUNT - 1) % DEPTH_COUNT]->handle();
                    pass.bind_sampled_image(previous, m_sampler->handle(), 0);
                }
                pass.bind_graphics_pipeline(pipeline_handle);
                draw_scene(pass);
            }
        );
    }

    // combine pass, blends all layers into final image
    m_device.render_pass(
        {.target = m_combine_target},
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_combine_pipeline->handle());
            for (const auto [index, color] : std::views::enumerate(m_colors)) {
                pass.bind_sampled_image(color->handle(), m_sampler->handle(), index);
            }
            pass.draw_arrays(0, 3);
        }
    );

    return *m_output;
}

auto DepthPeeling::resize(const glm::uvec2 extent) -> void {
    create_images(extent);
    create_render_targets();
}

auto DepthPeeling::render_debug_info() -> void {
    // PANIC("depth peeling debug info is not yet implemented");
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
    for (const auto i : siren::range(DEPTH_COUNT)) {
        m_depths[i] = std::make_unique<siren::Image>(
            m_device.create_image(
                {
                    .label         = std::format("Depth Peeling Depth {}", i),
                    .format        = siren::ImageFormat::Depth32f,
                    .extent        = {.width = extent.x, .height = extent.y},
                    .dimension     = siren::ImageDimension::D2,
                    .mipmap_levels = 1,
                }
            )
        );
    }

    // create output
    m_output = std::make_unique<siren::Image>(
        m_device.create_image(
            {
                .label         = "output image",
                .format        = siren::ImageFormat::RGBA8,
                .extent        = {.width = extent.x, .height = extent.y},
                .dimension     = siren::ImageDimension::D2,
                .mipmap_levels = 1,
            }
        )
    );
}

auto DepthPeeling::create_sampler() -> void {
    m_sampler = std::make_unique<siren::Sampler>(
        m_device.create_sampler(
            {
                .min_filter    = siren::ImageFilterMode::Nearest,
                .max_filter    = siren::ImageFilterMode::Nearest,
                .mipmap_filter = siren::ImageFilterMode::Nearest,
                .s_wrap        = siren::ImageWrapMode::ClampEdge,
                .t_wrap        = siren::ImageWrapMode::ClampEdge,
                .r_wrap        = siren::ImageWrapMode::ClampEdge,
                .lod_min       = 0.f,
                .lod_max       = 1.f,
                .border_color  = std::nullopt,
                .compare_mode  = siren::ImageCompareMode::None,
                .compare_fn    = siren::ImageCompareFn::LessEqual,
            }
        )
    );
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

    // combine target
    m_combine_target = siren::RenderTarget{
        .colors = {
            {
                .image           = m_output->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero(),
            },
        },
        .depth_stencil = std::nullopt,
    };
}

auto DepthPeeling::create_pipelines() -> void {
    // create our graphics pipelines
    {
        m_gather_first_shader = m_assets.load<siren::ShaderAsset>(
            "oiter://assets/shaders/depth_peeling/gather_first.sshg"
        );
        const auto shader = m_assets.get_unsafe(m_gather_first_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Depth Peeling Gather First",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Less,
                    .back_face_culling = false,
                    .depth_test        = true,
                    .depth_write       = true,
                }
            )
        );
    }

    {
        m_gather_shader   = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/depth_peeling/gather.sshg");
        const auto shader = m_assets.get_unsafe(m_gather_shader).shader.handle();

        m_gather_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Depth Peeling Gather",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Less,
                    .back_face_culling = false,
                    .depth_test        = true,
                    .depth_write       = true,
                }
            )
        );
    }

    {
        m_combine_shader  = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/depth_peeling/combine.sshg");
        const auto shader = m_assets.get_unsafe(m_combine_shader).shader.handle();

        m_combine_pipeline = std::make_unique<siren::GraphicsPipeline>(
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
}
} // namespace oiter
