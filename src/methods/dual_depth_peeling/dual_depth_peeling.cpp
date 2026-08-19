#include "dual_depth_peeling.hpp"

#include <imgui.h>

#include "../../bake.hpp"
#include "2iren/rhi/device.hpp"

namespace oiter {
DualDepthPeeling::DualDepthPeeling(
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    create_query();
    create_sampler();
    create_images(extent);
    create_render_targets();
    create_pipelines();
}

auto DualDepthPeeling::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    // todo: could we cache and reuse the command buffers across frames? ig wont help 2 much for opengl
    update_buffers(camera, scene);

    const auto draw_scene = [&](siren::RenderPassRecorder& pass) {
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

    // init pass
    m_device.render_pass(
        siren::RenderPassDescriptor{.target = read_target()},
        [&](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_init_pipeline->handle());
            draw_scene(pass);
        }
    );

    m_blend_image->clear(siren::Rgba::zero());

    for (const auto _ : siren::range(m_config.max_peels)) {
        m_last_frame_peels++;
        // peel pass
        m_device.render_pass(
            siren::RenderPassDescriptor{.target = write_target()},
            [&](siren::RenderPassRecorder& pass) -> void {
                const auto& read_target = this->read_target();

                pass.bind_graphics_pipeline(m_peel_pipeline->handle());

                pass.bind_sampled_image(read_target.colors[0].image, m_sampler->handle(), 0); // min max
                pass.bind_sampled_image(read_target.colors[1].image, m_sampler->handle(), 1); // front
                pass.bind_sampled_image(read_target.colors[2].image, m_sampler->handle(), 2); // back

                draw_scene(pass);
            }
        );

        // blend pass
        m_device.render_pass(
            siren::RenderPassDescriptor{.target = m_blend_target},
            [this](siren::RenderPassRecorder& pass) -> void {
                if (m_config.perform_query) { pass.begin_query(m_occlusion_query->handle()); }

                pass.bind_graphics_pipeline(m_blend_pipeline->handle());
                pass.bind_sampled_image(write_target().colors[2].image, m_sampler->handle(), 0);
                pass.draw_arrays(0, 3);

                if (m_config.perform_query) { pass.end_query(m_occlusion_query->handle()); }
            }
        );

        swap_targets();

        if (m_config.perform_query) {
            const auto samples_passed = m_device.query(m_occlusion_query->handle());
            if (samples_passed == 0) { break; } // early end, nothing was drawn
        }
    }

    // final pass
    m_device.render_pass(
        siren::RenderPassDescriptor{.target = m_final_target},
        [this](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_final_pipeline->handle());
            pass.bind_sampled_image(read_target().colors[1].image, m_sampler->handle(), 0);
            pass.bind_sampled_image(m_blend_image->handle(), m_sampler->handle(), 1); // accumulated back
            pass.draw_arrays(0, 3);
        }
    );

    reset_targets();

    return *m_final_image;
}

auto DualDepthPeeling::resize(const glm::uvec2 extent) -> void {
    create_images(extent);
    create_render_targets();
}

auto DualDepthPeeling::reload_shaders() -> void {
    m_init_pipeline  = nullptr;
    m_peel_pipeline  = nullptr;
    m_blend_pipeline = nullptr;
    m_final_pipeline = nullptr;
    m_init_shader    = siren::NullHandle;
    m_peel_shader    = siren::NullHandle;
    m_blend_shader   = siren::NullHandle;
    m_final_shader   = siren::NullHandle;
    create_pipelines();
}

void DualDepthPeeling::render_debug_info() {
    ImGui::Text("Peels performed last frame %u", m_last_frame_peels);
    ImGui::SliderInt("Max Peels", &m_config.max_peels, 1, 16);
    ImGui::Checkbox("Occlusion Query", &m_config.perform_query);

    m_last_frame_peels = 0;
}

auto DualDepthPeeling::create_sampler() -> void {
    m_sampler = std::make_unique<siren::Sampler>(
        m_device.create_sampler(
            {
                .min_filter    = siren::ImageFilterMode::Nearest,
                .max_filter    = siren::ImageFilterMode::Nearest,
                .mipmap_filter = siren::ImageFilterMode::Nearest,
                .s_wrap        = siren::ImageWrapMode::ClampEdge,
                .t_wrap        = siren::ImageWrapMode::ClampEdge,
            }
        )
    );
}

