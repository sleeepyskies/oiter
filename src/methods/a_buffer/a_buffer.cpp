#include "a_buffer.hpp"
#include <imgui.h>
#include <utility>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/scene/camera.hpp"

namespace oiter {
ABuffer::ABuffer(siren::Device& device, const siren::Extent2u extent, siren::AssetServer& assets) :
    OitMethod(device, assets) {
    create_buffers(extent);
    create_images(extent);
    create_pipelines();
}

auto ABuffer::render(const siren::Camera& camera, const BakedScene& scene) const
    -> const siren::Image& {
    update_buffers(camera, scene);

    auto draw_scene = [&](siren::RenderPassRecorder& pass) {
        pass.bind_uniform_buffer(m_scene_buffer->handle(), 0);
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

    // reset the counter each frame
    m_ssbo->upload(siren::ByteBuffer{siren::u32{0}}.data());
    // reset the list heads each frame using 0xFFFFFFFF
    m_list_head->clear(std::numeric_limits<siren::u32>::max());

    m_device.render_pass(
        // we don't actually write to any output directly, we just manipulate the list_head and the
        // ssbo
        {.target = {}},
        [&](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_gather_pipeline->handle());
            pass.bind_storage_image(m_list_head->handle(), siren::AccessKind::ReadWrite, 0);
            pass.bind_shader_storage_buffer(m_ssbo->handle(), 0);
            draw_scene(pass);
        }
    );

    if (m_config.inspecting == Config::ListHead) {
        return *m_list_head;
    }

    m_device.render_pass(
        {
            .target =
                siren::RenderTarget{
                    .colors =
                        {
                            siren::ColorAttachment{
                                .image           = m_output->handle(),
                                .begin_operation = siren::BeginOperation::Clear,
                                .clear_color     = siren::Rgba::ZERO(),
                            },
                        },
                    .depth_stencil = std::nullopt,
                },
        },
        [this](siren::RenderPassRecorder& pass) {
            pass.bind_graphics_pipeline(m_blend_pipeline->handle());
            pass.bind_storage_image(m_list_head->handle(), siren::AccessKind::ReadWrite, 0);
            pass.bind_shader_storage_buffer(m_ssbo->handle(), 0);
            pass.draw_fullscreen();
        }
    );

    return *m_output;
}

auto ABuffer::resize(const siren::Extent2u extent) -> void {
    create_buffers(extent);
    create_images(extent);
}

auto ABuffer::reload_shaders() -> void {
    m_gather_shader   = siren::NullHandle;
    m_gather_pipeline = nullptr;
    m_blend_shader    = siren::NullHandle;
    m_blend_pipeline  = nullptr;
    create_pipelines();
}

auto ABuffer::render_debug_info() -> void {
    auto inspecting = (siren::i32*)(&m_config.inspecting);

    if (ImGui::RadioButton("See Final Output         ", inspecting, 0)) {
        m_config.inspecting = Config::None;
    }
    if (ImGui::RadioButton("Inspect List Head Texture", inspecting, 1)) {
        m_config.inspecting = Config::ListHead;
    }
}

auto ABuffer::create_buffers(const siren::Extent2u extent) -> void {
    const auto max_ssbo_size = m_device.limits().max_shader_storage_block_size;
    const auto desired_size =
        sizeof(siren::u32) + (k_list_length * extent.x * extent.y * sizeof(ABufferNode));

    ASSERT(max_ssbo_size > desired_size, "requested to make ssbo with size {}, max size allowed: {}", desired_size, max_ssbo_size);

    m_ssbo = std::make_unique<siren::Buffer>(m_device.create_buffer({
        .label = "A Buffer SSBO",
        // just 0 init the counter
        .data = std::nullopt,
        // we need to be able to store k_list_length * image_size * node_size elements PLUS a u32
        // for the counter
        .size  = desired_size,
        .usage = siren::BufferUsage::Dynamic,
    }));
}

auto ABuffer::create_images(const siren::Extent2u extent) -> void {
    m_list_head =
        create_standard_image(extent, "A-Buffer List Head Image", siren::ImageFormat::R32UI);
    m_output = create_standard_image(extent, "A-Buffer Output Image", siren::ImageFormat::RGBA8);
}

auto ABuffer::create_pipelines() -> void {
    // gather pipeline
    {
        m_gather_shader =
            m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/a_buffer/gather.sshg");
        m_assets.wait_until_loaded(m_gather_shader);
        m_gather_pipeline =
            std::make_unique<siren::GraphicsPipeline>(m_device.create_graphics_pipeline({
                .label             = "A-Buffer Gather Pipeline",
                .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                .shader            = m_assets.get_unsafe(m_gather_shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .back_face_culling = false,
                .depth_test        = false,
                .depth_write       = false,
            }));
    }

    // combine pipeline
    {
        m_blend_shader =
            m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/a_buffer/blend.sshg");
        m_assets.wait_until_loaded(m_blend_shader);
        m_blend_pipeline =
            std::make_unique<siren::GraphicsPipeline>(m_device.create_graphics_pipeline({
                .label             = "A-Buffer Blend Pipeline",
                .layout            = siren::FULLSCREEN_VERTEX_LAYOUT,
                .shader            = m_assets.get_unsafe(m_blend_shader).shader.handle(),
                .topology          = siren::PrimitiveTopology::Triangles,
                .alpha_mode        = siren::AlphaMode::Opaque,
                .back_face_culling = false,
                .depth_test        = false,
                .depth_write       = false,
            }));
    }
}
} // namespace oiter
