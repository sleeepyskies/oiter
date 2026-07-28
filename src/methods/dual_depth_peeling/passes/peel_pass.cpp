#include "peel_pass.hpp"

namespace oiter {
PingPongTarget::PingPongTarget(RenderTargetResources&& target0, RenderTargetResources&& target1) : targets(
    {
        std::move(target0),
        std::move(target1)
    }
) {}

auto PingPongTarget::read_target() const -> const RenderTargetResources& { return targets[index]; }
auto PingPongTarget::write_target() const -> const RenderTargetResources& { return targets[1 - index]; }
auto PingPongTarget::swap_targets() const -> void { index = 1 - index; }

static auto create_target(siren::Device& device, const glm::uvec2 extent) -> PingPongTarget {
    // todo: put -1 into max depth constant or something

    auto target0 = RenderTargetBuilder::create(device, extent, "Peel Pass Target 0")
                   .add_color(siren::ImageFormat::RG32f, siren::BeginOperation::Clear, siren::Rgba{-1, -1, 0, 0})
                   .add_color(siren::ImageFormat::RGBA8, siren::BeginOperation::Clear, siren::Rgba::zero())
                   .add_color(siren::ImageFormat::RGBA8, siren::BeginOperation::Clear, siren::Rgba::zero())
                   .build();

    auto target1 = RenderTargetBuilder::create(device, extent, "Peel Pass Target 1")
                   .add_color(siren::ImageFormat::RG32f, siren::BeginOperation::Clear, siren::Rgba{-1, -1, 0, 0})
                   .add_color(siren::ImageFormat::RGBA8, siren::BeginOperation::Clear, siren::Rgba::zero())
                   .add_color(siren::ImageFormat::RGBA8, siren::BeginOperation::Clear, siren::Rgba::zero())
                   .build();

    return PingPongTarget(std::move(target0), std::move(target1));
}

static auto create_pipeline(siren::Device& device, siren::AssetServer& server) -> GraphicsPipelineResources {
    siren::GraphicsPipelineDescriptor descriptor{
        .label          = "Peel Pass Pipeline",
        .layout         = siren::DEFAULT_VERTEX_LAYOUT,
        .topology       = siren::PrimitiveTopology::Triangles,
        .alpha_mode     = siren::AlphaMode::Blend,
        .blend_function = siren::BlendFunction::Max,
        .depth_function = siren::DepthFunction::Less,
        // todo: should this be enabled? back face cull
        .back_face_culling = false,
        .depth_test        = false,
        .depth_write       = false,
    };

    return GraphicsPipelineResources::create(
        device,
        server,
        "oiter://assets/shaders/dual_depth_peeling/peel.sshg",
        descriptor
    );
}

PeelPass::PeelPass(
    siren::Device& device,
    siren::AssetServer& server,
    const glm::uvec2& extent
) : m_device(device),
    m_target(create_target(device, extent)),
    m_pipeline(create_pipeline(device, server)) {}

auto PeelPass::execute(
    const BakedScene& scene,
    const siren::Sampler& sampler,
    const DualDepthPeelingUniforms& uniforms,
    const siren::usize ubo_alignment
) const -> void {
    const auto sampler_handle = sampler.handle();

    // lil risky pass into lambda by ref but DualDepthPeeling is sure to keep alive :D
    m_device.render_submit(
        [this, &scene, &uniforms, sampler_handle, ubo_alignment](siren::RenderCommandRecorder& cmds) -> void {
            cmds.render_pass(
                siren::RenderPassDescriptor{.target = m_target.write_target().render_target},
                [this, &scene, &uniforms, sampler_handle, ubo_alignment](siren::RenderPassRecorder& pass) -> void {
                    const auto& read_target = m_target.read_target();

                    pass.bind_graphics_pipeline(m_pipeline.graphics_pipeline.handle());

                    pass.bind_sampled_image(read_target.colors[0].handle(), sampler_handle, 0); // min max
                    pass.bind_sampled_image(read_target.colors[1].handle(), sampler_handle, 1); // front
                    pass.bind_sampled_image(read_target.colors[2].handle(), sampler_handle, 2); // back

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

auto PeelPass::resize(const glm::uvec2 extent) -> void { m_target = create_target(m_device, extent); }
auto PeelPass::read_target() const -> const RenderTargetResources& { return m_target.read_target(); }
auto PeelPass::write_target() const -> const RenderTargetResources& { return m_target.write_target(); }
auto PeelPass::swap_targets() const -> void { m_target.swap_targets(); }
} // namespace oiter