auto DualDepthPeeling::create_images(const glm::uvec2 extent) -> void {
    const auto create_image = [&](const std::string& label, const siren::ImageFormat format) {
        return std::make_unique<siren::Image>(
            m_device.create_image(
                {
                    .label         = label,
                    .format        = format,
                    .extent        = {.width = extent.x, .height = extent.y, .depth_or_layers = 1},
                    .dimension     = siren::ImageDimension::D2,
                    .mipmap_levels = 1,
                }
            )
        );
    };

    // create ping pong dual depth images
    for (const auto i : siren::range(2)) {
        m_pingpong_colors[i * 3] = create_image(std::format("Ping Pong {} Depth 0", i), siren::ImageFormat::RG32f);
    }

    // create ping pong color images
    for (const auto i : siren::range(4)) {
        // maps (0, 1, 2, 3) to  (1, 2, 4, 5)
        const auto j         = i + 1 + (i >= 2);
        m_pingpong_colors[j] = create_image(
            std::format("Ping Pong {} Color {}", j < 3 ? 0 : 1, 1 + i % 2),
            siren::ImageFormat::RGBA16f
        );
    }

    m_blend_image = create_image("Blend Image", siren::ImageFormat::RGBA16f);
    m_final_image = create_image("Final Image", siren::ImageFormat::RGBA8);
}

auto DualDepthPeeling::create_render_targets() -> void {
    m_pingpong_targets[0] = siren::RenderTarget{
        .colors = {
            {
                .image           = m_pingpong_colors[0]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba{-1, -1, 0, 0}
            },
            {
                .image           = m_pingpong_colors[1]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero(),
            },
            {
                .image           = m_pingpong_colors[2]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero(),
            },
        },
        .depth_stencil = std::nullopt,
    };

    m_pingpong_targets[1] = siren::RenderTarget{
        .colors = {
            {
                .image           = m_pingpong_colors[3]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba{-1, -1, 0, 0}
            },
            {
                .image           = m_pingpong_colors[4]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero(),
            },
            {
                .image           = m_pingpong_colors[5]->handle(),
                .begin_operation = siren::BeginOperation::Clear,
                .clear_color     = siren::Rgba::zero(),
            },
        },
        .depth_stencil = std::nullopt,
    };

    m_blend_target = siren::RenderTarget{
        .colors = {
            {
                .image           = m_blend_image->handle(),
                .begin_operation = siren::BeginOperation::Preserve,
                .clear_color     = siren::Rgba{0, 1},
            },
        },
        .depth_stencil = std::nullopt,
    };

    m_final_target = siren::RenderTarget{
        .colors = {
            {
                .image           = m_final_image->handle(),
                .begin_operation = siren::BeginOperation::Preserve,
                .clear_color     = siren::Rgba::zero(),
            },
        },
        .depth_stencil = std::nullopt,
    };
}

auto DualDepthPeeling::create_pipelines() -> void {
    {
        m_init_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/init.sshg");
        m_assets.wait_until_loaded(m_init_shader);
        const auto shader = m_assets.get(m_init_shader)->shader.handle();
        m_init_pipeline   = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Init Pipeline",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Blend,
                    .color_blend       = {.function = siren::BlendFunction::Max}, // MAX(C_src, C_dst)
                    .alpha_blend       = {.function = siren::BlendFunction::Max},
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }

    {
        m_peel_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/peel.sshg");
        m_assets.wait_until_loaded(m_peel_shader);
        const auto shader = m_assets.get(m_peel_shader)->shader.handle();
        m_peel_pipeline   = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Peel Pipeline",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Blend,
                    .color_blend       = {.function = siren::BlendFunction::Max}, // MAX(C_src, C_dst)
                    .alpha_blend       = {.function = siren::BlendFunction::Max},
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }

    {
        m_blend_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/blend.sshg");
        m_assets.wait_until_loaded(m_blend_shader);
        const auto shader = m_assets.get(m_blend_shader)->shader.handle();

        m_blend_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label       = "Blend Pipeline",
                    .layout      = siren::DEFAULT_VERTEX_LAYOUT,
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

    {
        m_final_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/final.sshg");
        m_assets.wait_until_loaded(m_final_shader);
        const auto shader = m_assets.get(m_final_shader)->shader.handle();
        m_final_pipeline  = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "Final Pipeline",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader,
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }
}

auto DualDepthPeeling::create_query() -> void {
    m_occlusion_query = std::make_unique<siren::Query>(
        m_device.create_query({.kind = siren::QueryKind::SamplesPassed})
    );
}

auto DualDepthPeeling::read_target() const -> const siren::RenderTarget& {
    return m_pingpong_targets[m_pingpong_index];
}

auto DualDepthPeeling::write_target() const -> const siren::RenderTarget& {
    return m_pingpong_targets[1 - m_pingpong_index];
}

auto DualDepthPeeling::swap_targets() const -> void { m_pingpong_index = 1 - m_pingpong_index; }

auto DualDepthPeeling::reset_targets() const -> void { m_pingpong_index = 0; }
} // namespace oiter
