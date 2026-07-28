#include "blend_pass.hpp"

#include "2iren/rhi/resources/sampler.hpp"

namespace oiter {
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
    const glm::uvec2& extent
) : m_device(device),
    m_pipeline(create_pipeline(device, server)),
    m_target(create_target(device, extent)) {}

auto BlendPass::execute(
    const siren::Sampler& sampler,
    const RenderTargetResources& read_target
) const -> void {
    const auto pipeline_handle = m_pipeline.graphics_pipeline.handle();
    const auto sampler_handle  = sampler.handle();
    const auto read_handle     = read_target.colors[0].handle();

    m_device.render_submit(
        [this, sampler_handle, pipeline_handle, read_handle](siren::RenderCommandRecorder& cmds) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = m_target.render_target},
                [sampler_handle, pipeline_handle, read_handle](siren::RenderPassRecorder& pass) -> void {
                    pass.bind_graphics_pipeline(pipeline_handle);
                    pass.bind_sampled_image(read_handle, sampler_handle, 0);
                    pass.draw_arrays(0, 3);
                }
            );
        }
    );
}

auto BlendPass::resize(const glm::uvec2 extent) -> void {
    m_target = create_target(m_device, extent);
}
} // namespace oiter
