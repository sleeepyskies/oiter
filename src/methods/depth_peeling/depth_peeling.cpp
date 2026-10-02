#include "depth_peeling.hpp"

#include <imgui.h>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/core/base.hpp"

#include "2iREN/graphics/commands.hpp"
#include "2iREN/graphics/image.hpp"
#include "2iREN/graphics/types.hpp"
#include "2iREN/math/bounded.hpp"
#include "utility/imgui_extras.hpp"

using namespace siren;

namespace oiter {

DepthPeeling::DepthPeeling(Device& device, const Extent2 extent, AssetServer& assets) :
    OitMethod(device, assets) {
    create_images(extent);
    create_sampler();
    create_pipelines();
    create_queries();
}

auto DepthPeeling::render(
    CommandBuffer& cmds,
    const ImageHandle output,
    const Camera& camera,
    const BakedScene& scene
) const -> void {
    update_buffers(camera, scene);

    auto draw_scene = [&](RenderCommandEncoder& pass) {
        pass.bind_uniform_buffer(m_scene_buffer->handle(), Slot{0});
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
    const auto write_color = ColorAttachment{
        .image           = m_write_color->handle(),
        .begin_operation = BeginOperation::Clear,
        .clear_color     = Rgba::ZERO(),
    };

    const auto accumulation_color = ColorAttachment{
        .image           = m_accumulation_color->handle(),
        .begin_operation = BeginOperation::Preserve,
    };

    // set up image to back to front blending
    m_accumulation_color->clear(Rgba::ZERO());

    for (const auto layer : range(m_config.layers)) {
        m_config.peels_last_frame++;

        const auto query_index      = layer % m_queries.size();
        const auto last_query_index = 1 - query_index;
        const auto& query           = m_queries[query_index];
        const auto& last_query      = m_queries[last_query_index];

        // we need to ping pong between our 2 depth buffers
        const auto write_buffer_index = layer % 2;
        const auto read_buffer_index  = 1 - write_buffer_index;

        if (m_config.occlusion_query && layer > 0) {
            m_device.begin_conditional_render(last_query->handle());
        }

        // perform the peeling pass
        cmds.render_pass(
            RenderPassDescriptor{
                .target =
                    {
                        .colors = {write_color},
                        .depth_stencil =
                            DepthStencilAttachment{
                                .image           = m_depths[write_buffer_index]->handle(),
                                .begin_operation = BeginOperation::Clear,
                                .clear_depth     = 1,
                                .clear_stencil   = 0,
                            },
                    }
            },
            [&](RenderCommandEncoder& pass) {
                // first pass never discards fragments
                if (m_config.occlusion_query) {
                    pass.begin_query(query->handle());
                }

                // use different peel shader on the first pass
                const auto first_pass    = layer == 0;
                const auto peel_pipeline = first_pass ? m_gather_first_pipeline->handle()
                                                      : m_gather_pipeline->handle();
                if (!first_pass) {
                    pass.bind_sampled_image(
                        m_depths[read_buffer_index]->handle(),
                        m_sampler->handle(),
                        Slot{0}
                    );
                }
                pass.bind_graphics_pipeline(peel_pipeline);

                draw_scene(pass);

                if (m_config.occlusion_query) {
                    pass.end_query(query->handle());
                }
            }
        );

        if (m_config.occlusion_query && layer > 0) {
            m_device.end_conditional_render();
        }

        // stop early for debug inspections
        if (m_config.inspected_layer.get() - 1u == layer) {
            switch (m_config.inspecting) {
                case Config::DepthTexture: {
                    return m_depths[write_buffer_index]->handle();
                }
                case Config::WriteTexture: {
                    return m_write_color->handle();
                }
                default: break;
            }
        }

        if (m_config.occlusion_query) {
            m_device.begin_conditional_render(query->handle());
        }

        // perform on the fly blending
        m_device.render_pass(
            RenderPassDescriptor{
                .target =
                    {
                        .colors        = {accumulation_color},
                        .depth_stencil = std::nullopt,
                    }
            },
            [&](RenderPassRecorder& pass) {
                pass.bind_graphics_pipeline(m_blend_pipeline->handle());
                pass.bind_sampled_image(m_write_color->handle(), m_sampler->handle(), Slot{0});
                pass.draw_fullscreen();
            }
        );

        if (m_config.occlusion_query) {
            m_device.end_conditional_render();

            if (layer > 0 && m_device.query_available(last_query->handle())) {
                log::trace("query result available.");
                if (m_device.query_result(last_query->handle()) == 0) {
                    log::trace("query result: no samples passes, breaking.");
                    break;
                }
            }
        }
    }
}

auto DepthPeeling::resize(const Extent2 extent) -> void {
    create_images(extent);
}

auto DepthPeeling::reload_shaders() -> void {
    create_pipelines();
}

auto DepthPeeling::render_debug_info() -> void {
    ImGui::Text("Peels performed last frame %u", m_config.peels_last_frame);
    m_config.peels_last_frame = 0;

    ImGuiExtra::SliderBoundedU32("Layers", &m_config.layers);
    ImGui::Checkbox("Perform Occlussion Query", &m_config.occlusion_query);

    const auto select_layer = [this]() {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        auto val = m_config.inspected_layer.get();
        if (ImGuiExtra::SliderUint("##inspected_layer", &val, 1, m_config.layers.get())) {
            m_config.inspected_layer = val;
        };
    };

    i32* inspecting = (i32*)(&m_config.inspecting);

    ImGui::RadioButton("See Final Output     ", inspecting, 0);

    ImGui::RadioButton("Inspect Write Texture", inspecting, 1);
    if (m_config.inspecting == Config::Inspecting::WriteTexture) {
        select_layer();
    }

    ImGui::RadioButton("Inspect Depth Texture", inspecting, 2);
    if (m_config.inspecting == Config::Inspecting::DepthTexture) {
        select_layer();
    }
}

auto DepthPeeling::create_images(const Extent2 extent) -> void {
    m_accumulation_color = std::make_unique(m_device.make_image({
        .label  = "Depth Peeling Accumulation Color",
        .format = ImageFormat::RGBA8,
        .extent = extent.to_extent3(),
        .flags  = ImageFlags::make(ImageFlag::ShaderWrite),
    }));

    m_write_color = std::make_unique(m_device.make_image({
        .label  = "Depth Peeling Write Color",
        .format = ImageFormat::RGBA8,
        .extent = extent.to_extent3(),
        .flags  = ImageFlags::make(ImageFlag::ShaderWrite),
    }));

    m_depths[0] = std::make_unique(m_device.make_image({
        .label  = "Depth Peeling Depth0",
        .format = ImageFormat::Depth32f,
        .extent = extent.to_extent3(),
        .flags  = ImageFlags::make(ImageFlag::ShaderRead),
    }));

    m_depths[1] = std::make_unique(m_device.make_image({
        .label  = "Depth Peeling Depth1",
        .format = ImageFormat::Depth32f,
        .extent = extent.to_extent3(),
        .flags  = ImageFlags::make(ImageFlag::ShaderRead),
    }));
}

auto DepthPeeling::create_sampler() -> void {
    m_sampler = std::make_unique<Sampler>(m_device.make_sampler({
        .min_filter    = ImageFilterMode::Nearest,
        .max_filter    = ImageFilterMode::Nearest,
        .mipmap_filter = ImageFilterMode::Nearest,
        .s_wrap        = ImageWrapMode::ClampEdge,
        .t_wrap        = ImageWrapMode::ClampEdge,
        .r_wrap        = ImageWrapMode::ClampEdge,
        .lod_min       = 0.f,
        .lod_max       = 1.f,
        .border_color  = std::nullopt,
        .compare_mode  = ImageCompareMode::None,
        .compare_fn    = ImageCompareFn::LessEqual,
    }));
}

auto DepthPeeling::create_pipelines() -> void {
    m_gather_first_shader = NullHandle;
    m_gather_shader       = NullHandle;
    m_blend_shader        = NullHandle;

    m_gather_first_pipeline = nullptr;
    m_gather_pipeline       = nullptr;
    m_blend_pipeline        = nullptr;

    {
        m_gather_first_shader = m_assets.load<ShaderAsset>(
            "oiter://assets/shaders/depth_peeling/gather_first.sshg"
        );
        const auto shader = m_assets.get_unsafe(m_gather_first_shader).shader.handle();

        m_gather_first_pipeline = std::make_unique<GraphicsPipeline>(
            m_device.make_graphics_pipeline({
                .label             = "Depth Peeling Gather First",
                .layout            = DEFAULT_VERTEX_LAYOUT,
                .shader            = shader,
                .topology          = PrimitiveTopology::Triangles,
                .alpha_mode        = AlphaMode::Opaque,
                .depth_function    = DepthFunction::Less,
                .back_face_culling = false,
                .depth_test        = true,
                .depth_write       = true,
            })
        );
    }

    {
        m_gather_shader = m_assets.load<ShaderAsset>(
            "oiter://assets/shaders/depth_peeling/gather.sshg"
        );
        const auto shader = m_assets.get_unsafe(m_gather_shader).shader.handle();

        m_gather_pipeline = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
            .label             = "Depth Peeling Gather",
            .layout            = DEFAULT_VERTEX_LAYOUT,
            .shader            = shader,
            .topology          = PrimitiveTopology::Triangles,
            .alpha_mode        = AlphaMode::Opaque,
            .depth_function    = DepthFunction::Less,
            .back_face_culling = false,
            .depth_test        = true,
            .depth_write       = true,
        }));
    }

    {
        m_blend_shader = m_assets.load<ShaderAsset>(
            "oiter://assets/shaders/depth_peeling/blend.sshg"
        );
        const auto shader = m_assets.get_unsafe(m_blend_shader).shader.handle();
        m_blend_pipeline  = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
            .label      = "Depth Peeling Blend",
            .layout     = FULLSCREEN_VERTEX_LAYOUT,
            .shader     = shader,
            .topology   = PrimitiveTopology::Triangles,
            .alpha_mode = AlphaMode::Blend,
            .color_blend =
                {
                    .function      = BlendFunction::Add,
                    .source_factor = BlendFactor::OneMinusDestinationAlpha,
                    .dest_factor   = BlendFactor::One,
                },
            .alpha_blend =
                {
                    .function      = BlendFunction::Add,
                    .source_factor = BlendFactor::OneMinusDestinationAlpha,
                    .dest_factor   = BlendFactor::One,
                },
            .back_face_culling = false,
            .depth_test        = false,
            .depth_write       = false,
        }));
    }
}

auto DepthPeeling::create_queries() -> void {
    for (auto& query : m_queries) {
        query = std::make_unique<Query>(m_device.make_query({.kind = QueryKind::AnySamplesPassed}));
    }
}
} // namespace oiter
