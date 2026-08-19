#include "depth_peeling.hpp"

#include <imgui.h>

#include "2iren/asset/asset_server.hpp"

namespace oiter {
DepthPeeling::DepthPeeling(
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    create_images(extent);
    create_sampler();
    create_pipelines();
}

auto DepthPeeling::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    update_buffers(camera, scene);

    auto draw_scene = [&](siren::RenderPassRecorder& pass) {
        pass.bind_uniform_buffer(m_scene_buffer->handle(), 0);
        for (const auto& [index, surface] : std::views::enumerate(scene.transparent)) {
            pass.bind_uniform_buffer_range(
                m_mesh_buffer->handle(),
                1,
                mesh_uniforms_alignment() * (scene.opaque.size() + index),
                sizeof(MeshUniforms)
            );
            pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
            pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
            pass.draw_indexed(surface.index.count, 0);
        }
    };

    // we always use same color attachments, so we just create once
    const auto write_color = siren::ColorAttachment{
        .image           = m_write_color->handle(),
        .begin_operation = siren::BeginOperation::Clear,
        .clear_color     = siren::Rgba::zero(),
    };

    const auto accumulation_color = siren::ColorAttachment{
        .image           = m_accumulation_color->handle(),
        .begin_operation = siren::BeginOperation::Preserve,
    };

    // set up image to back to front blending
    m_accumulation_color->clear(siren::Rgba::zero());

    for (const auto layer : siren::range(m_config.layers)) {
        // we need to ping pong between our 2 depth buffers
        const auto write_buffer_index = layer % 2;
        const auto read_buffer_index  = 1 - write_buffer_index;

        // perform the peeling pass
        m_device.render_pass(
            siren::RenderPassDescriptor{
                .target = {
                    .colors        = {write_color},
                    .depth_stencil = siren::DepthStencilAttachment{
                        .image           = m_depths[write_buffer_index]->handle(),
                        .begin_operation = siren::BeginOperation::Clear,
                        .clear_depth     = 1,
                        .clear_stencil   = 0,
                    }
                }
            },
            [&](siren::RenderPassRecorder& pass) {
                // use different peel shader on the first pass
                const auto first_pass    = layer == 0;
                const auto peel_pipeline = first_pass ? m_gather_first_pipeline->handle() : m_gather_pipeline->handle();
                if (!first_pass) {
                    pass.bind_sampled_image(m_depths[read_buffer_index]->handle(), m_sampler->handle(), 0);
                }
                pass.bind_graphics_pipeline(peel_pipeline);
                draw_scene(pass);
            }
        );

        // perform on the fly blending
        m_device.render_pass(
            siren::RenderPassDescriptor{
                .target = {
                    .colors        = {accumulation_color},
                    .depth_stencil = std::nullopt,
                }
            },
            [&](siren::RenderPassRecorder& pass) {
                pass.bind_graphics_pipeline(m_blend_pipeline->handle());
                pass.bind_sampled_image(m_write_color->handle(), m_sampler->handle(), 0);
                pass.draw_arrays(0, 3);
            }
        );
    }

    return *m_accumulation_color;
}

auto DepthPeeling::resize(const glm::uvec2 extent) -> void {
    create_images(extent);
}

auto DepthPeeling::reload_shaders() -> void {
    m_gather_first_shader   = siren::NullHandle;
    m_gather_shader         = siren::NullHandle;
    m_blend_shader          = siren::NullHandle;
    m_gather_first_pipeline = nullptr;
    m_gather_pipeline       = nullptr;
    m_blend_pipeline        = nullptr;
    create_pipelines();
}

auto DepthPeeling::render_debug_info() -> void {
    ImGui::SliderInt("Layers", (int*)&m_config.layers, 1, 25);
}

auto DepthPeeling::create_images(const glm::uvec2 extent) -> void {
    const auto create_image = [&](
        const std::string& label,
        const siren::ImageFormat format
    ) -> std::unique_ptr<siren::Image> {
        return std::make_unique<siren::Image>(
            m_device.create_image(
                {
                    .label         = label,
                    .format        = format,
                    .extent        = {.width = extent.x, .height = extent.y},
                    .dimension     = siren::ImageDimension::D2,
                    .mipmap_levels = 1,
                }
            )
        );
    };

    m_accumulation_color = create_image("Depth Peeling Accumulation Color", siren::ImageFormat::RGBA8);
    m_write_color        = create_image("Depth Peeling Write Color", siren::ImageFormat::RGBA8);

    m_depths[0] = create_image("Depth Peeling Depth0", siren::ImageFormat::Depth32f);
    m_depths[1] = create_image("Depth Peeling Depth1", siren::ImageFormat::Depth32f);
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
        m_blend_shader    = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/depth_peeling/blend.sshg");
        const auto shader = m_assets.get_unsafe(m_blend_shader).shader.handle();

        m_blend_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label       = "Depth Peeling Blend",
                    .layout      = siren::FULLSCREEN_VERTEX_LAYOUT,
                    .shader      = shader,
                    .topology    = siren::PrimitiveTopology::Triangles,
                    .alpha_mode  = siren::AlphaMode::Blend,
                    .color_blend = {
                        .function      = siren::BlendFunction::Add,
                        .source_factor = siren::BlendFactor::SourceAlpha,
                        .dest_factor   = siren::BlendFactor::OneMinusSourceAlpha,
                    },
                    .alpha_blend = {
                        .function      = siren::BlendFunction::Add,
                        .source_factor = siren::BlendFactor::One,
                        .dest_factor   = siren::BlendFactor::OneMinusSourceAlpha,
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
