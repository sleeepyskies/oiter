#include "dual_depth_peeling.hpp"

#include <glad/gl.h>

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
    m_device(device), m_sampler(init_sampler(device)), m_uniforms(init_uniforms(device)),
    m_geometry(init_geometry_pass(device, server, window)), m_init(init_init_pass(device, server)),
    m_peel(init_peel_pass(device, server, window)), m_blend(init_blend_pass(device, server, window)),
    m_final(init_final_pass(device, server, window)) {
    //
}

auto DualDepthPeeling::render(const siren::PerspectiveCamera& camera, const BakedScene& scene) const
        -> const siren::Image& {
    m_peel.flag = true;

    {
        SceneData data;
        data.view_projection = camera.projection_view();
        data.camera_position = camera.position();
        m_uniforms.scene_data.upload(siren::ByteBuffer{ data });
    }
    {
        MaterialData data;
        for (const auto& [index, material] : std::views::enumerate(scene.materials)) {
            data.materials[index] = material;
        }
        m_uniforms.material_data.upload(siren::ByteBuffer{ data });
    }

    geometry_pass(scene);
    init_pass(scene);

    for (const auto _ : siren::range(MAX_PEELS)) {
        peel_pass(scene);
        m_peel.flag = !m_peel.flag;
        blend_pass();
    }
    final_pass();

    return m_final.target.colors[0];
}

// ==================== RENDER PASSES ==============

