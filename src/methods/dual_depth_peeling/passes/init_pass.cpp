#include "init_pass.hpp"


namespace oiter {
static auto create_pipeline(siren::Device& device, siren::AssetServer& server) -> GraphicsPipelineResources {
    siren::GraphicsPipelineDescriptor descriptor{
        .label = "Init Pipeline",
        .layout = siren::DEFAULT_VERTEX_LAYOUT,
        .topology = siren::PrimitiveTopology::Triangles,
        .alpha_mode = siren::AlphaMode::Blend,
        .blend_function = siren::BlendFunction::Max,
        .depth_function = siren::DepthFunction::Less,
        .back_face_culling = false,
        .depth_test = false,
        .depth_write = false,
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
    siren::AssetServer& server
) : m_device(device),
    m_pipeline(create_pipeline(device, server)) {}

auto InitPass::execute(
    const BakedScene& scene,
    const siren::RenderTarget& read_target,
    const DualDepthPeelingUniforms& uniforms
) const -> void {
    const auto pipeline_handle = m_pipeline.graphics_pipeline.handle();

    // inits the depth min max image of the peel target to max and min depths of transparent geometry in the scene
    m_device.render_submit(
        [&scene, &read_target, &uniforms, pipeline_handle](
        siren::RenderCommandRecorder& cmds) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = read_target},
                [&scene, pipeline_handle, &uniforms](
                siren::RenderPassRecorder& pass) -> void {
                    pass.bind_graphics_pipeline(pipeline_handle);

                    pass.bind_uniform_buffer(uniforms.scene_data.handle(), 0);
                    pass.bind_uniform_buffer(uniforms.material_data.handle(), 1);

                    for (const auto& surface : scene.transparent) {
                        uniforms.draw_call_data.upload(siren::ByteBuffer{
                            DrawCallData{
                                .model = surface.transform, .material_index = surface.material_index
                            }
                        });
                        pass.bind_uniform_buffer(uniforms.draw_call_data.handle(), 2);

                        pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                        pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                        pass.draw_indexed(surface.index.count, 0);
                    }
                });
        });
}
} // namespace oiter
