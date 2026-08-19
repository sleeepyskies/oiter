#include "a_buffer.hpp"

#include "2iren/asset/asset_server.hpp"

namespace oiter {
ABuffer::ABuffer(
    siren::Device& device,
    const glm::uvec2 extent,
    siren::AssetServer& assets
) : OitMethod(device, assets) {
    // todo: do we need to check gpu requirements?

    create_buffers(extent);
    create_images(extent);
    create_pipelines();
}

auto ABuffer::render(
    const siren::PerspectiveCamera& camera,
    const BakedScene& scene
) const -> const siren::Image& {
    update_buffers(camera, scene);

    UNIMPLEMENTED();

    return *m_output;
}

auto ABuffer::resize(const glm::uvec2 extent) -> void {
    create_buffers(extent);
    create_images(extent);
}

auto ABuffer::reload_shaders() -> void {
    UNIMPLEMENTED();
}

auto ABuffer::render_debug_info() -> void {
    UNIMPLEMENTED();
}

auto ABuffer::create_buffers(const glm::uvec2 extent) -> void {
    m_ssbo = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "A Buffer SSBO",
                .data  = std::nullopt,
                // we need to be able to store k_list_length * image_size * node_size elements
                .size  = k_list_length * extent.x * extent.y * sizeof(ABufferNode),
                .usage = siren::BufferUsage::Dynamic,
            }
        )
    );
}

auto ABuffer::create_images(const glm::uvec2 extent) -> void {
    m_list_head = create_standard_image(extent, "A-Buffer List Head Image", siren::ImageFormat::R32UI);
    m_output    = create_standard_image(extent, "A-Buffer Output Image", siren::ImageFormat::RGBA8);
}

auto ABuffer::create_pipelines() -> void {
    // gather pipeline
    {
        m_gather_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/a_buffer/gather.sshg");
        m_assets.wait_until_loaded(m_gather_shader);
        m_gather_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "A-Buffer Gather Pipeline",
                    .layout            = siren::DEFAULT_VERTEX_LAYOUT,
                    .shader            = m_assets.get_unsafe(m_gather_shader).shader.handle(),
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }

    // combine pipeline
    {
        m_combine_shader = m_assets.load<siren::ShaderAsset>("oiter://assets/shaders/a_buffer/combine.sshg");
        m_assets.wait_until_loaded(m_combine_shader);
        m_combine_pipeline = std::make_unique<siren::GraphicsPipeline>(
            m_device.create_graphics_pipeline(
                {
                    .label             = "A-Buffer Combine Pipeline",
                    .layout            = siren::FULLSCREEN_VERTEX_LAYOUT,
                    .shader            = m_assets.get_unsafe(m_combine_shader).shader.handle(),
                    .topology          = siren::PrimitiveTopology::Triangles,
                    .alpha_mode        = siren::AlphaMode::Opaque,
                    .back_face_culling = false,
                    .depth_test        = false,
                    .depth_write       = false,
                }
            )
        );
    }
}
} // namespace oiter
