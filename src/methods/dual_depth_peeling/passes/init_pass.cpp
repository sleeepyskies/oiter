#include "init_pass.hpp"


namespace oiter {
static auto create_pipeline(siren::Device& device, siren::AssetServer& server) -> GraphicsPipelineResources {
    siren::GraphicsPipelineDescriptor descriptor{
        .label             = "Init Pipeline",
        .layout            = siren::DEFAULT_VERTEX_LAYOUT,
        .topology          = siren::PrimitiveTopology::Triangles,
        .alpha_mode        = siren::AlphaMode::Blend,
        .blend_function    = siren::BlendFunction::Max,
        .back_face_culling = false,
        .depth_test        = false,
        .depth_write       = false,
    };

    return GraphicsPipelineResources::create(
        device,
        server,
        "oiter://assets/shaders/dual_depth_peeling/init.sshg",
        descriptor
    );
}

InitPass::InitPass(
    siren::Device& device,
    siren::AssetServer& server,
    const std::shared_ptr<DdpConfig>& config
) : m_device(device),
    m_pipeline(create_pipeline(device, server)), m_config(config) {}

auto InitPass::execute(
    const BakedScene& scene,
    const siren::RenderTarget& read_target,
    const DualDepthPeelingUniforms& uniforms,
    const siren::usize ubo_alignment
) const -> void {
    const auto pipeline_handle = m_pipeline.graphics_pipeline.handle();

    // inits the depth min max image of the peel target to max and min depths of transparent geometry in the scene
    m_device.render_submit(
        [&scene, &read_target, &uniforms, pipeline_handle, ubo_alignment](
        siren::RenderCommandRecorder& cmds
    ) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = read_target},
                [&scene, pipeline_handle, &uniforms, ubo_alignment](
                siren::RenderPassRecorder& pass
            ) -> void {
                    pass.bind_graphics_pipeline(pipeline_handle);

                    pass.bind_uniform_buffer(uniforms.scene_data.handle(), 0);
                    pass.bind_uniform_buffer(uniforms.material_data.handle(), 1);

                    for (const auto& [index, surface] : std::views::enumerate(scene.transparent)) {
                        pass.bind_uniform_buffer_range(
                            uniforms.per_mesh_data.handle(),
                            2,
                            ubo_alignment * (scene.opaque.size() + index),
                            sizeof(PerMeshData)
                        );

                        pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                        pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                        pass.draw_indexed(surface.index.count, 0);
                    }
                }
            );
        }
    );
}
} // namespace oiter