auto DualDepthPeeling::geometry_pass(const BakedScene& scene) const -> void {
    // just renders all non-transparent aka opaque geometry into an image

    m_device.render_submit([&](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = m_geometry.target.render_target }, [&](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_geometry.pipeline.graphics_pipeline.handle());

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
    m_device.render_submit([this, &scene](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = m_peel.target0.render_target },
                         [this, &scene](siren::RenderPassRecorder& pass) -> void {
                             pass.bind_graphics_pipeline(m_init.pipeline.graphics_pipeline.handle());

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
    m_device.render_submit([this, &scene](siren::RenderCommandRecorder& cmds) -> void {
        const auto& write_target = m_peel.write_target();

        cmds.render_pass({ .target = write_target.render_target },
                         [this, &scene](siren::RenderPassRecorder& pass) -> void {
                             const auto& read_target = m_peel.read_target();

                             pass.bind_graphics_pipeline(m_peel.pipeline.graphics_pipeline.handle());

                             pass.bind_sampled_image(read_target.colors[0].handle(), m_sampler.handle(), 0);  // min max
                             pass.bind_sampled_image(read_target.colors[1].handle(), m_sampler.handle(), 1);  // front
                             pass.bind_sampled_image(read_target.colors[2].handle(), m_sampler.handle(), 2);  // back

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
    m_device.render_submit([this](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = m_blend.target.render_target }, [this](siren::RenderPassRecorder& pass) -> void {
            pass.bind_graphics_pipeline(m_blend.pipeline.graphics_pipeline.handle());
            pass.bind_sampled_image(m_peel.read_target().colors[2].handle(), m_sampler.handle(), 0);
            pass.draw_arrays(0, 3);  // just draw to a fullscreen quad
        });
    });
}

auto DualDepthPeeling::final_pass() const -> void {
    m_device.render_submit([this](siren::RenderCommandRecorder& cmds) -> void {
        cmds.render_pass({ .target = m_final.target.render_target }, [this](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_final.pipeline.graphics_pipeline.handle());

            const auto& target = !m_peel.flag ? m_peel.target0 : m_peel.target1;

            pass.bind_sampled_image(target.colors[0].handle(), m_sampler.handle(), 0);             // min max
            pass.bind_sampled_image(target.colors[1].handle(), m_sampler.handle(), 1);             // front
            pass.bind_sampled_image(target.colors[2].handle(), m_sampler.handle(), 2);             // back
            pass.bind_sampled_image(m_geometry.target.colors[0].handle(), m_sampler.handle(), 3);  // opaque

            pass.draw_arrays(0, 3);
        });
    });
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
                .usage = siren::BufferUsage::Dynamic,
        }),
    };
}

auto DualDepthPeeling::init_sampler(siren::Device& device) const -> siren::Sampler {
    return device.create_sampler({
            .min_filter    = siren::ImageFilterMode::Linear,
            .max_filter    = siren::ImageFilterMode::Nearest,
            .mipmap_filter = siren::ImageFilterMode::Nearest,
            .s_wrap        = siren::ImageWrapMode::ClampEdge,
            .t_wrap        = siren::ImageWrapMode::ClampEdge,
    });
}

auto DualDepthPeeling::init_geometry_pass(siren::Device& device,
                                          siren::AssetServer& server,
                                          const siren::Window& window) const -> GeometryPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/geometry.sshg");

    Pipeline pipeline{
        .shader            = shader,
        .graphics_pipeline = device.create_graphics_pipeline({
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

    siren::ImageDescriptor img_desc;
    auto depth = device.create_image({
            .label         = "Geometry Pass Depth Buffer",
            .format        = siren::ImageFormat::Depth24Stencil8,
            .extent        = siren::ImageExtent{ .width = window.width(), .height = window.height() },
            .dimension     = siren::ImageDimension::D2,
            .mipmap_levels = 1,
    });

    return GeometryPass{
        .pipeline = std::move(pipeline),
        .target =
                Target{
                        siren::RenderTarget{
                            .colors = {
                                siren::ColorAttachment{images[0].handle(), siren::BeginOperation::Clear, siren::Rgba::gray()},
                            },
                            .depth_stencil = siren::DepthStencilAttachment{depth.handle(), siren::BeginOperation::Clear, 1.f, 0},
                        },
                        std::move(images),
                        std::move(depth),
                },
    };
}

auto DualDepthPeeling::init_init_pass(siren::Device& device, siren::AssetServer& server) const -> InitPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/init.sshg");

    Pipeline pipeline{
        .shader            = shader,
        .graphics_pipeline = device.create_graphics_pipeline({
                .label               = "Init Pipeline",
                .layout              = siren::DEFAULT_VERTEX_LAYOUT,
                .shader              = server.get_unsafe(shader).shader.handle(),
                .topology            = siren::PrimitiveTopology::Triangles,
                .alpha_mode          = siren::AlphaMode::Blend,
                .blend_function      = siren::BlendFunction::Max,
                .depth_function      = siren::DepthFunction::Less,
                .source_blend_factor = siren::BlendFactor::One,
                .dest_blend_factor   = siren::BlendFactor::One,
                .back_face_culling   = false,  // todo: should we make this true?
                .depth_test          = false,
                .depth_write         = false,
        }),
    };

    return InitPass{ .pipeline = std::move(pipeline) };
}

auto DualDepthPeeling::init_peel_pass(siren::Device& device,
                                      siren::AssetServer& server,
                                      const siren::Window& window) const -> PeelPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/peel.sshg");

    Pipeline pipeline{
        .shader            = shader,
        .graphics_pipeline = device.create_graphics_pipeline({
                .label             = "Peel Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Blend,
                .blend_function    = siren::BlendFunction::Add,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = false,  // todo: this should be enabled
                .depth_test        = false,
                .depth_write       = false,
        }),
    };

    auto make_peel_target = [](siren::Device& d, const siren::Window& w) -> Target {
        std::vector<siren::Image> images;

        // min max image
        images.emplace_back(d.create_image({
                .format        = siren::ImageFormat::RG32f,
                .extent        = siren::ImageExtent{ .width = w.width(), .height = w.height() },
                .dimension     = siren::ImageDimension::D2,
                .mipmap_levels = 1,
        }));

        // front color
        images.emplace_back(create_image(d, w));

        // back color
        images.emplace_back(create_image(d, w));

        return Target{
            siren::RenderTarget{
                .colors = {
                    { siren::ColorAttachment{images[0].handle(), siren::BeginOperation::Clear, siren::Rgba::black()} },
                    { siren::ColorAttachment{images[1].handle(), siren::BeginOperation::Clear, siren::Rgba::black()} },
                    { siren::ColorAttachment{images[2].handle(), siren::BeginOperation::Clear, siren::Rgba::black()} },
                },
                .depth_stencil = std::nullopt,
            },
            std::move(images),
        };
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
        .shader            = shader,
        .graphics_pipeline = device.create_graphics_pipeline({
                .label             = "Blend Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Blend,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = false,
                .depth_test        = false,
                .depth_write       = false,
        }),
    };

    std::vector<siren::Image> images{};
    images.emplace_back(create_image(device, window));

    // todo: just temp values to compile, idk how right this is
    return BlendPass{
        .pipeline = std::move(pipeline),
        .target   = Target{
            siren::RenderTarget{
                .colors = {
                    siren::ColorAttachment{
                        .image = images[0].handle(),
                        .begin_operation = siren::BeginOperation::Clear,
                        .clear_color = siren::Rgba::black(),
                    },
                },
                .depth_stencil = std::nullopt,
            },
            std::move(images),
        },
    };
}

auto DualDepthPeeling::init_final_pass(siren::Device& device,
                                       siren::AssetServer& server,
                                       const siren::Window& window) const -> FinalPass {
    const auto shader = server.load<siren::ShaderAsset>("oiter://assets/shaders/dual_depth_peeling/final.sshg");

    Pipeline pipeline{
        .shader            = shader,
        .graphics_pipeline = device.create_graphics_pipeline({
                .label             = "Blend Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = server.get_unsafe(shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .depth_function    = siren::DepthFunction::Less,
                .back_face_culling = false,
                .depth_test        = false,
                .depth_write       = false,
        }),
    };

    std::vector<siren::Image> images;
    images.emplace_back(create_image(device, window));

    return FinalPass{
        .pipeline = std::move(pipeline),
        .target =
                Target{
                        siren::RenderTarget{
                                .colors        = {
                                    siren::ColorAttachment{
                                               .image           = images[0].handle(),
                                               .begin_operation = siren::BeginOperation::Clear,
                                               .clear_color     = siren::Rgba::black(),
                                    },
                                },
                                .depth_stencil = std::nullopt,
                        },
                        std::move(images),
                },
    };
}

}  // namespace oiter
