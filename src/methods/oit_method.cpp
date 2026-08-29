#include "oit_method.hpp"

#include "2iREN/asset/asset_server.hpp"
#include "2iREN/rhi/device.hpp"

namespace oiter {
OitMethod::OitMethod(siren::Device& device, siren::AssetServer& assets)
    : m_device(device),
      m_assets(assets) {
    create_buffers();
}

auto OitMethod::update_buffers(const siren::PerspectiveCamera& camera, const BakedScene& scene) const -> void {
    m_scene_buffer->upload(
        SceneUniforms{
            .projection_view = camera.projection_view(),
            .camera_position = camera.position()
        }
    );

    if (!m_scene_updated) { return; }
    m_scene_updated = false;
    ASSERT(scene.opaque.size() + scene.transparent.size() <= MAX_MESHES);

    const auto alignment = siren::align_up(sizeof(MeshUniforms), m_device.limits().uniform_buffer_offset_alignment);

    siren::ByteBuffer buffer;

    const auto append_meshes = [&](const auto& surfaces) {
        for (const auto& surface : surfaces) {
            buffer.append(
                MeshUniforms{
                    .material = scene.materials[surface.material_index],
                    .model    = surface.transform,
                },
                alignment
            );
        }
    };

    append_meshes(scene.opaque);
    append_meshes(scene.transparent);
    m_mesh_buffer->upload(buffer);
}


auto OitMethod::create_standard_image(
    const glm::uvec2 extent,
    const std::string& label,
    const siren::ImageFormat image_format
) const -> std::unique_ptr<siren::Image> {
    return std::make_unique<siren::Image>(
        m_device.create_image(
            {
                .label         = label,
                .format        = image_format,
                .extent        = {.width = extent.x, .height = extent.y, .depth_or_layers = 1},
                .dimension     = siren::ImageDimension::D2,
                .mipmap_levels = 1,
            }
        )
    );
}

auto OitMethod::create_buffers() -> void {
    m_scene_buffer = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "scene uniforms",
                .data  = std::nullopt,
                .size  = sizeof(SceneUniforms),
                .usage = siren::BufferUsage::Static,
            }
        )
    );

    m_mesh_buffer = std::make_unique<siren::Buffer>(
        m_device.create_buffer(
            {
                .label = "mesh uniforms",
                .data  = std::nullopt,
                .size  = siren::align_up(
                    sizeof(MeshUniforms),
                    m_device.limits().uniform_buffer_offset_alignment
                ) * MAX_MESHES,
                .usage = siren::BufferUsage::Static,
            }
        )
    );
}
} // namespace oiter
