#include "a_buffer.hpp"

#include <imgui.h>

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/container/byte_buffer.hpp"
#include "2iREN/graphics/buffer.hpp"
#include "2iREN/graphics/commands.hpp"
#include "2iREN/graphics/graphics_pipeline.hpp"
#include "2iREN/graphics/types.hpp"
#include "2iREN/scene/camera.hpp"

using namespace siren;

namespace oiter {

ABuffer::ABuffer(Device& device, const Extent2u extent, AssetServer& assets) :
    OitMethod(device, assets) {
    create_buffers(extent);
    create_images(extent);
    create_pipelines();
}

auto ABuffer::render(
    siren::CommandBuffer& cmds,
    const Camera& camera,
    const BakedScene& scene
) const -> ImageHandle {
    update_buffers(camera, scene);

    auto draw_scene = [&](RenderCommandEncoder& pass) {
        pass.bind_uniform_buffer(m_scene_buffer->handle(), 0, 0);

        for (u32 i = 0; i < scene.transparent.size(); i++) {
            const auto& surface = scene.transparent[i];

            pass.bind_uniform_buffer_range(
                m_mesh_buffer->handle(),
                1,
                mesh_uniforms_alignment() * (scene.opaque.size() + i),
                sizeof(MeshUniforms)
            );
            pass.bind_vertex_buffer(surface.vertex.buffer.handle(), 0, 0);
            pass.bind_index_buffer(surface.index.buffer.handle(), surface.index.format);
            pass.draw_indexed(surface.index.count, 0);
        }
    };

    // reset the counter each frame
    const auto ssbodata = ByteBuffer::make({0});
    m_ssbo->upload(ssbodata.view());
    // reset the list heads each frame using 0xFFFFFFFF
    m_list_head->clear(std::numeric_limits<u32>::max());

    cmds.render_pass(
        // we don't actually write to any output directly, we just manipulate the list_head and the
        // ssbo
        {.target = {}},
        [&](RenderCommandEncoder& pass) {
            pass.bind_graphics_pipeline(m_gather_pipeline->handle());
            pass.bind_storage_image(m_list_head->handle(), AccessKind::ReadWrite, 0);
            pass.bind_shader_storage_buffer(m_ssbo->handle(), 0);
            draw_scene(pass);
        }
    );

    if (m_config.inspecting == Config::ListHead) {
        return m_list_head->handle();
    }

    cmds.render_pass(
        {
            .target =
                RenderTarget{
                    .colors =
                        {
                            ColorAttachment{
                                .image           = m_output->handle(),
                                .begin_operation = BeginOperation::Clear,
                                .clear_color     = Rgba::ZERO(),
                            },
                        },
                },
        },
        [this](RenderCommandEncoder& pass) {
            pass.bind_graphics_pipeline(m_blend_pipeline->handle());
            pass.bind_image(m_list_head->handle(), AccessKind::ReadWrite, 0);
            pass.bind_storage_buffer(m_ssbo->handle(), 0);
            pass.draw_arrays(0, 3);
        }
    );

    return m_output->handle();
}

auto ABuffer::resize(const Extent2u extent) -> void {
    create_buffers(extent);
    create_images(extent);
}

auto ABuffer::reload_shaders() -> void {
    m_gather_shader   = NullHandle;
    m_gather_pipeline = nullptr;
    m_blend_shader    = NullHandle;
    m_blend_pipeline  = nullptr;
    create_pipelines();
}

auto ABuffer::render_debug_info() -> void {
    auto inspecting = (i32*)(&m_config.inspecting);

    if (ImGui::RadioButton("See Final Output         ", inspecting, 0)) {
        m_config.inspecting = Config::None;
    }
    if (ImGui::RadioButton("Inspect List Head Texture", inspecting, 1)) {
        m_config.inspecting = Config::ListHead;
    }
}

auto ABuffer::create_buffers(const Extent2u extent) -> void {
    const auto max_ssbo_size = m_device.limits().max_shader_storage_block_size;
    const auto desired_size =
        sizeof(u32) + (k_list_length * extent.x * extent.y * sizeof(ABufferNode));

    ASSERT(max_ssbo_size > desired_size);

    m_ssbo = std::make_unique<Buffer>(m_device.make_buffer({
        .label        = "A Buffer SSBO",
        .size         = desired_size,
        .usage        = BufferFlags::from(BufferFlag::Storage),
        .memory_usage = MemoryUsage::CpuAndGpu,
    }));
}

auto ABuffer::create_images(const Extent2u extent) -> void {
    m_list_head = std::make_unique<Image>(m_device.make_image({
        .label        = "A-Buffer List Head Image",
        .format       = ImageFormat::R32UI,
        .extent       = extent.to_extent3(),
        .memory_usage = MemoryUsage::CpuAndGpu,
        .flags        = ImageFlags::from(ImageFlag::ShaderRead),
    }));

    m_output = std::make_unique<Image>(m_device.make_image({
        .label  = "A-Buffer Output Image",
        .format = ImageFormat::RGBA8,
        .extent = extent.to_extent3(),
        .flags  = ImageFlags::from(ImageFlag::RenderAttachment),
    }));
}

auto ABuffer::create_pipelines() -> void {
    // gather pipeline
    {
        m_gather_shader = m_assets.load<ShaderAsset>("oiter://assets/shaders/a_buffer/gather.sshg");
        m_assets.wait_until_loaded(m_gather_shader);
        m_gather_pipeline = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
            .label         = "A-Buffer Gather Pipeline",
            .shader        = m_assets.get_unsafe(m_gather_shader).shader.handle(),
            .layout        = DEFAULT_VERTEX_LAYOUT,
            .colors        = {},
            .depth_stencil = std::nullopt,
            .cull_mode     = CullMode::None,
        }));
    }

    // combine pipeline
    {
        m_blend_shader = m_assets.load<ShaderAsset>("oiter://assets/shaders/a_buffer/blend.sshg");
        m_assets.wait_until_loaded(m_blend_shader);
        m_blend_pipeline = std::make_unique<GraphicsPipeline>(m_device.make_graphics_pipeline({
            .label  = "A-Buffer Blend Pipeline",
            .shader = m_assets.get_unsafe(m_blend_shader).shader.handle(),
            .layout = FULLSCREEN_VERTEX_LAYOUT,
            .colors =
                ColorAttachmentDescriptors{
                    ColorAttachmentDescriptor{
                        .format     = ImageFormat::RGBA8,
                        .alpha_mode = AlphaMode::Opaque,
                    },
                },
            .cull_mode = CullMode::None,
        }));
    }
}
} // namespace oiter
