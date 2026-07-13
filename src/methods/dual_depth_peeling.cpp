#include "dual_depth_peeling.hpp"

#include "../bake.hpp"
#include "2iren/asset/assets/shader.hpp"
#include "2iren/rhi/device.hpp"
#include "2iren/window.hpp"

namespace oiter {

[[nodiscard]] static auto create_image(siren::Device& device, const siren::Window& window) -> siren::Image {
    return device.create_image({
            .format        = siren::ImageFormat::RGBA8,
            .extent        = siren::ImageExtent{ .width = window.width(), .height = window.height() },
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
    });
}

DualDepthPeeling::DualDepthPeeling(siren::Device& device, const siren::Window& window, siren::AssetServer& server) :
    m_device(device), m_final_image(create_image(device, window)), m_uniforms(init_uniforms(device)),
    m_geometry(init_geometry_pass(device, server, window)), m_init(init_init_pass(device, server)),
    m_peel(init_peel_pass(device, server, window)), m_blend(init_blend_pass(device, server, window)) {
    //
}

auto DualDepthPeeling::render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> const siren::Image& {
    {
        SceneData data;
        data.view_projection = camera.projection_view();
        data.camera_position = camera.position();
        m_uniforms.scene_data.upload(siren::ByteBuffer{ data });
    }
    {
        // just upload once?
        const auto materials = scene.materials;
        m_uniforms.material_data.upload(siren::ByteBuffer{ materials });
    }

    geometry_pass(scene);
    init_pass(scene);
    peel_pass(scene);
    blend_pass();

    auto& target = m_peel.flag ? m_peel.target0 : m_peel.target1;
    m_peel.flag  = !m_peel.flag;
    return target.images[0];  // todo: just for testing init pass
}

// ==================== RENDER PASSES ==============

auto DualDepthPeeling::geometry_pass(const BakedScene& scene) const -> void {
    // just renders all non-transparent aka opaque geometry into an image

    m_device.render_submit([&](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = m_geometry.target.target }, [&](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_geometry.pipeline.pipeline.handle());

            pass.bind_uniform_buffer(m_uniforms.scene_data.handle(), 0);
            pass.bind_uniform_buffer(m_uniforms.material_data.handle(), 1);

            for (const auto& surface : scene.opaque) {
                m_uniforms.draw_call_data.upload(siren::ByteBuffer{ surface.material_index });
                pass.bind_uniform_buffer(m_uniforms.draw_call_data.handle(), 2);

                pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                pass.draw_indexed(surface.index.count, 0);
            }
        });
    });
}

auto DualDepthPeeling::init_pass(const BakedScene& scene) const -> void {
    // inits the depth min max image of the peel target to max and min depths of transparent geometry in the scene
    auto& target = m_peel.flag ? m_peel.target0 : m_peel.target1;

    m_device.render_submit([this, &target, &scene](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = target.target }, [this, &scene](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_init.pipeline.pipeline.handle());

            pass.bind_uniform_buffer(m_uniforms.scene_data.handle(), 0);
            pass.bind_uniform_buffer(m_uniforms.material_data.handle(), 1);

            for (const auto& surface : scene.transparent) {
                m_uniforms.draw_call_data.upload(siren::ByteBuffer{ surface.material_index });
                pass.bind_uniform_buffer(m_uniforms.draw_call_data.handle(), 2);

                pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                pass.draw_indexed(surface.index.count, 0);
            }
        });
    });
};

auto DualDepthPeeling::peel_pass(const BakedScene& scene) const -> void {
    auto& target = m_peel.flag ? m_peel.target0 : m_peel.target1;

    m_device.render_submit([this, &scene, &target](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = target.target }, [this, &scene](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_peel.pipeline.pipeline.handle());

            pass.bind_uniform_buffer(m_uniforms.scene_data.handle(), 0);
            pass.bind_uniform_buffer(m_uniforms.material_data.handle(), 1);

            for (const auto& surface : scene.transparent) {
                m_uniforms.draw_call_data.upload(siren::ByteBuffer{ surface.material_index });
                pass.bind_uniform_buffer(m_uniforms.draw_call_data.handle(), 2);

                pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
                pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
                pass.draw_indexed(surface.index.count, 0);
            }
        });
    });
}

auto DualDepthPeeling::blend_pass() const -> void {
    return;
}

// ==================== DATA INIT ==============

auto DualDepthPeeling::init_uniforms(siren::Device& device) const -> UniformBuffers {
    return UniformBuffers{
        .scene_data     = device.create_buffer({
                    .label = "UBO: Scene Data",
                    .size  = sizeof(SceneData),
                    .usage = siren::BufferUsage::Static,
        }),
        .material_data  = device.create_buffer({
                 .label = "UBO: Material Data",
                 .size  = sizeof(MaterialData),
                 .usage = siren::BufferUsage::Static,
        }),
        .draw_call_data = device.create_buffer({
                .label = "UBO: Call Data",
                .size  = sizeof(DrawCallData),
                .usage = siren::BufferUsage::Static,
        }),
    };
}

auto DualDepthPeeling::init_geometry_pass(siren::Device& device,
                                          siren::AssetServer& server,
                                          const siren::Window& window) const -> GeometryPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/geometry.sshg");

    Pipeline pipeline{
        .shader   = shader,
        .pipeline = device.create_graphics_pipeline({
                .label             = "Geometry Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = true,
                .depth_test        = true,
                .depth_write       = true,
        }),
    };

    std::vector<siren::Image> images;
    images.emplace_back(create_image(device, window));

    return GeometryPass{
        .pipeline = std::move(pipeline),
        .target   = Target{ std::move(images) },
    };
}

auto DualDepthPeeling::init_init_pass(siren::Device& device, siren::AssetServer& server) const -> InitPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/init.sshg");

    Pipeline pipeline{
        .shader   = shader,
        .pipeline = device.create_graphics_pipeline({
                .label             = "Init Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Blend,
                .blend_function    = siren::BlendFunction::Max,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = true,
                .depth_test        = true,
                .depth_write       = true,
        }),
    };

    return InitPass{ .pipeline = std::move(pipeline) };
}

auto DualDepthPeeling::init_peel_pass(siren::Device& device,
                                      siren::AssetServer& server,
                                      const siren::Window& window) const -> PeelPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/peel.sshg");

    Pipeline pipeline{
        .shader   = shader,
        .pipeline = device.create_graphics_pipeline({
                .label             = "Init Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = true,
                .depth_test        = true,
                .depth_write       = true,
        }),
    };

    auto make_peel_target = [](siren::Device& d, const siren::Window& w) -> Target {
        std::vector<siren::Image> images;
        images.emplace_back(create_image(d, w));
        images.emplace_back(create_image(d, w));
        images.emplace_back(create_image(d, w));
        return Target{ std::move(images) };
    };

    return PeelPass{
        .pipeline = std::move(pipeline),
        .target0  = make_peel_target(device, window),
        .target1  = make_peel_target(device, window),
    };
}

auto DualDepthPeeling::init_blend_pass(siren::Device& device,
                                       siren::AssetServer& server,
                                       const siren::Window& window) const -> BlendPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/blend.sshg");

    Pipeline pipeline{
        .shader   = shader,
        .pipeline = device.create_graphics_pipeline({
                .label             = "Blend Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = true,
                .depth_test        = true,
                .depth_write       = true,
        }),
    };

    return BlendPass{
        .pipeline = std::move(pipeline),
        .target   = Target{{}}, // todo
    };
}

}  // namespace oiter
