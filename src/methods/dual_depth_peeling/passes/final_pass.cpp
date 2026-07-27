#include "final_pass.hpp"

#include "../../util.hpp"
#include "2iren/rhi/resources/sampler.hpp"

namespace oiter {
static auto create_target(siren::Device& device, const glm::uvec2 extent) -> RenderTargetResources {
    return RenderTargetBuilder::create(device, extent, "Final Pass")
           .add_color(siren::ImageFormat::RGB8, siren::BeginOperation::Clear, siren::Rgba::zero())
           .build();
}

static auto create_pipeline(siren::Device& device, siren::AssetServer& server) -> GraphicsPipelineResources {
    siren::GraphicsPipelineDescriptor descriptor{
        .label = "Blend Pipeline",
        .layout = siren::DEFAULT_VERTEX_LAYOUT,
        .alpha_mode = siren::AlphaMode::Opaque,
        .depth_function = siren::DepthFunction::Less,
        .back_face_culling = false,
        .depth_test = false,
        .depth_write = false,
    };

    return GraphicsPipelineResources::create(
        device,
        server,
        "oiter://assets/shaders/dual_depth_peeling/final.sshg",
        descriptor
    );
}

FinalPass::FinalPass(
    siren::Device& device,
    siren::AssetServer& asset_server,
    const glm::uvec2 extent
) : m_device(device),
    m_asset_server(asset_server),
    m_pipeline(create_pipeline(device, asset_server)),
    m_target(create_target(device, extent)) {}

auto FinalPass::execute(
    const siren::Sampler& sampler,
    const RenderTargetResources& read_target
) const -> const siren::Image& {
    const auto sampler_handle = sampler.handle();
    const auto img_handle1 = read_target.colors[1].handle();
    const auto img_handle2 = read_target.colors[2].handle();
    const auto pipeline_handle = m_pipeline.graphics_pipeline.handle();

    m_device.render_submit(
        [this, sampler_handle, img_handle1, img_handle2, pipeline_handle](
        siren::RenderCommandRecorder& cmds) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = m_target.render_target},
                [pipeline_handle, img_handle1, img_handle2, sampler_handle](
                siren::RenderPassRecorder& pass) {
                    pass.bind_graphics_pipeline(pipeline_handle);

                    pass.bind_sampled_image(img_handle1, sampler_handle, 0);
                    pass.bind_sampled_image(img_handle2, sampler_handle, 1);

                    pass.draw_arrays(0, 3);
                });
        });

    return m_target.colors[0];
}

auto FinalPass::resize(const glm::uvec2 extent) -> void {
    m_target = create_target(m_device, extent);
}
} // namespace oiter
