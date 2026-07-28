#include "blend_pass.hpp"

#include "2iren/rhi/resources/sampler.hpp"

namespace oiter {
static auto create_query(siren::Device& device) -> siren::Query {
    return device.create_query({.kind = siren::QueryKind::SamplesPassed});
}

static auto create_target(siren::Device& device, const glm::uvec2 extent) -> RenderTargetResources {
    return RenderTargetBuilder::create(device, extent, "Blend Pass")
           .add_color(siren::ImageFormat::RGB8, siren::BeginOperation::Preserve, siren::Rgba::white())
           .build();
}

static auto create_pipeline(siren::Device& device, siren::AssetServer& server) -> GraphicsPipelineResources {
    siren::GraphicsPipelineDescriptor descriptor{
        .label               = "Blend Pipeline",
        .layout              = siren::DEFAULT_VERTEX_LAYOUT,
        .alpha_mode          = siren::AlphaMode::Blend,
        .depth_function      = siren::DepthFunction::Less,
        .source_blend_factor = siren::BlendFactor::SourceAlpha,
        .dest_blend_factor   = siren::BlendFactor::OneMinusSourceAlpha,
        .back_face_culling   = false,
        .depth_test          = false,
        .depth_write         = false,
    };

    return GraphicsPipelineResources::create(
        device,
        server,
        "oiter://assets/shaders/dual_depth_peeling/blend.sshg",
        descriptor
    );
}

BlendPass::BlendPass(
    siren::Device& device,
    siren::AssetServer& server,
    const glm::uvec2& extent,
    const std::shared_ptr<DualDepthPeelingConfig>& config
) : m_device(device),
    m_pipeline(create_pipeline(device, server)),
    m_target(create_target(device, extent)),
    m_query(create_query(device)),
    m_config(config) {}

auto BlendPass::execute(
    const siren::Sampler& sampler,
    const RenderTargetResources& read_target
) const -> bool {
    const auto pipeline_handle = m_pipeline.graphics_pipeline.handle();
    const auto sampler_handle  = sampler.handle();
    const auto read_handle     = read_target.colors[0].handle();

    m_device.render_submit(
        [this, sampler_handle, pipeline_handle, read_handle](siren::RenderCommandRecorder& cmds) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = m_target.render_target},
                [this, sampler_handle, pipeline_handle, read_handle](siren::RenderPassRecorder& pass) -> void {
                    if (m_config->occlusion_query) {
                        pass.begin_query(m_query.handle());
                    }
                    pass.bind_graphics_pipeline(pipeline_handle);
                    pass.bind_sampled_image(read_handle, sampler_handle, 0);
                    pass.draw_arrays(0, 3);
                    if (m_config->occlusion_query) {
                        pass.end_query(m_query.handle());
                    }
                }
            );
        }
    );

    if (m_config->occlusion_query) {
        const auto samples_passed = m_device.query(m_query.handle());
        siren::log::debug("Samples passed: {}", samples_passed);
        return samples_passed == 0;
    }
    return false;
}

auto BlendPass::resize(const glm::uvec2 extent) -> void {
    m_target = create_target(m_device, extent);
}
} // namespace oiter
