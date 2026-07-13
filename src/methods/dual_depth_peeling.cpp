#include "dual_depth_peeling.hpp"

#include "../bake.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/window.hpp"

namespace oiter {

DualDepthPeeling::DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server) :
    m_device(device), m_opaque(init_opaque_pass(device, window, server)),
    m_final_image(init_final_image(device, window)), m_uniform_buffer(init_uniform_buffer(device)) {
    //
}

auto DualDepthPeeling::render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const -> const siren::Image& {
    perform_opaque_pass(camera, scene);
    perform_depth_peeling(scene);
    perform_blending();
    return m_opaque.target_image;
}

auto DualDepthPeeling::perform_opaque_pass(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> void {
    m_uniform_buffer.upload(siren::ByteBuffer{ camera.projection_view() });
    m_device.render_submit([&](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .clear_color = siren::RGBA::green(), .target = m_opaque.target },
                         [&](siren::RenderPassRecorder& pass) -> void {
                             pass.bind_graphics_pipeline(m_opaque.pipeline.handle());
                             pass.bind_uniform_buffer(m_uniform_buffer.handle(), 0);
                             for (const auto& surface : scene.opaque) {
                                 pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                                 pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                                 pass.draw_indexed(surface.index.count, 0);
                             }
                         });
    });
}

auto DualDepthPeeling::perform_depth_peeling(const BakedScene& scene) const -> void {
    return;
}
auto DualDepthPeeling::perform_blending() const -> void {
    return;
}

auto DualDepthPeeling::init_opaque_pass(siren::Device& device, const siren::Window& window, siren::AssetServer& server)
        -> OpaquePassData {
    const auto shaderh = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/opaque.sshg");
    auto shader_asset  = server.get(shaderh);
    auto target_image  = device.create_image({
             .label         = "DDP Opaque Pass Target",
             .format        = siren::ImageFormat::RGBA8,
             .extent        = siren::ImageExtent{ .width = window.width(), .height = window.height() },
             .dimension     = siren::ImageDimension::D2,
             .mipmap_levels = 1,
    });

    siren::log::info("Created opaque render target with img ID {}", target_image);

    return OpaquePassData{
        .shaderh      = shaderh,
        .target       = siren::RenderTarget{ target_image.handle() },
        .target_image = std::move(target_image),
        .pipeline     = device.create_graphics_pipeline({
                    .label             = "Opaque Graphics Pipeline",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = shader_asset->shader.handle(),
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .depth_function    = siren::DepthFunction::Less,
                    .back_face_culling = true,
                    .depth_test        = true,
                    .depth_write       = true,
        }),
    };
}

auto DualDepthPeeling::init_final_image(siren::Device& device, const siren::Window& window) -> siren::Image {
    return device.create_image({
            .label         = "Dual Depth Peeling Final Image",
            .format        = siren::ImageFormat::RGBA8,
            .extent        = siren::ImageExtent{ .width = window.width(), .height = window.height() },
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
    });
}

auto DualDepthPeeling::init_uniform_buffer(siren::Device& device) -> siren::Buffer {
    return device.create_buffer({
            .label = "Uniform Buffer",
            .size  = sizeof(UniformBufferData),
            .usage = siren::BufferUsage::Static,
    });
}

}  // namespace oiter
